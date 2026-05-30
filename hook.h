#ifndef HOOK_H
#define HOOK_H

#include <QString>
#include <QList>
#include <QPair>

int setConfig(QString devicePath, int colorMode, int pollingRate);
int getBatteryInfo();
QList<QPair<QString, QString>> getDevices(bool allDevices);

#endif // HOOK_H
