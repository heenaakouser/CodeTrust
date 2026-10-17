# CodeTrust

**Multi-language Code Verification and Automated Testing Platform**

CodeTrust compiles/runs submissions in Docker for **C++, Python, Java,
JavaScript, and Go**, executes auto-generated I/O test cases, classifies
outcomes (pass, wrong output, runtime error, timeout, compile error), and
produces structured JSON + text reports.

> This platform automates testing. It does **not** prove that a program is
> correct for every possible input.

## Features

- **5 languages:** C++, Python, Java, JavaScript, Go (each in its own Docker image)
- Docker-sandboxed build/run (memory/CPU/pids limits, no network)
- **Auto test-case generation** from the problem — **no LLM** (deterministic C++ generators)
- Modes: **full** run or **compile / syntax only**
- Configurable per-test timeout and memory limit
- Outcome classification + actionable hints (does not auto-fix your code)
- JSON run history under `data/runs/`
- Judge-style UI (problem panel + Monaco + testcase/result console)
- C++ HTTP API (cpp-httplib)
- Makefiles for one-command local runs

## How test generation works (not an LLM)

**No LLM API.** Generators live in `engine/src/generator.cpp`:

1. Problem type (`add` / `factorial` / `palindrome`) selects a generator
2. It creates edge + random **inputs**
3. It computes **expected outputs** with the known correct formula
4. Same I/O tests apply to every language

## Dry run (what happens when you click Run)

1. UI sends source + **language** + tests to `POST /api/runs`.
2. Engine writes a job folder under `data/jobs/`.
3. Docker builds/syntax-checks with that language’s image.
4. If build fails → report stops with log + suggestion.
5. For each `input|expected` line: run with stdin = input (timeout/memory caps).
6. Compare stdout → PASS / FAIL_OUTPUT / RUNTIME_ERROR / TIMEOUT.
7. Save report in `data/runs/` and show results + hints in the console.


## Prerequisites

- Docker Desktop (running)
- A C++17 compiler (`g++` or Apple Clang)
- Node.js 20+ (for the UI)
- `make`

## Quick start

```bash
# 1) Free LLM key (Groq) — required for Generate tests
cp .env.example .env
# Edit .env and set CODETRUST_LLM_API_KEY from https://console.groq.com/keys

# 2) Pull language Docker images + install UI deps (first time)
make deps

# 3) Build + start
make run
```

Open [http://127.0.0.1:8080](http://127.0.0.1:8080).

**App flow:** write any code in the editor → **Generate tests** (LLM) → **Run** (Docker).

### Other targets

| Command       | What it does                                      |
|---------------|---------------------------------------------------|
| `make build`  | Build CLI, server, and production UI              |
| `make run`    | Build then start API+UI on port 8080              |
| `make serve`  | Start server only (no rebuild)                    |
| `make demo`   | Run the `add` sample through the CLI              |
| `make test`   | Self-tests (pass / compile error / wrong / timeout) |
| `make clean`  | Remove build artifacts                            |

### CLI

```bash
./build/codetrust-cli \
  --source samples/add/submission.cpp \
  --tests samples/add/testcases.txt \
  --timeout 2 \
  --memory 128

# Backward-compatible shell wrapper
sh run_codetrust.sh
```

### Dev UI (hot reload)

```bash
make cli server
./build/codetrust-server --root . --port 8080   # terminal 1
cd web && npm run dev                           # terminal 2 → :5173
```

## Project layout

```
engine/           C++ test engine, CLI, HTTP server
third_party/      httplib.h, json.hpp
web/              Vite React TypeScript UI
samples/          Sample problems (add, factorial, palindrome)
tests/fixtures/   Engine self-test sources
data/runs/        Persisted JSON/text reports
archive/          Early prototype files
```

## Test case format

```
# optional name
input text|expected output
2 3|5
```

Blank lines and `#` comments are ignored. Comparison trims trailing
whitespace/newlines.

## HTTP API

| Method | Path                 | Description              |
|--------|----------------------|--------------------------|
| GET    | `/api/health`        | Service + Docker status  |
| GET    | `/api/samples`       | List sample problems     |
| GET    | `/api/samples/:id`   | Sample source + tests    |
| POST   | `/api/runs`          | Create a verification run|
| GET    | `/api/runs`          | Recent run history       |
| GET    | `/api/runs/:id`      | One report               |

`POST /api/runs` body:

```json
{
  "source": "#include <iostream>...",
  "testcases": "2 3|5\n",
  "mode": "full",
  "timeoutSec": 2,
  "memoryMb": 128,
  "label": "workspace"
}
```

## Limitations

- I/O testing only — not formal verification
- Requires a local Docker daemon
- Not hardened as a multi-tenant public code-execution service
