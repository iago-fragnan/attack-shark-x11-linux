#include "atsx11.h"
#include "./ui_atsx11.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QIcon>
#include <QSettings>
#include <QStyle>
#include <QtConcurrent/QtConcurrent>
#include "hook.h"

#include <algorithm>


namespace {
constexpr auto kSettingsOrg = "AttackShark";
constexpr auto kSettingsApp = "X11";
} // namespace


atsx11::atsx11(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::atsx11)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(QStringLiteral(":/images/mouse.png")));
    ui->btn_refresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    loadSettings();
    ui->lbl_batteryinfo->setEnabled(false);
    ui->pbar_batteryinfo->setEnabled(false);
    ui->btn_apply->setEnabled(false);
}

atsx11::~atsx11()
{
    delete ui;
}

void atsx11::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void atsx11::populateDevices(bool allDevices)
{
    ui->cbox_devices->clear();
    devicesConnected = getDevices(allDevices);
    for (const auto &device : devicesConnected) {
        ui->cbox_devices->addItem(QString("%1 (%2)").arg(device.second).arg(device.first));
    }
}

void atsx11::loadSettings()
{
    QSettings settings(kSettingsOrg, kSettingsApp);

    const bool allDevices = settings.value(QStringLiteral("allDevices"), false).toBool();
    const int colorMode = settings.value(QStringLiteral("colorMode"), 0).toInt();
    const int pollingRate = settings.value(QStringLiteral("pollingRate"), 0).toInt();
    const QString devicePath = settings.value(QStringLiteral("devicePath")).toString();

    ui->chbox_alldevices->blockSignals(true);
    ui->chbox_alldevices->setChecked(allDevices);
    ui->chbox_alldevices->blockSignals(false);

    populateDevices(allDevices);

    if (!devicePath.isEmpty()) {
        for (int i = 0; i < devicesConnected.size(); ++i) {
            if (devicesConnected[i].first == devicePath) {
                ui->cbox_devices->setCurrentIndex(i);
                break;
            }
        }
    }

    if (colorMode >= 0 && colorMode < ui->cbox_colormodes->count())
        ui->cbox_colormodes->setCurrentIndex(colorMode);
    if (pollingRate >= 0 && pollingRate < ui->cbox_pollingrate->count())
        ui->cbox_pollingrate->setCurrentIndex(pollingRate);

    const bool angleSnap = settings.value(QStringLiteral("angleSnap"), false).toBool();
    ui->chbox_anglesnap->setChecked(angleSnap);
}

void atsx11::saveSettings()
{
    QSettings settings(kSettingsOrg, kSettingsApp);

    settings.setValue(QStringLiteral("allDevices"), ui->chbox_alldevices->isChecked());
    settings.setValue(QStringLiteral("colorMode"), ui->cbox_colormodes->currentIndex());
    settings.setValue(QStringLiteral("pollingRate"), ui->cbox_pollingrate->currentIndex());
    settings.setValue(QStringLiteral("angleSnap"), ui->chbox_anglesnap->isChecked());

    QString devicePath;
    const int index = ui->cbox_devices->currentIndex();
    if (index >= 0 && index < devicesConnected.size())
        devicePath = devicesConnected[index].first;

    settings.setValue(QStringLiteral("devicePath"), devicePath);
    settings.sync();
}


void atsx11::on_btn_apply_clicked()
{
    // Settings defualt
    QString devicePath;
    int color, prate = 0;
    bool angleSnap = false;

    // values from the UI
    QString device = ui->cbox_devices->currentText();
    if(device == ""){
        ui->lbl_debug->setText("<html><head/><body><p><span style='color:red;'>Error: device not found</span></p></body></html>");
        QMessageBox::warning(this, "Error", "Device not selected.");
        return;
    }
    color = ui->cbox_colormodes->currentIndex();
    prate = ui->cbox_pollingrate->currentIndex();
    angleSnap = ui->chbox_anglesnap->isChecked();

    for (const auto& pair : devicesConnected) {
        if (QString("%1 (%2)").arg(pair.second).arg(pair.first) == device) {
            devicePath = pair.first;
            break;
        }
    }

    if (devicePath.isEmpty()) {
        ui->lbl_debug->setText("<html><head/><body><p><span style='color:red;'>Error: device not found</span></p></body></html>");
        QMessageBox::warning(this, "Error", "Device not found in the connected list.");
        return;
    }

    const int result = applySettingsFromUser(devicePath, color, prate, angleSnap);
    if (result != 0) {
        ui->lbl_debug->setText("<html><head/><body><p><span style='color:red;'>Error: failed to apply settings</span></p></body></html>");
        QMessageBox::warning(this, "Error", "Failed to apply settings to the device (code " + QString::number(result) + ").");
        return;
    }



    saveSettings();
    ui->lbl_debug->setText("<html><head/><body><p><span style='color:green;'>Sucessfully applied</span></p></body></html>");
    QMessageBox::information(this, "Success", "Settings applied to " + devicePath + ".");
}


