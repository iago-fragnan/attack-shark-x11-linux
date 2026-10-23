#include "atsx11.h"

#include <unistd.h>
#include <pwd.h>

#include <QApplication>
#include <QColor>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPalette>
#include <QStyleFactory>
#include <QSystemTrayIcon>

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

} // namespace

int main(int argc, char *argv[])
{
    adoptDesktopUserEnvironment();

    QApplication a(argc, argv);
    applyDarkTheme(a);

    const QString serverName = QStringLiteral("attackshark-x11-")
        + QString::number(getuid());

    QLocalSocket probe;
    probe.connectToServer(serverName);
    if (probe.waitForConnected(200) || probe.state() == QLocalSocket::ConnectedState) {
        probe.write("show");
        probe.flush();
        probe.waitForBytesWritten(200);
        return 0;
    }

    QLocalServer server;
    QLocalServer::removeServer(serverName);
    if (!server.listen(serverName))
        return 0;

    atsx11 w;

    QObject::connect(&server, &QLocalServer::newConnection, &w,
        [&server, &w] {
            QLocalSocket *client = server.nextPendingConnection();
            if (!client)
                return;
            client->deleteLater();
            w.show();
            w.raise();
            w.activateWindow();
        });

    const QStringList args = QCoreApplication::arguments();
    const bool startHidden = args.contains(QStringLiteral("--hidden"))
        && QSystemTrayIcon::isSystemTrayAvailable();
    if (!startHidden)
        w.show();

    return a.exec();
}
