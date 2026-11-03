#include <QCoreApplication>

#include <cstdio>

#include "CodeTools.h"

// Headless tests for the heuristic code navigation/completion helpers.

namespace {

int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n",                      \
                         __FILE__, __LINE__, #cond);                      \
            ++failures;                                                   \
        }                                                                 \
    } while (false)

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 1. Language mapping by extension.
    CHECK(CodeTools::languageForExtension(QStringLiteral("kt"))
          == CodeTools::Language::Kotlin);
    CHECK(CodeTools::languageForExtension(QStringLiteral("KTS"))
          == CodeTools::Language::Kotlin);
    CHECK(CodeTools::languageForExtension(QStringLiteral("swift"))
          == CodeTools::Language::Swift);
    CHECK(CodeTools::languageForExtension(QStringLiteral("java"))
          == CodeTools::Language::Java);
    CHECK(CodeTools::languageForExtension(QStringLiteral("tsx"))
          == CodeTools::Language::TsJs);
    CHECK(CodeTools::languageForExtension(QStringLiteral("dart"))
          == CodeTools::Language::Dart);
    CHECK(CodeTools::languageForExtension(QStringLiteral("cpp"))
          == CodeTools::Language::Generic);
    CHECK(CodeTools::languageForExtension(QStringLiteral("txt"))
          == CodeTools::Language::Generic);

    // 2. Member collection: unique, sorted.
    {
        const QString text = QStringLiteral(
            "user.name = \"a\";\n"
            "print(user.email);\n"
            "foo(user.alias);\n"
            "user.name = \"b\";\n");
        const QStringList members = CodeTools::collectMembers(
            text, QStringLiteral("user"));
        CHECK(members == QStringList({ QStringLiteral("alias"),
                                       QStringLiteral("email"),
                                       QStringLiteral("name") }));
        CHECK(CodeTools::collectMembers(text, QStringLiteral("nothing"))
              .isEmpty());
    }

    // 3. identifierAt boundaries.
    {
        const QString line = QStringLiteral("    foo.bar(baz);");
        CHECK(CodeTools::identifierAt(line, 0).isEmpty());
        CHECK(CodeTools::identifierAt(line, 4) == QStringLiteral("foo"));
        CHECK(CodeTools::identifierAt(line, 6) == QStringLiteral("foo"));
        CHECK(CodeTools::identifierAt(line, 7) == QStringLiteral("foo"));
        CHECK(CodeTools::identifierAt(line, 8) == QStringLiteral("bar"));
        CHECK(CodeTools::identifierAt(line, 10) == QStringLiteral("bar"));
        // Clicking just past an identifier (e.g. the open paren of a call)
        // resolves to that identifier.
        CHECK(CodeTools::identifierAt(line, 11) == QStringLiteral("bar"));
        CHECK(CodeTools::identifierAt(line, 17).isEmpty());
        CHECK(CodeTools::identifierAt(line, 12) == QStringLiteral("baz"));
        CHECK(CodeTools::identifierAt(line, 14) == QStringLiteral("baz"));
    }

    // 4. Kotlin declarations: functions, properties, classes; calls skipped.
    {
        const QString text = QStringLiteral(
            "// fun fake()\n"
            "class App {\n"
            "    val name = \"x\"\n"
            "    fun greet() {\n"
            "        println(name)\n"
            "        greet()\n"
            "    }\n"
            "}\n");
        const QList<CodeTools::SymbolMatch> greets = CodeTools::findDeclarations(
            text, QStringLiteral("greet"), CodeTools::Language::Kotlin);
        CHECK(greets.size() == 1);
        if (greets.size() == 1) {
            CHECK(greets.first().line == 3);
            CHECK(greets.first().snippet.contains(QStringLiteral("fun greet")));
        }

        const QList<CodeTools::SymbolMatch> names = CodeTools::findDeclarations(
            text, QStringLiteral("name"), CodeTools::Language::Kotlin);
        CHECK(names.size() == 1);
        CHECK(names.first().line == 2);

        const QList<CodeTools::SymbolMatch> apps = CodeTools::findDeclarations(
            text, QStringLiteral("App"), CodeTools::Language::Kotlin);
        CHECK(apps.size() == 1);
        if (apps.size() == 1)
            CHECK(apps.first().line == 1);
    }