void atsx11::on_cbox_devices_currentIndexChanged(int index)
{
    if (index < 0 || index >= devicesConnected.size()) {
        ui->lbl_batteryinfo->setEnabled(false);
        ui->pbar_batteryinfo->setEnabled(false);
        ui->btn_apply->setEnabled(false);
        return;
    }

    ui->btn_apply->setEnabled(true);

    if (m_batteryWatcher) {
        m_batteryWatcher->cancel();
        m_batteryWatcher->waitForFinished();
        m_batteryWatcher->deleteLater();
        m_batteryWatcher = nullptr;
    }

    const QString devicePath = devicesConnected[index].first;
    ui->lbl_debug->setText("Reading battery\u2026");
    ui->lbl_batteryinfo->setEnabled(true);
    ui->pbar_batteryinfo->setEnabled(true);
    ui->pbar_batteryinfo->setValue(0);

    m_batteryWatcher = new QFutureWatcher<int>(this);
    connect(m_batteryWatcher, &QFutureWatcher<int>::finished,
            this, &atsx11::onBatteryInfoReady);
    m_batteryWatcher->setFuture(
        QtConcurrent::run([devicePath]() { return getBatteryInfo(devicePath); }));
}

void atsx11::onBatteryInfoReady()
{
    if (!m_batteryWatcher)
        return;

    const int battery = m_batteryWatcher->result();
    m_batteryWatcher->deleteLater();
    m_batteryWatcher = nullptr;

    if (battery < 0) {
        ui->lbl_debug->setText("<html><head/><body><p><span style='color:orange;'>Battery info unavailable</span></p></body></html>");
        ui->pbar_batteryinfo->setValue(0);
    } else {
        ui->lbl_debug->setText("Battery: " + QString::number(battery) + "%");
        ui->pbar_batteryinfo->setValue(battery);
    }
}


void atsx11::on_btn_refresh_clicked()
{
    ui->cbox_devices->setCurrentIndex(-1);
    ui->lbl_batteryinfo->setEnabled(false);
    ui->pbar_batteryinfo->setEnabled(false);
    ui->pbar_batteryinfo->setValue(-1);

    populateDevices(ui->chbox_alldevices->isChecked());

    const int quantity = devicesConnected.size();
    ui->lbl_debug->setText("Refreshed devices list (" + QString::number(quantity) + " devices).");
}

void atsx11::on_chbox_alldevices_stateChanged(int arg1)
{
    ui->cbox_devices->clear();
    populateDevices(arg1 != 0);

    const int quantity = devicesConnected.size();
    if (arg1 == 0) {
        ui->lbl_debug->setText("Listing only mice devices (" + QString::number(quantity) + ").");
    } else {
        ui->lbl_debug->setText("Listing all devices (" + QString::number(quantity) + ").");
    }
}

void atsx11::on_sld_keyresptime_sliderMoved(int position)
{
    ui->lbl_kreptimeValueDisplay->setText(QString::number(position) + " ms");
    ui->lbl_debug->setText("[DEBUG] Ripple Speed: " + QString::number(position));
}

void atsx11::on_sldr_deepSleepTime_sliderMoved(int position)
{
    ui->lbl_deepsleeptimeValue->setText("Deep Sleep Time: " + QString::number(position) + "min");
}

void atsx11::on_sldr_sleeptime_sliderMoved(int position)
{
    ui->lbl_sleepTimerValue->setText("Sleep Time: " + QString::number(position) + "min");
}



