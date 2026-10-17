#include "language.hpp"

namespace codetrust {

Language parseLanguage(const std::string& s) {
  if (s == "python" || s == "py" || s == "python3") return Language::Python;
  if (s == "java") return Language::Java;
  if (s == "javascript" || s == "js" || s == "node") return Language::JavaScript;
  if (s == "go" || s == "golang") return Language::Go;
  if (s == "c++" || s == "cpp" || s == "cxx") return Language::Cpp;
  return Language::Cpp;
}

std::string toString(Language lang) {
  switch (lang) {
    case Language::Python: return "python";
    case Language::Java: return "java";
    case Language::JavaScript: return "javascript";
    case Language::Go: return "go";
    case Language::Cpp: return "cpp";
  }
  return "cpp";
}

LanguageInfo languageInfo(Language lang) {
  switch (lang) {
    case Language::Python:
      return {lang, "python", "Python", "python", "submission.py",
              "python:3.12-slim", false};
    case Language::Java:
      return {lang, "java", "Java", "java", "Main.java",
              "eclipse-temurin:17-jdk", true};
    case Language::JavaScript:
      return {lang, "javascript", "JavaScript", "javascript", "submission.js",
              "node:20-slim", false};
    case Language::Go:
      return {lang, "go", "Go", "go", "submission.go", "golang:1.22-alpine",
              true};
    case Language::Cpp:
    default:
      return {lang, "cpp", "C++", "cpp", "submission.cpp", "gcc:14", true};
  }
}

std::vector<LanguageInfo> allLanguages() {
  return {
      languageInfo(Language::Cpp),
      languageInfo(Language::Python),
      languageInfo(Language::Java),
      languageInfo(Language::JavaScript),
      languageInfo(Language::Go),
  };
}

namespace {

std::string tplAdd(Language lang) {
  switch (lang) {
    case Language::Python:
      return "a, b = map(int, input().split())\nprint(a + b)\n";
    case Language::Java:
      return "import java.util.*;\n"
             "public class Main {\n"
             "  public static void main(String[] args) {\n"
             "    Scanner sc = new Scanner(System.in);\n"
             "    int a = sc.nextInt();\n"
             "    int b = sc.nextInt();\n"
             "    System.out.println(a + b);\n"
             "  }\n"
             "}\n";
    case Language::JavaScript:
      return "const fs = require('fs');\n"
             "const parts = fs.readFileSync(0, 'utf8').trim().split(/\\s+/).map(Number);\n"
             "console.log(parts[0] + parts[1]);\n";
    case Language::Go:
      return "package main\n\n"
             "import \"fmt\"\n\n"
             "func main() {\n"
             "\tvar a, b int\n"
             "\tfmt.Scan(&a, &b)\n"
             "\tfmt.Println(a + b)\n"
             "}\n";
    case Language::Cpp:
    default:
      return "#include <iostream>\nusing namespace std;\n\n"
             "int main() {\n"
             "    int a, b;\n"
             "    cin >> a >> b;\n"
             "    cout << a + b << endl;\n"
             "    return 0;\n"
             "}\n";
  }
}

std::string tplFactorial(Language lang) {
  switch (lang) {
    case Language::Python:
      return "n = int(input())\n"
             "r = 1\n"
             "for i in range(2, n + 1):\n"
             "    r *= i\n"
             "print(r)\n";
    case Language::Java:
      return "import java.util.*;\n"
             "public class Main {\n"
             "  public static void main(String[] args) {\n"
             "    Scanner sc = new Scanner(System.in);\n"
             "    int n = sc.nextInt();\n"
             "    long r = 1;\n"
             "    for (int i = 2; i <= n; i++) r *= i;\n"
             "    System.out.println(r);\n"
             "  }\n"
             "}\n";
    case Language::JavaScript:
      return "const fs = require('fs');\n"
             "const n = Number(fs.readFileSync(0, 'utf8').trim());\n"
             "let r = 1n;\n"
             "for (let i = 2; i <= n; i++) r *= BigInt(i);\n"
             "console.log(r.toString());\n";
    case Language::Go:
      return "package main\n\n"
             "import \"fmt\"\n\n"
             "func main() {\n"
             "\tvar n int\n"
             "\tfmt.Scan(&n)\n"
             "\tr := int64(1)\n"
             "\tfor i := 2; i <= n; i++ {\n"
             "\t\tr *= int64(i)\n"
             "\t}\n"
             "\tfmt.Println(r)\n"
             "}\n";
    case Language::Cpp:
    default:
      return "#include <iostream>\nusing namespace std;\n\n"
             "int main() {\n"
             "    int n;\n"
             "    cin >> n;\n"
             "    long long r = 1;\n"
             "    for (int i = 2; i <= n; ++i) r *= i;\n"
             "    cout << r << endl;\n"
             "    return 0;\n"
             "}\n";
  }
}

std::string tplPalindrome(Language lang) {
  switch (lang) {
    case Language::Python:
      return "s = input().strip()\n"
             "print('YES' if s == s[::-1] else 'NO')\n";
    case Language::Java:
      return "import java.util.*;\n"
             "public class Main {\n"
             "  public static void main(String[] args) {\n"
             "    Scanner sc = new Scanner(System.in);\n"
             "    String s = sc.next();\n"
             "    String rev = new StringBuilder(s).reverse().toString();\n"
             "    System.out.println(s.equals(rev) ? \"YES\" : \"NO\");\n"
             "  }\n"
             "}\n";
    case Language::JavaScript:
      return "const fs = require('fs');\n"
             "const s = fs.readFileSync(0, 'utf8').trim();\n"
             "const rev = s.split('').reverse().join('');\n"
             "console.log(s === rev ? 'YES' : 'NO');\n";
    case Language::Go:
      return "package main\n\n"
             "import \"fmt\"\n\n"
             "func main() {\n"
             "\tvar s string\n"
             "\tfmt.Scan(&s)\n"
             "\tok := true\n"
             "\tfor i := 0; i < len(s)/2; i++ {\n"
             "\t\tif s[i] != s[len(s)-1-i] {\n"
             "\t\t\tok = false\n"
             "\t\t\tbreak\n"
             "\t\t}\n"
             "\t}\n"
             "\tif ok {\n"
             "\t\tfmt.Println(\"YES\")\n"
             "\t} else {\n"
             "\t\tfmt.Println(\"NO\")\n"
             "\t}\n"
             "}\n";
    case Language::Cpp:
    default:
      return "#include <iostream>\n#include <string>\n#include <algorithm>\n"
             "using namespace std;\n\n"
             "int main() {\n"
             "    string s;\n"
             "    cin >> s;\n"
             "    string rev = s;\n"
             "    reverse(rev.begin(), rev.end());\n"
             "    cout << (s == rev ? \"YES\" : \"NO\") << endl;\n"
             "    return 0;\n"
             "}\n";
  }
}

}  // namespace

std::string defaultTemplate(Language lang, const std::string& problemKind) {
  if (problemKind == "factorial") return tplFactorial(lang);
  if (problemKind == "palindrome") return tplPalindrome(lang);
  return tplAdd(lang);
}

}  // namespace codetrust
