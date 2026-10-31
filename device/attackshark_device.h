#ifndef DEVICE_ATTACKSHARK_DEVICE_H
#define DEVICE_ATTACKSHARK_DEVICE_H

#include "../core/types.h"

#include <QList>
#include <QObject>
#include <QPair>
#include <QSocketNotifier>
#include <QString>
#include <QTimer>

namespace attackshark {

class AttackSharkDevice : public QObject
{
    Q_OBJECT

public:
    explicit AttackSharkDevice(QObject *parent = nullptr);
    ~AttackSharkDevice() override;

    // Device enumeration & auto-detection
    static QList<DeviceInfo> enumerateDevices(bool allHidDevices = false);
    static QString findDefaultDevicePath();
    static QString resolveConfigHidrawPath(const QString &devicePath, bool *isWiredOut = nullptr);

    // Connect / monitor a specific device path for telemetry (battery/charging)
    bool openDevice(const QString &devicePath);
    void closeDevice();
    QString currentDevicePath() const { return m_devicePath; }
    QString currentHidrawPath() const { return m_hidrawPath; }
    bool isConnected() const { return m_fd >= 0; }
    bool isWiredMode() const { return m_isWired; }

    // Apply full mouse configuration (DPI & Sensor first, LED/Power/Debounce second, Polling Rate third)
    bool applyConfiguration(const MouseConfig &config, QString *errorMessage = nullptr);

    // Apply only the LED & Sleep/Debounce configuration (Report 0x05)
    bool sendColorReport(const MouseConfig &config, QString *errorMessage = nullptr);

    // Synchronous one-shot battery read (with timeoutMs)
    BatteryStatus pollBatteryOnce(int timeoutMs = 1500);

    BatteryStatus lastBatteryStatus() const { return m_lastBattery; }

signals:
    void batteryStatusChanged(const attackshark::BatteryStatus &status);
    void activeDpiStageChanged(int stageIndex);
    void commandAcknowledged(int reportId, bool ok);
    void connectionModeChanged(bool isWired, const QString &hidrawPath);
    void deviceDisconnected();

private slots:
    void onHidrawReadyRead();
    void checkHotplugTransition();

private:
    bool sendFeatureReport(const uint8_t *data, size_t length, QString *errorMessage = nullptr);
    void parseIncomingReport(const uint8_t *buf, ssize_t length);

    static void buildPollingReport(PollingRate rate, uint8_t out[9]);
    static void buildColorAndPowerReport(const MouseConfig &config, uint8_t out[15]);
    static void buildDpiAndSensorReport(const MouseConfig &config, uint8_t out[56]);

    QString m_devicePath;
    QString m_hidrawPath;
    int m_fd = -1;
    bool m_isWired = false;
    QSocketNotifier *m_readNotifier = nullptr;
    QTimer *m_hotplugTimer = nullptr;
    BatteryStatus m_lastBattery;
};

} // namespace attackshark

#endif // DEVICE_ATTACKSHARK_DEVICE_H
