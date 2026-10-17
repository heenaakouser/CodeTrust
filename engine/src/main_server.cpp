#include "engine.hpp"
#include "generator.hpp"
#include "language.hpp"
#include "llm.hpp"
#include "report.hpp"
#include "sandbox.hpp"
#include "testcase.hpp"
#include "types.hpp"

#include "httplib.h"
#include "json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

// Run source on LLM-proposed inputs and use program stdout as expected.
// Keeps system-generated solutions in sync with generated tests.
std::string oracleExpectedsFromSource(codetrust::Engine& engine,
                                      const std::string& language,
                                      const std::string& source,
                                      const std::string& testcasesText) {
  auto cases = codetrust::parseTestcases(testcasesText);
  if (cases.empty() || source.empty()) return testcasesText;

  std::ostringstream probe;
  for (const auto& tc : cases) {
    if (!tc.name.empty()) probe << "# " << tc.name << "\n";
    probe << tc.input << "|\n";
  }

  codetrust::RunRequest req;
  req.source = source;
  req.language = codetrust::parseLanguage(language);
  req.testcasesText = probe.str();
  req.timeoutSec = 2;
  req.memoryMb = 256;
  req.label = "oracle";
  req.mode = codetrust::RunMode::Full;

  auto report = engine.execute(req);
  if (!report.compileOk || report.tests.empty()) return testcasesText;

  std::ostringstream out;
  int kept = 0;
  for (const auto& tr : report.tests) {
    if (tr.status == codetrust::TestStatus::Timeout ||
        tr.status == codetrust::TestStatus::RuntimeError) {
      continue;
    }
    if (!tr.name.empty()) out << "# " << tr.name << "\n";
    std::string exp = tr.actual;
    for (char& c : exp) {
      if (c == '|') c = ' ';
      if (c == '\n' || c == '\r') c = ' ';
    }
    while (!exp.empty() && (exp.back() == ' ' || exp.back() == '\t')) {
      exp.pop_back();
    }
    out << tr.input << "|" << exp << "\n";
    ++kept;
  }
  return kept > 0 ? out.str() : testcasesText;
}

std::string readFile(const fs::path& path) {
  std::ifstream in(path);
  if (!in) return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

json sampleMeta(const fs::path& dir) {
  json j;
  j["id"] = dir.filename().string();
  const auto metaPath = dir / "meta.json";
  if (fs::exists(metaPath)) {
    try {
      j = json::parse(readFile(metaPath));
      j["id"] = dir.filename().string();
    } catch (...) {
      j["title"] = dir.filename().string();
    }
  } else {
    j["title"] = dir.filename().string();
    j["description"] = "";
  }
  if (!j.contains("generator")) {
    j["generator"] = codetrust::inferGeneratorKind(
        j.value("id", ""), j.value("title", ""), j.value("description", ""));
  }
  return j;
}

json sampleFull(const fs::path& dir, codetrust::Language lang) {
  auto j = sampleMeta(dir);
  const std::string kind = j.value("generator", dir.filename().string());
  const auto info = codetrust::languageInfo(lang);

  // Prefer on-disk template if present, else built-in starter for that language.
  const auto tplPath = dir / "templates" / info.sourceFile;
  const auto legacyCpp = dir / "submission.cpp";
  if (fs::exists(tplPath)) {
    j["source"] = readFile(tplPath);
  } else if (lang == codetrust::Language::Cpp && fs::exists(legacyCpp)) {
    j["source"] = readFile(legacyCpp);
  } else {
    j["source"] = codetrust::defaultTemplate(lang, kind);
  }

  j["language"] = info.key;
  j["sourceFile"] = info.sourceFile;
  j["testcases"] = readFile(dir / "testcases.txt");
  return j;
}

std::string projectRootFromArgs(int argc, char** argv) {
  std::string root = fs::current_path().string();
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if ((arg == "--root" || arg == "-r") && i + 1 < argc) {
      root = argv[++i];
    } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
      ++i;
    }
  }
  return fs::weakly_canonical(root).string();
}

