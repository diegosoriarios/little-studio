#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

#include "GitTooling.h"

// Headless tests for git tooling: parsing and repo detection. No git binary
// or repository needed - fixtures only.

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
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString base = dir.path();

    // 1. findGit scans an injected PATH.
    {
        const QString tools = base + "/tools";
        CHECK(QDir().mkpath(tools));
        QFile git(tools + "/git");
        CHECK(git.open(QIODevice::WriteOnly));
        git.close();

        CHECK(GitTooling::findGit(tools + ":/usr/bin")
              == tools + "/git");
        CHECK(GitTooling::findGit(base + "/empty").isEmpty());
    }

    // 2. isGitRepository: .git dir, .git file (worktree), absent.
    {
        const QString repo = base + "/repo";
        CHECK(QDir().mkpath(repo + "/.git"));
        CHECK(GitTooling::isGitRepository(repo));

        const QString worktree = base + "/worktree";
        CHECK(QDir().mkpath(worktree));
        QFile gitfile(worktree + "/.git");
        CHECK(gitfile.open(QIODevice::WriteOnly));
        gitfile.write("gitdir: /somewhere/else\n");
        gitfile.close();
        CHECK(GitTooling::isGitRepository(worktree));

        const QString plain = base + "/plain";
        CHECK(QDir().mkpath(plain));
        CHECK(!GitTooling::isGitRepository(plain));
        CHECK(!GitTooling::isGitRepository(QString()));
    }

    // 3. parseNameStatus: statuses, renames, quoted paths, noise lines.
    {
        const QString output = QStringLiteral(
            "* branch            main\n"
            "M\tsrc/a.txt\n"
            "A\tnew file.txt\n"
            "D\told/removed.txt\n"
            "R100\told-name.txt\trenamed.txt\n"
            "C75\tcopy-src.txt\tcopy-dst.txt\n"
            "M\t\"path with \\\"quotes\\\" and spaces.txt\"\n"
            "warning: some noise line\n");
        const QList<GitTooling::ChangedFile> files =
            GitTooling::parseNameStatus(output);
        CHECK(files.size() == 6);
        if (files.size() == 6) {
            CHECK(files.at(0).status == QStringLiteral("M"));
            CHECK(files.at(0).path == QStringLiteral("src/a.txt"));
            CHECK(files.at(1).status == QStringLiteral("A"));
            CHECK(files.at(2).status == QStringLiteral("D"));
            CHECK(files.at(3).status == QStringLiteral("R100"));
            CHECK(files.at(3).path == QStringLiteral("renamed.txt"));
            CHECK(files.at(4).status == QStringLiteral("C75"));
            CHECK(files.at(4).path == QStringLiteral("copy-dst.txt"));
            CHECK(files.at(5).path
                  == QStringLiteral("path with \"quotes\" and spaces.txt"));
        }

        CHECK(GitTooling::parseNameStatus(
            QStringLiteral("List of devices attached\n")).isEmpty());
        CHECK(GitTooling::parseNameStatus(QStringLiteral("")).isEmpty());
    }

    // 4. diff section offsets: two files, per-file slicing.
    {
        const QString diff = QStringLiteral(
            "diff --git a/a.txt b/a.txt\n"
            "index 111..222 100644\n"
            "--- a/a.txt\n"
            "+++ b/a.txt\n"
            "@@ -1 +1 @@\n"
            "-one\n"
            "+one two\n"
            "diff --git a/b.txt b/b.txt\n"
            "index 333..444 100644\n"
            "--- a/b.txt\n"
            "+++ b/b.txt\n"
            "@@ -1 +1 @@\n"
            "-x\n"
            "+x y\n");

        const QList<GitTooling::SectionRange> sections =
            GitTooling::diffSectionOffsets(diff);
        CHECK(sections.size() == 2);
        if (sections.size() == 2) {
            CHECK(sections.at(0).path == QStringLiteral("a.txt"));
            CHECK(sections.at(0).start == 0);
            CHECK(diff.mid(sections.at(0).start,
                           sections.at(0).end - sections.at(0).start)
                  .contains(QStringLiteral("+one two")));
            CHECK(!diff.mid(sections.at(0).start,
                            sections.at(0).end - sections.at(0).start)
                  .contains(QStringLiteral("b.txt")));

            CHECK(sections.at(1).path == QStringLiteral("b.txt"));
            CHECK(diff.mid(sections.at(1).start,
                           sections.at(1).end - sections.at(1).start)
                  .contains(QStringLiteral("+x y")));
            CHECK(!diff.mid(sections.at(1).start,
                            sections.at(1).end - sections.at(1).start)
                  .contains(QStringLiteral("a.txt")));
        }
    }

    // 5. diff.noprefix style headers and deleted files.
    {
        const QString diff = QStringLiteral(
            "diff --git run.c run.c\n"
            "--- run.c\n"
            "+++ run.c\n"
            "@@ -1 +1 @@\n"
            "+int x;\n"
            "diff --git a/gone.txt b/gone.txt\n"
            "deleted file mode 100644\n"
            "--- a/gone.txt\n"
            "+++ /dev/null\n"
            "@@ -1 +0,0 @@\n"
            "-content\n");
        const QList<GitTooling::SectionRange> sections =
            GitTooling::diffSectionOffsets(diff);
        CHECK(sections.size() == 2);
        if (sections.size() == 2) {
            CHECK(sections.at(0).path == QStringLiteral("run.c"));
            CHECK(sections.at(1).path == QStringLiteral("gone.txt"));
        }
    }

    if (failures == 0) {
        std::printf("ALL GIT TESTS PASSED\n");
        return 0;
    }
    std::printf("%d GIT TEST(S) FAILED\n", failures);
    return 1;
}
