#ifndef HOOK_H
#define HOOK_H

#include <QString>
#include <QList>
#include <QPair>

int applySettingsFromUser(const QString &devicePath, int colorMode, int pollingRate, bool angleSnap);
int getBatteryInfo(const QString &devicePath);
QList<QPair<QString, QString>> getDevices(bool allDevices);

#endif // HOOK_H
