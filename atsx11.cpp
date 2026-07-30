#include "atsx11.h"
#include "./ui_atsx11.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QIcon>
#include <QSettings>
#include <QStyle>
#include <QtConcurrent/QtConcurrent>
#include "hook.h"
#include "settings.h"


namespace {
constexpr auto kSettingsOrg = "AttackShark";
constexpr auto kSettingsApp = "X11";
} // namespace


atsx11::atsx11(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::atsx11)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(QStringLiteral(":/images/assets/mouse.png")));
    ui->btn_settings->setIcon(QIcon());
    ui->btn_settings->setText(QStringLiteral("\u2699"));
    ui->lbl_isCharging->setPixmap(QPixmap());

    ui->lbl_batteryinfo->setEnabled(false);
    ui->pbar_batteryinfo->setEnabled(false);
    ui->lbl_isCharging->setEnabled(false);
    ui->btn_apply->setEnabled(false);

    loadSettings();
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

void atsx11::updateInfoLabels()
{
    ui->lbl_PLRValue->setText(ui->cbox_pollingrate->currentText());
    ui->lbl_infoLEDValue->setText(ui->cbox_colormodes->currentText());
    ui->info_ripplectrlValue->setText(ui->chbox_rippleCTRL->isChecked() ? QStringLiteral("On") : QStringLiteral("Off"));
    ui->lbl_activeAngleSnapValue->setText(ui->chbox_anglesnap->isChecked() ? QStringLiteral("On") : QStringLiteral("Off"));
}

void atsx11::loadDeviceAndBattery(const QString &devicePath)
{
    m_currentDevicePath = devicePath;

    ui->btn_apply->setEnabled(true);
    ui->lbl_batteryinfo->setEnabled(true);
    ui->pbar_batteryinfo->setEnabled(true);
    ui->pbar_batteryinfo->setValue(0);
    ui->lbl_isCharging->setEnabled(false);
    ui->lbl_debug->setText(QStringLiteral("Reading battery\u2026"));

    if (m_batteryWatcher) {
        m_batteryWatcher->cancel();
        m_batteryWatcher->waitForFinished();
        m_batteryWatcher->deleteLater();
        m_batteryWatcher = nullptr;
    }

    m_batteryWatcher = new QFutureWatcher<int>(this);
    connect(m_batteryWatcher, &QFutureWatcher<int>::finished,
            this, &atsx11::onBatteryInfoReady);
    m_batteryWatcher->setFuture(
        QtConcurrent::run([devicePath]() { return getBatteryInfo(devicePath); }));
}

void atsx11::loadSettings()
{
    QSettings qSettings(kSettingsOrg, kSettingsApp);

    const int colorMode    = qSettings.value(QStringLiteral("colorMode"), 0).toInt();
    const int pollingRate  = qSettings.value(QStringLiteral("pollingRate"), 0).toInt();
    const bool angleSnap   = qSettings.value(QStringLiteral("angleSnap"), false).toBool();
    const bool rippleCtrl  = qSettings.value(QStringLiteral("rippleControl"), false).toBool();
    const int sleepTime    = qSettings.value(QStringLiteral("sleepTime"), 5).toInt();
    const int deepSleep    = qSettings.value(QStringLiteral("deepSleepTime"), 10).toInt();
    const int keyRespTime  = qSettings.value(QStringLiteral("keyRespTime"), 8).toInt();
    const QString devPath  = qSettings.value(QStringLiteral("devicePath")).toString();

    if (colorMode >= 0 && colorMode < ui->cbox_colormodes->count())
        ui->cbox_colormodes->setCurrentIndex(colorMode);
    if (pollingRate >= 0 && pollingRate < ui->cbox_pollingrate->count())
        ui->cbox_pollingrate->setCurrentIndex(pollingRate);

    ui->chbox_anglesnap->setChecked(angleSnap);
    ui->chbox_rippleCTRL->setChecked(rippleCtrl);

    ui->sldr_sleeptime->setValue(sleepTime);
    ui->lbl_sleepTimerValue->setText(QStringLiteral("Sleep Time: ") + QString::number(sleepTime) + QStringLiteral("min"));

    ui->sldr_deepSleepTime->setValue(deepSleep);
    ui->lbl_deepsleeptimeValue->setText(QStringLiteral("Deep Sleep Time: ") + QString::number(deepSleep) + QStringLiteral("min"));

    ui->sld_keyresptime->setValue(keyRespTime);
    ui->lbl_kreptimeValueDisplay->setText(QString::number(keyRespTime) + QStringLiteral(" ms"));

    updateInfoLabels();

    if (!devPath.isEmpty())
        loadDeviceAndBattery(devPath);
}

void atsx11::saveSettings()
{
    QSettings qSettings(kSettingsOrg, kSettingsApp);

    qSettings.setValue(QStringLiteral("colorMode"),    ui->cbox_colormodes->currentIndex());
    qSettings.setValue(QStringLiteral("pollingRate"),  ui->cbox_pollingrate->currentIndex());
    qSettings.setValue(QStringLiteral("angleSnap"),    ui->chbox_anglesnap->isChecked());
    qSettings.setValue(QStringLiteral("rippleControl"),ui->chbox_rippleCTRL->isChecked());
    qSettings.setValue(QStringLiteral("sleepTime"),    ui->sldr_sleeptime->value());
    qSettings.setValue(QStringLiteral("deepSleepTime"),ui->sldr_deepSleepTime->value());
    qSettings.setValue(QStringLiteral("keyRespTime"),  ui->sld_keyresptime->value());
    qSettings.sync();
}


