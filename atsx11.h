#ifndef ATSX11_H
#define ATSX11_H

#include <QMainWindow>
#include <QFutureWatcher>
#include <QTimer>
#include "dpibar.h"

class QCloseEvent;
class systemtray;
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
    atsx11(QWidget *parent = nullptr);
    ~atsx11();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_btn_apply_clicked();
    void on_sld_keyresptime_sliderMoved(int position);
    void on_sldr_sleeptime_sliderMoved(int position);
    void on_sldr_deepSleepTime_sliderMoved(int position);
    void onBatteryInfoReady();
    void refreshBattery();
    void on_btn_settings_clicked();
    void onSettingsDeviceSelected(const QString &devicePath);
    void reloadSettingsUi();
    void onDpiValueChanged(int stage, int dpi);
    void onDpiStageActivated(int stage);
    void onTrayRestoreRequested();
    void onTrayQuitRequested();

private:
    void loadDeviceAndBattery(const QString &devicePath, bool resetUi = true);
    void loadSettings();
    void saveSettings();
    void updateInfoLabels();

    Ui::atsx11 *ui;
    QString m_currentDevicePath;
    QFutureWatcher<int> *m_batteryWatcher = nullptr;
    QTimer *m_batteryTimer = nullptr;
    settings *m_settings = nullptr;
    DpiBarWidget *m_dpiBar = nullptr;
    systemtray *m_tray = nullptr;
    bool m_quitting = false;
};
#endif // ATSX11_H
