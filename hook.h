#ifndef HOOK_H
#define HOOK_H

#include <QString>
#include <QList>
#include <QPair>

#include "dpi.h"

// Reason codes returned by applySettingsFromUser. The window shows the number
// it gets back, so they are part of the user facing behaviour.
enum ApplyError {
    kApplyOk = 0,
    kApplyInvalidSettings = -1,
    kApplyNoUdev = -2,
    kApplyDeviceNotFound = -3,
    kApplyReportFailed = -5,
    kApplyNoPermission = -6,
};

int applySettingsFromUser(const QString &devicePath, int colorMode, int pollingRate, bool angleSnap, int keyRespTime, int sleepTime, int deepSleepTime, bool rippleControl, const int dpiValues[6], int activeDpiStage);

// Explains why the device cannot be written to, including the command that fixes
// it. Empty string when the device can be opened.
QString describeAccessProblem(const QString &devicePath);

// Status byte carried by the battery packet. Byte 3 of the packet is this field,
// it is not part of the signature.
enum BatteryStatus {
    kBatteryStatusUnknown = 0,
    kBatteryStatusNormal = 1,
    kBatteryStatusFullyCharged = 2,
    kBatteryStatusCharging = 3,
};

struct BatteryInfo {
    int percent = -1; // -1 = no data available
    int status = kBatteryStatusUnknown;

    bool valid() const { return percent >= 0; }
};

// Pure decoder for the status packets the mouse pushes on the battery endpoint.
BatteryInfo decodeBatteryPacket(const uint8_t *data, int len);

int getBatteryInfo(const QString &devicePath);
QList<QPair<QString, QString>> getDevices(bool allDevices);
QString findAttackSharkDevice();

#endif // HOOK_H
