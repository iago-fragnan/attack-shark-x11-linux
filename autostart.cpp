#include "autostart.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace autostart {

QString desktopFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/attackshark.desktop");
}

bool isEnabled()
{
    return QFileInfo::exists(desktopFilePath());
}

bool setEnabled(bool enabled)
{
    const QString path = desktopFilePath();

    if (!enabled)
        return !QFileInfo::exists(path) || QFile::remove(path);

    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir))
        return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    const QString content = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Attack Shark X11\n"
        "Comment=Configuración del mouse Attack Shark\n"
        "Icon=attackshark-x11\n"
        "Exec=\"%1\" --hidden\n"
        "Terminal=false\n"
        "StartupNotify=false\n"
        "X-GNOME-Autostart-enabled=true\n")
        .arg(QCoreApplication::applicationFilePath());

    file.write(content.toUtf8());
    return file.commit();
}

} // namespace autostart