int portFromArgs(int argc, char** argv) {
  int port = 8080;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    }
  }
  return port;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string root = projectRootFromArgs(argc, argv);
  const int port = portFromArgs(argc, argv);

  codetrust::Engine engine(root);
  const auto webDist = fs::path(root) / "web" / "dist";
  const auto samplesDir = fs::path(engine.samplesDir());

  // IMPORTANT: httplib stores the mount path as const char* — keep it alive.
  const std::string webDistStr = webDist.string();

  httplib::Server svr;
  svr.new_task_queue = [] { return new httplib::ThreadPool(4); };

  svr.set_default_headers({
      {"Access-Control-Allow-Origin", "*"},
      {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
      {"Access-Control-Allow-Headers", "Content-Type"},
  });

  svr.Options(R"(/api/.*)",
              [](const httplib::Request&, httplib::Response& res) {
                res.status = 204;
              });

  svr.Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
    const auto llm = codetrust::loadLlmConfig();
    // Keep vendor details off the public health payload (college demo / production UI).
    json j{
        {"ok", true},
        {"service", "CodeTrust"},
        {"version", "2.0.0"},
        {"docker", codetrust::dockerAvailable()},
        {"llm", {{"configured", llm.configured}}},
    };
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/api/languages", [&](const httplib::Request&, httplib::Response& res) {
    json arr = json::array();
    for (const auto& lang : codetrust::allLanguages()) {
      arr.push_back({
          {"id", lang.key},
          {"name", lang.displayName},
          {"monaco", lang.monacoId},
          {"sourceFile", lang.sourceFile},
          {"dockerImage", lang.dockerImage},
          {"compiled", lang.compiled},
      });
    }
    res.set_content(arr.dump(2), "application/json");
  });

  svr.Get("/api/samples", [&](const httplib::Request&, httplib::Response& res) {
    json arr = json::array();
    std::error_code ec;
    if (fs::exists(samplesDir, ec)) {
      for (const auto& entry : fs::directory_iterator(samplesDir, ec)) {
        if (entry.is_directory()) {
          arr.push_back(sampleMeta(entry.path()));
        }
      }
    }
    res.set_content(arr.dump(2), "application/json");
  });

  svr.Get(R"(/api/samples/([a-zA-Z0-9_-]+))",
          [&](const httplib::Request& req, httplib::Response& res) {
            const auto id = req.matches[1].str();
            const auto dir = samplesDir / id;
            if (!fs::exists(dir) || !fs::is_directory(dir)) {
              res.status = 404;
              res.set_content(R"({"error":"sample not found"})",
                              "application/json");
              return;
            }
            auto lang = codetrust::Language::Cpp;
            if (req.has_param("lang")) {
              lang = codetrust::parseLanguage(req.get_param_value("lang"));
            }
            res.set_content(sampleFull(dir, lang).dump(2), "application/json");
          });

  // Generate tests from editor source via free LLM (Groq/Gemini).
  svr.Post("/api/generate-tests",
           [&](const httplib::Request& req, httplib::Response& res) {
             json body;
             try {
               body = json::parse(req.body.empty() ? "{}" : req.body);
             } catch (...) {
               res.status = 400;
               res.set_content(R"({"error":"invalid JSON body"})",
                               "application/json");
               return;
             }

             codetrust::DynamicGenerateRequest genReq;
             genReq.language = body.value("language", "cpp");
             genReq.source = body.value("source", "");
             genReq.specification = body.value("specification", "");
             if (genReq.specification.empty()) {
               genReq.specification = body.value("description", "");
             }
             genReq.count = body.value("count", 10);
             genReq.allowLegacyFallback = body.value("allowLegacyFallback", false);
             genReq.legacyKind = body.value("generator", "");
             // oracleFromSource: fill expecteds by running the editor source
             // (used when the solution was system-generated).
             const bool oracle =
                 body.value("oracleFromSource", false) ||
                 body.value("useSourceAsOracle", false);
             genReq.inputsOnly = oracle;

             auto out = codetrust::generateTestsDynamic(genReq);
             if (!out.ok) {
               res.status = 400;
               res.set_content(json({{"error", out.error}}).dump(2),
                               "application/json");
               return;
             }

             std::string cases = out.testcases;
             bool oracled = false;
             if (oracle) {
               cases = oracleExpectedsFromSource(engine, genReq.language,
                                                 genReq.source, cases);
               oracled = true;
             }

             res.set_content(
                 json({{"testcases", cases},
                       {"count", genReq.count},
                       {"oracled", oracled}})
                     .dump(2),
                 "application/json");
           });

  svr.Post("/api/sync",
           [&](const httplib::Request& req, httplib::Response& res) {
             json body;
             try {
               body = json::parse(req.body.empty() ? "{}" : req.body);
             } catch (...) {
               res.status = 400;
               res.set_content(R"({"error":"invalid JSON body"})",
                               "application/json");
               return;
             }

             const std::string direction = body.value("direction", "");
             const std::string language = body.value("language", "cpp");
             const std::string source = body.value("source", "");
             const std::string specification = body.value("specification", "");

             codetrust::LlmSyncResult sync;
             if (direction == "spec-to-code") {
               sync = codetrust::llmSourceFromSpec(language, specification, source);
             } else if (direction == "code-to-spec") {
               sync = codetrust::llmSpecFromSource(language, source, specification);
             } else {
               res.status = 400;
               res.set_content(
                   R"({"error":"direction must be spec-to-code or code-to-spec"})",
                   "application/json");
               return;
             }

             if (!sync.ok) {
               res.status = 400;
               res.set_content(json({{"error", sync.error}}).dump(2),
                               "application/json");
               return;
             }

             res.set_content(
                 json({{"source", sync.source},
                       {"specification", sync.specification}})
                     .dump(2),
                 "application/json");
           });

  // Suggest a corrected program when verification fails.
  svr.Post("/api/fix-code",
           [&](const httplib::Request& req, httplib::Response& res) {
             json body;
             try {
               body = json::parse(req.body.empty() ? "{}" : req.body);
             } catch (...) {
               res.status = 400;
               res.set_content(R"({"error":"invalid JSON body"})",
                               "application/json");
               return;
             }

             auto fix = codetrust::llmFixCode(
                 body.value("language", "cpp"), body.value("source", ""),
                 body.value("specification", ""),
                 body.value("compileLog", ""),
                 body.value("failureContext", ""));

             if (!fix.ok) {
               res.status = 400;
               res.set_content(json({{"error", fix.error}}).dump(2),
                               "application/json");
               return;
             }

             res.set_content(
                 json({{"fixedSource", fix.fixedSource},
                       {"explanation", fix.explanation}})
                     .dump(2),
                 "application/json");
           });

  svr.Get("/api/runs", [&](const httplib::Request& req, httplib::Response& res) {
    int limit = 50;
    if (req.has_param("limit")) {
      try {
        limit = std::stoi(req.get_param_value("limit"));
      } catch (...) {
      }
    }
    auto reports = codetrust::listReports(engine.runsDir(), limit);
    json arr = json::array();
    for (const auto& r : reports) {
      arr.push_back(json::parse(codetrust::reportToJson(r, false)));
    }
    res.set_content(arr.dump(2), "application/json");
  });

  svr.Get(R"(/api/runs/([A-Za-z0-9_-]+))",
          [&](const httplib::Request& req, httplib::Response& res) {
            const auto id = req.matches[1].str();
            const auto path = fs::path(engine.runsDir()) / (id + ".json");
            if (!fs::exists(path)) {
              res.status = 404;
              res.set_content(R"({"error":"run not found"})",
                              "application/json");
              return;
            }
            auto report = codetrust::loadReport(path.string());
            res.set_content(codetrust::reportToJson(report, true),
                            "application/json");
          });

  svr.Post("/api/runs", [&](const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
      body = json::parse(req.body.empty() ? "{}" : req.body);
    } catch (...) {
      res.status = 400;
      res.set_content(R"({"error":"invalid JSON body"})", "application/json");
      return;
    }

    codetrust::RunRequest runReq;
    runReq.source = body.value("source", "");
    runReq.testcasesText = body.value("testcases", "");
    runReq.language = codetrust::parseLanguage(body.value("language", "cpp"));
    runReq.mode = codetrust::parseMode(body.value("mode", "full"));
    runReq.timeoutSec = body.value("timeoutSec", 2);
    runReq.memoryMb = body.value("memoryMb", 128);
    runReq.label = body.value("label", "");

    // If client omitted tests, generate from source via LLM.
    if (runReq.testcasesText.empty() && runReq.mode == codetrust::RunMode::Full) {
      codetrust::DynamicGenerateRequest genReq;
      genReq.language = toString(runReq.language);
      genReq.source = runReq.source;
      genReq.specification = body.value("specification", "");
      if (genReq.specification.empty()) {
        genReq.specification = body.value("description", "");
      }
      genReq.count = 10;
      auto gen = codetrust::generateTestsDynamic(genReq);
      if (!gen.ok) {
        res.status = 400;
        res.set_content(
            json({{"error", "No test cases and LLM generation failed: " + gen.error}})
                .dump(2),
            "application/json");
        return;
      }
      runReq.testcasesText = gen.testcases;
    }

    auto report = engine.execute(runReq);
    res.status = 201;
    res.set_content(codetrust::reportToJson(report, true), "application/json");
  });

  if (fs::exists(webDist)) {
    if (!svr.set_mount_point("/", webDistStr.c_str())) {
      std::cerr << "Warning: failed to mount UI from " << webDistStr << "\n";
    }
    svr.set_error_handler(
        [&](const httplib::Request& req, httplib::Response& res) {
          if (res.status == 404 && req.path.rfind("/api/", 0) != 0) {
            const auto index = webDist / "index.html";
            if (fs::exists(index)) {
              res.status = 200;
              res.set_content(readFile(index), "text/html");
            }
          }
        });
  } else {
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
      res.set_content(
          "CodeTrust API is running. Build the UI with `make build` and restart.",
          "text/plain");
    });
  }

  std::cout << "CodeTrust listening on http://127.0.0.1:" << port << std::endl;
  std::cout << "Project root: " << root << std::endl;
  if (!fs::exists(webDist)) {
    std::cout << "Note: web/dist missing — API only until `make build`."
              << std::endl;
  }

  // Bind localhost first (more reliable on some macOS setups than 0.0.0.0).
  if (!svr.listen("127.0.0.1", port)) {
    std::cerr << "Failed to bind port " << port
              << ". Is another CodeTrust already running?\n";
    return 1;
  }
  return 0;
}
