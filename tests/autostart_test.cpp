#include "../autostart.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QByteArray>
#include <cstdio>

static int g_failures = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_failures; } \
} while (0)

static bool fileContains(const QString &path, const QString &needle)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    return file.readAll().contains(needle.toUtf8());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::printf("  FAIL: cannot create temp dir\n");
        return 1;
    }
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());

    const QString filePath = autostart::desktopFilePath();
    EXPECT(filePath.endsWith(QStringLiteral("/autostart/attackshark.desktop")),
        "paths under XDG_CONFIG_HOME/autostart");

    EXPECT(!autostart::isEnabled(), "no file yet -> disabled");

    EXPECT(autostart::setEnabled(true), "setEnabled(true) succeeds");
    EXPECT(autostart::isEnabled(), "enabled after setEnabled(true)");
    EXPECT(QFileInfo::exists(filePath), "desktop file exists");
    EXPECT(fileContains(filePath, QStringLiteral("Exec=")),
        "desktop file has Exec line");
    EXPECT(fileContains(filePath, QStringLiteral("--hidden")),
        "desktop file Exec passes --hidden");
    EXPECT(fileContains(filePath, QStringLiteral("Type=Application")),
        "desktop file parse header");
    EXPECT(fileContains(filePath, QStringLiteral("Exec=\"")),
        "desktop file Exec is quoted");
    EXPECT(fileContains(filePath, QStringLiteral("Terminal=false")),
        "desktop file has Terminal=false");
    EXPECT(fileContains(filePath, QStringLiteral("StartupNotify=false")),
        "desktop file has StartupNotify=false");
    EXPECT(fileContains(filePath, QStringLiteral("X-GNOME-Autostart-enabled=true")),
        "desktop file has X-GNOME-Autostart-enabled=true");
    EXPECT(fileContains(filePath, QStringLiteral("Icon=attackshark-x11")),
        "desktop file has Icon=attackshark-x11");

    EXPECT(autostart::setEnabled(true), "setEnabled(true) is idempotent");
    EXPECT(autostart::isEnabled(), "still enabled after second enable");

    EXPECT(autostart::setEnabled(false), "setEnabled(false) succeeds");
    EXPECT(!autostart::isEnabled(), "disabled after setEnabled(false)");
    EXPECT(!QFileInfo::exists(filePath), "desktop file removed");

    EXPECT(autostart::setEnabled(false), "setEnabled(false) is idempotent");

    if (g_failures) {
        std::printf("\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll autostart tests PASSED\n");
    return 0;
}