    // 5. Swift declarations.
    {
        const QString text = QStringLiteral(
            "class Counter {\n"
            "    let start = 0\n"
            "    func bump() -> Int { return start + 1 }\n"
            "}\n");
        const QList<CodeTools::SymbolMatch> bumps = CodeTools::findDeclarations(
            text, QStringLiteral("bump"), CodeTools::Language::Swift);
        CHECK(bumps.size() == 1);
        if (bumps.size() == 1)
            CHECK(bumps.first().line == 2);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("start"),
              CodeTools::Language::Swift).size() == 1);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("Counter"),
              CodeTools::Language::Swift).size() == 1);
    }

    // 6. Java declarations: class, method, field; call statements skipped.
    {
        const QString text = QStringLiteral(
            "public class Greeter {\n"
            "    private int count;\n"
            "    public void run(int n) {\n"
            "        run(1);\n"
            "        int x = n;\n"
            "    }\n"
            "}\n");
        const QList<CodeTools::SymbolMatch> runs = CodeTools::findDeclarations(
            text, QStringLiteral("run"), CodeTools::Language::Java);
        CHECK(runs.size() == 1);
        if (runs.size() == 1)
            CHECK(runs.first().line == 2);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("count"),
              CodeTools::Language::Java).size() == 1);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("Greeter"),
              CodeTools::Language::Java).size() == 1);
    }

    // 7. TS/JS declarations.
    {
        const QString text = QStringLiteral(
            "function add(a, b) { return a + b; }\n"
            "const multiply = (a, b) => a * b;\n"
            "class Calc {}\n");
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("add"),
              CodeTools::Language::TsJs).size() == 1);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("multiply"),
              CodeTools::Language::TsJs).size() == 1);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("Calc"),
              CodeTools::Language::TsJs).size() == 1);
    }

    // 8. Dart declarations.
    {
        const QString text = QStringLiteral(
            "class Util {\n"
            "  final int seed;\n"
            "  int twice(int v) => v * 2;\n"
            "}\n");
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("twice"),
              CodeTools::Language::Dart).size() == 1);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("Util"),
              CodeTools::Language::Dart).size() == 1);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("seed"),
              CodeTools::Language::Dart).size() == 1);
    }

    // 9. Generic C: functions and defines; recursive calls skipped.
    {
        const QString text = QStringLiteral(
            "int add(int a, int b) {\n"
            "    return add(a, b);\n"
            "}\n"
            "#define MAX 5\n");
        const QList<CodeTools::SymbolMatch> adds = CodeTools::findDeclarations(
            text, QStringLiteral("add"), CodeTools::Language::Generic);
        CHECK(adds.size() == 1);
        if (adds.size() == 1)
            CHECK(adds.first().line == 0);
        CHECK(CodeTools::findDeclarations(text, QStringLiteral("MAX"),
              CodeTools::Language::Generic).size() == 1);
    }

    // 10. Member uses are not declarations (`.greet(...)` never matches).
    {
        const QString text = QStringLiteral(
            "val x = obj.greet()\n"
            "fun greet() {}\n");
        const QList<CodeTools::SymbolMatch> matches = CodeTools::findDeclarations(
            text, QStringLiteral("greet"), CodeTools::Language::Kotlin);
        CHECK(matches.size() == 1);
        CHECK(matches.first().line == 1);
    }

    // 11. replaceAll: count + text, case sensitivity and whole-word.
    {
        const QString text = QStringLiteral("Foo foo FOO foo\n");

        const auto cs = CodeTools::replaceAll(text, QStringLiteral("foo"),
            QStringLiteral("x"), true, false);
        CHECK(cs.count == 2);
        CHECK(cs.text == QStringLiteral("Foo x FOO x\n"));

        const auto ci = CodeTools::replaceAll(text, QStringLiteral("foo"),
            QStringLiteral("x"), false, false);
        CHECK(ci.count == 4);

        const auto ww = CodeTools::replaceAll(text, QStringLiteral("foo"),
            QStringLiteral("bar"), true, true);
        CHECK(ww.count == 2);
        CHECK(ww.text == QStringLiteral("Foo bar FOO bar\n"));

        const CodeTools::ReplaceResult none = CodeTools::replaceAll(
            text, QStringLiteral("zzz"), QStringLiteral("y"), false, false);
        CHECK(none.count == 0);
        CHECK(none.text == text);

        const CodeTools::ReplaceResult emptyTerm = CodeTools::replaceAll(
            text, QString(), QStringLiteral("y"), false, false);
        CHECK(emptyTerm.count == 0);
        CHECK(emptyTerm.text == text);
    }

    if (failures == 0) {
        std::printf("ALL CODETOOLS TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CODETOOLS TEST(S) FAILED\n", failures);
    return 1;
}
