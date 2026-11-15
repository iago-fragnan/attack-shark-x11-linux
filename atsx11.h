#ifndef ATSX11_H
#define ATSX11_H

#include "core/types.h"
#include "device/attackshark_device.h"

#include <QColor>
#include <QMainWindow>
#include <QTimer>
#include <array>
#include <limits>

class QCloseEvent;
class QRadioButton;
class QSystemTrayIcon;
class QSpinBox;
class settings;

QT_BEGIN_NAMESPACE
namespace Ui {
class atsx11;
}
QT_END_NAMESPACE

class atsx11 : public QMainWindow
{
    Q_OBJECT

public:
    explicit atsx11(QWidget *parent = nullptr);
    ~atsx11() override;

    bool isMinimizeToTrayEnabled() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_btn_apply_clicked();
    void on_sld_keyresptime_valueChanged(int value);
    void on_sldr_sleeptime_valueChanged(int value);
    void on_sldr_deepSleepTime_valueChanged(int value);
    void on_btn_settings_clicked();
    void on_btn_ledColor_clicked();
    void onSettingsDeviceSelected(const QString &devicePath);
    void reloadSettingsUi(const QString &profileName);
    void onBatteryStatusUpdated(const attackshark::BatteryStatus &status);
    void onConnectionModeChanged(bool isWired, const QString &hidrawPath);
    void onHardwareDpiStageChanged(int stageIndex);
    void onDeviceDisconnected();
    void updateInfoLabels();
    void onUserSettingChanged(bool immediate = false);
    void performAutoApply();
    void updateTrayVisibility();
    void showFromTray();
    void quitApplication();
    void sendLowBatteryNotification(int percentage);

private:
    void connectToDevice(const QString &devicePath);
    void loadSettings();
    void saveSettings();
    attackshark::MouseConfig collectUiConfig() const;
    void applyConfigToUi(const attackshark::MouseConfig &cfg);
    void updateLedColorButton();
    void setupTrayIcon();

    Ui::atsx11 *ui;
    attackshark::AttackSharkDevice m_device;
    settings *m_settingsDialog = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QTimer *m_autoApplyTimer = nullptr;
    QTimer *m_dpiLedRestoreTimer = nullptr;
    QColor m_ledColor { 255, 0, 0 };
    bool m_isQuitting = false;
    int m_lastBatteryAlertLevel = std::numeric_limits<int>::max();
    bool m_isLoadingSettings = false;
    bool m_isHardwareUpdating = false;
    std::array<QRadioButton *, attackshark::kDpiStagesCount> m_stageRadios {};
    std::array<QSpinBox *, attackshark::kDpiStagesCount> m_stageSpins {};
};

#endif // ATSX11_H
