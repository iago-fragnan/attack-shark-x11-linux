#ifndef HOOK_H
#define HOOK_H

#include <QString>

class x11mouse
{
public:
    x11mouse();
};

int setConfig(QString sel_deviceID, int sel_colormode, int sel_prate);
int getBatteryInfo();
QList<QPair<QString, QString>> getDevices(bool allDevices);

#endif // HOOK_H