void atsx11::on_btn_apply_clicked()
{
    if (m_currentDevicePath.isEmpty()) {
        ui->lbl_debug->setText(QStringLiteral("<html><head/><body><p><span style='color:red;'>Error: no device selected. Open Settings to select a device.</span></p></body></html>"));
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("No device selected. Open Settings to choose a device."));
        return;
    }

    const int color        = ui->cbox_colormodes->currentIndex();
    const int prate        = ui->cbox_pollingrate->currentIndex();
    const bool angleSnap   = ui->chbox_anglesnap->isChecked();
    const int keyRespTime  = ui->sld_keyresptime->value();
    const int sleepTime    = ui->sldr_sleeptime->value();
    const int deepSleepTime= ui->sldr_deepSleepTime->value();
    const bool rippleCtrl  = ui->chbox_rippleCTRL->isChecked();

    const int result = applySettingsFromUser(m_currentDevicePath, color, prate, angleSnap, keyRespTime, sleepTime, deepSleepTime, rippleCtrl);
    if (result != 0) {
        ui->lbl_debug->setText(QStringLiteral("<html><head/><body><p><span style='color:red;'>Error: failed to apply settings</span></p></body></html>"));
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Failed to apply settings to the device (code %1).").arg(result));
        return;
    }

    saveSettings();
    updateInfoLabels();
    ui->lbl_debug->setText(QStringLiteral("<html><head/><body><p><span style='color:green;'>Successfully applied</span></p></body></html>"));
    QMessageBox::information(this, QStringLiteral("Success"), QStringLiteral("Settings applied to %1.").arg(m_currentDevicePath));
}

void atsx11::onBatteryInfoReady()
{
    if (!m_batteryWatcher)
        return;

    const int battery = m_batteryWatcher->result();
    m_batteryWatcher->deleteLater();
    m_batteryWatcher = nullptr;

    if (battery == -2) {
        ui->lbl_isCharging->setEnabled(true);
        ui->lbl_isCharging->setPixmap(QPixmap(":/images/assets/lightning.png"));
        ui->lbl_debug->setText(QStringLiteral("Mouse is charging"));
        ui->pbar_batteryinfo->setValue(0);
    } else if (battery < 0) {
        ui->lbl_isCharging->setEnabled(false);
        ui->lbl_debug->setText(QStringLiteral("<html><head/><body><p><span style='color:orange;'>Battery info unavailable</span></p></body></html>"));
        ui->pbar_batteryinfo->setValue(0);
    } else {
        ui->lbl_isCharging->setEnabled(true);
        ui->lbl_isCharging->setPixmap(QPixmap());
        ui->lbl_debug->setText(QStringLiteral("Battery: ") + QString::number(battery) + QStringLiteral("%"));
        ui->pbar_batteryinfo->setValue(battery);
    }
}

void atsx11::on_sld_keyresptime_sliderMoved(int position)
{
    ui->lbl_kreptimeValueDisplay->setText(QString::number(position) + " ms");
}

void atsx11::on_sldr_deepSleepTime_sliderMoved(int position)
{
    ui->lbl_deepsleeptimeValue->setText("Deep Sleep Time: " + QString::number(position) + "min");
}

void atsx11::on_sldr_sleeptime_sliderMoved(int position)
{
    ui->lbl_sleepTimerValue->setText("Sleep Time: " + QString::number(position) + "min");
}

void atsx11::on_btn_settings_clicked()
{
    if (!m_settings) {
        m_settings = new settings(this);
        connect(m_settings, &settings::deviceSelected,
                this, &atsx11::onSettingsDeviceSelected);
        connect(m_settings, &settings::profileApplied,
                this, &atsx11::reloadSettingsUi);
    }
    m_settings->show();
}

void atsx11::onSettingsDeviceSelected(const QString &devicePath)
{
    loadDeviceAndBattery(devicePath);
    ui->lbl_debug->setText(QStringLiteral("Device selected: ") + devicePath + QStringLiteral(". Reading battery\u2026"));
}

void atsx11::reloadSettingsUi()
{
    QSettings qSettings(kSettingsOrg, kSettingsApp);

    const int colorMode   = qSettings.value(QStringLiteral("colorMode"),    0).toInt();
    const int pollingRate = qSettings.value(QStringLiteral("pollingRate"),   0).toInt();
    const bool angleSnap  = qSettings.value(QStringLiteral("angleSnap"),    false).toBool();
    const bool rippleCtrl = qSettings.value(QStringLiteral("rippleControl"),false).toBool();
    const int sleepTime   = qSettings.value(QStringLiteral("sleepTime"),    5).toInt();
    const int deepSleep   = qSettings.value(QStringLiteral("deepSleepTime"),10).toInt();
    const int keyRespTime = qSettings.value(QStringLiteral("keyRespTime"),  8).toInt();

    if (colorMode >= 0 && colorMode < ui->cbox_colormodes->count())
        ui->cbox_colormodes->setCurrentIndex(colorMode);
    if (pollingRate >= 0 && pollingRate < ui->cbox_pollingrate->count())
        ui->cbox_pollingrate->setCurrentIndex(pollingRate);

    ui->chbox_anglesnap->setChecked(angleSnap);
    ui->chbox_rippleCTRL->setChecked(rippleCtrl);

    ui->sldr_sleeptime->setValue(sleepTime);
    ui->lbl_sleepTimerValue->setText(QStringLiteral("Sleep Time: ") + QString::number(sleepTime) + QStringLiteral("min"));

    ui->sldr_deepSleepTime->setValue(deepSleep);
    ui->lbl_deepsleeptimeValue->setText(QStringLiteral("Deep Sleep Time: ") + QString::number(deepSleep) + QStringLiteral("min"));

    ui->sld_keyresptime->setValue(keyRespTime);
    ui->lbl_kreptimeValueDisplay->setText(QString::number(keyRespTime) + QStringLiteral(" ms"));

    updateInfoLabels();
}
