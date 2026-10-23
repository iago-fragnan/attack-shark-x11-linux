#ifndef AUTOSTART_H
#define AUTOSTART_H

#include <QString>

namespace autostart {

QString desktopFilePath();
bool isEnabled();
bool setEnabled(bool enabled);

} // namespace autostart

#endif // AUTOSTART_H
