#ifndef ATSX11_H
#define ATSX11_H

#include <QMainWindow>
#include <QFutureWatcher>

class QCloseEvent;
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
    void on_cbox_devices_currentIndexChanged(int index);
    void on_btn_refresh_clicked();
    void on_chbox_alldevices_stateChanged(int arg1);
    void on_sld_keyresptime_sliderMoved(int position);
    void on_sldr_sleeptime_sliderMoved(int position);
    void on_sldr_deepSleepTime_sliderMoved(int position);
    void onBatteryInfoReady();

    void on_btn_settings_clicked();

private:
    void populateDevices(bool allDevices);
    void loadSettings();
    void saveSettings();
    void updateInfoLabels();

    Ui::atsx11 *ui;
    QList<QPair<QString, QString>> devicesConnected;
    QFutureWatcher<int> *m_batteryWatcher = nullptr;
    settings *m_settings = nullptr;
};
#endif // ATSX11_H
