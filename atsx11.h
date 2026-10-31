#ifndef ATSX11_H
#define ATSX11_H

#include "core/types.h"
#include "device/attackshark_device.h"

#include <QColor>
#include <QMainWindow>
#include <QTimer>
#include <array>

class QCloseEvent;
class QRadioButton;
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
    void reloadSettingsUi();
    void onBatteryStatusUpdated(const attackshark::BatteryStatus &status);
    void onConnectionModeChanged(bool isWired, const QString &hidrawPath);
    void onHardwareDpiStageChanged(int stageIndex);
    void onDeviceDisconnected();
    void updateInfoLabels();
    void onUserSettingChanged(bool immediate = false);
    void performAutoApply();

private:
    void connectToDevice(const QString &devicePath);
    void loadSettings();
    void saveSettings();
    attackshark::MouseConfig collectUiConfig() const;
    void applyConfigToUi(const attackshark::MouseConfig &cfg);
    void updateLedColorButton();

    Ui::atsx11 *ui;
    attackshark::AttackSharkDevice m_device;
    settings *m_settingsDialog = nullptr;
    QTimer *m_autoApplyTimer = nullptr;
    QTimer *m_dpiLedRestoreTimer = nullptr;
    QColor m_ledColor { 255, 0, 0 };
    bool m_isLoadingSettings = false;
    bool m_isHardwareUpdating = false;
    std::array<QRadioButton *, attackshark::kDpiStagesCount> m_stageRadios {};
    std::array<QSpinBox *, attackshark::kDpiStagesCount> m_stageSpins {};
};

#endif // ATSX11_H
