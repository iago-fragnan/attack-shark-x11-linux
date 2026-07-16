#ifndef ATSX11_H
#define ATSX11_H

#include <QMainWindow>
#include <QFutureWatcher>

class QCloseEvent;

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

    void onBatteryInfoReady();

    void on_horizontalSlider_sliderMoved(int position);

    void on_checkBox_stateChanged(int arg1);

    void on_sld_keyreptime_valueChanged(int value);

    void on_sld_keyreptime_actionTriggered(int action);

    void on_sld_keyreptime_sliderMoved(int position);

    void on_sld_keyreptime_rangeChanged(int min, int max);

    void on_horizontalSlider_valueChanged(int value);

    void on_sld_keyresptime_sliderMoved(int position);

private:
    void populateDevices(bool allDevices);
    void loadSettings();
    void saveSettings();

    Ui::atsx11 *ui;
    QList<QPair<QString, QString>> devicesConnected;
    QFutureWatcher<int> *m_batteryWatcher = nullptr;
};
#endif // ATSX11_H
