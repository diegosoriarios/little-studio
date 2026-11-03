#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>

#include "LineBuffer.h"
#include "ProcessRunner.h"
#include "ProcessUtil.h"

// Headless tests for the process layer: streaming, exit codes, cancellation,
// ANSI stripping and newline handling. Runs on Linux/macOS with /bin/sh.

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

struct RunResult {
    QStringList lines;
    int exitCode = -999;
    bool crashed = false;
    bool finished = false;
    qint64 elapsedMs = 0;
};

RunResult run(const QString &program, const QStringList &args,
              const QString &workdir = QString(), int timeoutMs = 8000,
              int stopAfterMs = -1,
              const QProcessEnvironment &environment = QProcessEnvironment())
{
    RunResult result;
    ProcessRunner runner;
    QEventLoop loop;

    QObject::connect(&runner, &ProcessRunner::outputLine, &loop,
                     [&result](const QString &line) {
        result.lines.append(line);
    });
    QObject::connect(&runner, &ProcessRunner::finished, &loop,
                     [&result, &loop](int code, bool crashed, qint64 ms) {
        result.exitCode = code;
        result.crashed = crashed;
        result.elapsedMs = ms;
        result.finished = true;
        loop.quit();
    });

    if (stopAfterMs >= 0)
        QTimer::singleShot(stopAfterMs, &runner, &ProcessRunner::stop);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);

    runner.start(program, args, workdir, environment);
    loop.exec();
    return result;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 1. ANSI / OSC stripping.
    CHECK(ProcessUtil::stripAnsi(QStringLiteral("\x1b[31mred\x1b[0m plain"))
          == QStringLiteral("red plain"));
    CHECK(ProcessUtil::stripAnsi(QStringLiteral("\x1b]0;title\x07x"))
          == QStringLiteral("x"));
    CHECK(ProcessUtil::stripAnsi(QStringLiteral("plain"))
          == QStringLiteral("plain"));

    // 2. Newline normalization.
    CHECK(ProcessUtil::normalizeNewlines(QStringLiteral("a\r\nb\rc\nd"))
          == QStringLiteral("a\nb\nc\nd"));

    // 3. Basic streaming and exit code 0.
    {
        const RunResult r = run(QStringLiteral("/bin/sh"),
                                { QStringLiteral("-c"),
                                  QStringLiteral("echo one; echo two") });
        CHECK(r.finished);
        CHECK(r.exitCode == 0);
        CHECK(!r.crashed);
        CHECK(r.lines == QStringList({ QStringLiteral("one"),
                                       QStringLiteral("two") }));
    }

    // 4. stderr is merged into the same stream; non-zero exit preserved.
    {
        const RunResult r = run(QStringLiteral("/bin/sh"),
                                { QStringLiteral("-c"),
                                  QStringLiteral("echo out; echo err >&2; exit 3") });
        CHECK(r.finished);
        CHECK(r.exitCode == 3);
        CHECK(r.lines.contains(QStringLiteral("out")));
        CHECK(r.lines.contains(QStringLiteral("err")));
    }

    // 5. Cancelling a long-running child finishes promptly.
    {
        const RunResult r = run(QStringLiteral("/bin/sh"),
                                { QStringLiteral("-c"),
                                  QStringLiteral("sleep 30") },
                                QString(), 8000, 200);
        CHECK(r.finished);
        CHECK(r.elapsedMs < 5000);
    }

    // 6. Carriage-return progress output becomes separate lines.
    {
        const RunResult r = run(QStringLiteral("/bin/sh"),
                                { QStringLiteral("-c"),
                                  QStringLiteral("printf 'a\\rb\\n'") });
        CHECK(r.finished);
        CHECK(r.lines == QStringList({ QStringLiteral("a"),
                                       QStringLiteral("b") }));
    }

    // 7. Failed start is reported as a finished(-1) job, not a hang.
    {
        const RunResult r = run(QStringLiteral("/nonexistent-lite-studio-prog"),
                                {}, QString(), 5000);
        CHECK(r.finished);
        CHECK(r.exitCode == -1);
        CHECK(r.crashed);
        CHECK(r.lines.join(QLatin1Char('\n'))
              .contains(QStringLiteral("Failed to start")));
    }

    // 8. Working directory is respected.
    {
        QTemporaryDir dir;
        CHECK(dir.isValid());
        const RunResult r = run(QStringLiteral("/bin/sh"),
                                { QStringLiteral("-c"),
                                  QStringLiteral("pwd") },
                                dir.path());
        CHECK(r.finished);
        CHECK(r.exitCode == 0);
        CHECK(!r.lines.isEmpty());
        if (!r.lines.isEmpty()) {
            const QString printed = QFileInfo(r.lines.first()).canonicalFilePath();
            const QString expected =
                QFileInfo(dir.path()).canonicalFilePath();
            CHECK(printed == expected);
        }
    }

    // 9. A supplied environment reaches the child.
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("LITE_TEST_VAR"), QStringLiteral("abc123"));
        const RunResult r = run(QStringLiteral("/bin/sh"),
                                { QStringLiteral("-c"),
                                  QStringLiteral("printf '%s\\n' \"$LITE_TEST_VAR\"") },
                                QString(), 5000, -1, env);
        CHECK(r.finished);
        CHECK(r.exitCode == 0);
        CHECK(r.lines == QStringList({ QStringLiteral("abc123") }));
    }

    // 10. LineBuffer keeps memory bounded and filters case-insensitively.
    {
        LineBuffer buffer(5);
        for (int i = 0; i < 8; ++i)
            buffer.append(QStringLiteral("line %1").arg(i));
        CHECK(buffer.size() == 5);
        CHECK(buffer.lines().first() == QStringLiteral("line 3"));
        CHECK(buffer.lines().last() == QStringLiteral("line 7"));

        CHECK(buffer.filtered(QString()).size() == 5);
        CHECK(buffer.filtered(QStringLiteral("LINE 5"))
              == QStringList({ QStringLiteral("line 5") }));
        CHECK(buffer.filtered(QStringLiteral("nothing")).isEmpty());

        buffer.clear();
        CHECK(buffer.size() == 0);

        LineBuffer batch(3);
        batch.append({ QStringLiteral("a"), QStringLiteral("b"),
                       QStringLiteral("c"), QStringLiteral("d") });
        CHECK(batch.size() == 3);
        CHECK(batch.lines().first() == QStringLiteral("b"));
    }

    if (failures == 0) {
        std::printf("ALL PROCESS TESTS PASSED\n");
        return 0;
    }
    std::printf("%d PROCESS TEST(S) FAILED\n", failures);
    return 1;
}
