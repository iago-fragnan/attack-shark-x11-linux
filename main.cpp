#include "atsx11.h"

#include <unistd.h>
#include <pwd.h>

#include <QApplication>
#include <QColor>
#include <QMessageBox>
#include <QPalette>
#include <QProcess>
#include <QStandardPaths>
#include <QStyleFactory>

namespace {

void applyDarkTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    const QColor window(30, 30, 30);
    const QColor text(242, 242, 242);
    const QColor base(37, 37, 37);
    const QColor button(45, 45, 45);
    const QColor disabled(127, 127, 127);
    const QColor highlight(42, 130, 218);

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, button);
    palette.setColor(QPalette::ToolTipBase, text);
    palette.setColor(QPalette::ToolTipText, window);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, highlight);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);

    app.setPalette(palette);
}

QString desktopUsername()
{
    if (const QByteArray sudoUser = qgetenv("SUDO_USER"); !sudoUser.isEmpty())
        return QString::fromLocal8Bit(sudoUser);

    if (const QByteArray pkexecUid = qgetenv("PKEXEC_UID"); !pkexecUid.isEmpty()) {
        if (const passwd *pw = getpwuid(static_cast<uid_t>(pkexecUid.toUInt())); pw && pw->pw_name)
            return QString::fromLocal8Bit(pw->pw_name);
    }

    return {};
}

void adoptDesktopUserEnvironment()
{
    if (geteuid() != 0)
        return;

    const QString username = desktopUsername();
    if (username.isEmpty())
        return;

    const passwd *pw = getpwnam(username.toUtf8().constData());
    if (!pw || !pw->pw_dir)
        return;

    const QString home = QString::fromLocal8Bit(pw->pw_dir);
    const QByteArray user = username.toLocal8Bit();

    qputenv("HOME", home.toLocal8Bit());
    qputenv("USER", user);
    qputenv("LOGNAME", user);

    const auto setPathEnv = [](const char *key, const QString &path) {
        if (qEnvironmentVariableIsEmpty(key))
            qputenv(key, path.toLocal8Bit());
    };

    setPathEnv("XDG_CONFIG_HOME", home + QStringLiteral("/.config"));
    setPathEnv("XDG_DATA_HOME", home + QStringLiteral("/.local/share"));
    setPathEnv("XDG_CACHE_HOME", home + QStringLiteral("/.cache"));

    if (qEnvironmentVariableIsEmpty("XDG_DATA_DIRS")) {
        qputenv("XDG_DATA_DIRS",
                QByteArray("/usr/local/share:/usr/share:")
                    + (home + QStringLiteral("/.local/share")).toLocal8Bit());
    }
}

QStringList guiSessionEnvironment()
{
    static const char *keys[] = {
        "DISPLAY",
        "XAUTHORITY",
        "WAYLAND_DISPLAY",
        "XDG_RUNTIME_DIR",
        "DBUS_SESSION_BUS_ADDRESS",
        "QT_QPA_PLATFORM",
        "XDG_SESSION_TYPE",
        "HOME",
        "USER",
        "LOGNAME",
        "XDG_CONFIG_HOME",
        "XDG_DATA_HOME",
        "XDG_CACHE_HOME",
        "XDG_DATA_DIRS",
    };

    QStringList env;
    for (const char *key : keys) {
        const QByteArray value = qgetenv(key);
        if (!value.isEmpty())
            env << QStringLiteral("%1=%2").arg(key, QString::fromLocal8Bit(value));
    }
    return env;
}

bool startElevated(const QString &program, const QStringList &arguments)
{
    QStringList args;
    args << QStringLiteral("env") << guiSessionEnvironment() << program << arguments;
    return QProcess::startDetached(QStringLiteral("pkexec"), args);
}

bool requestElevatedRestart()
{
    const QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        QObject::tr("Administrator privileges required"),
        QObject::tr(
            "This application needs root access to configure USB devices.\n\n"
            "Restart with elevated privileges?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (reply != QMessageBox::Yes)
        return false;

    const QString appPath = QCoreApplication::applicationFilePath();
    const QStringList appArgs = QCoreApplication::arguments().mid(1);

    if (startElevated(appPath, appArgs))
        return true;

    const QString kdesu = QStandardPaths::findExecutable(QStringLiteral("kdesu"));
    if (!kdesu.isEmpty()) {
        QStringList kdesuArgs;
        kdesuArgs << appPath << appArgs;
        if (QProcess::startDetached(kdesu, kdesuArgs))
            return true;
    }

    QMessageBox::critical(
        nullptr,
        QObject::tr("Elevation failed"),
        QObject::tr(
            "Could not restart with elevated privileges.\n"
            "Try running the application with sudo instead."));
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    adoptDesktopUserEnvironment();

    QApplication a(argc, argv);
    applyDarkTheme(a);

    if (geteuid() != 0) {
        if (requestElevatedRestart())
            return 0;
        return 1;
    }

    atsx11 w;
    w.show();
    return a.exec();
}
