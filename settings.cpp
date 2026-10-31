#include "settings.h"
#include "ui_settings.h"
#include "device/attackshark_device.h"

#include <QMessageBox>
#include <QSettings>
#include <QStyle>

namespace {
constexpr auto kSettingsOrg = "AttackShark";
constexpr auto kSettingsApp = "X11";
} // namespace

settings::settings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::settings)
{
    ui->setupUi(this);
    ui->btn_refresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

    QSettings qSettings(kSettingsOrg, kSettingsApp);
    const bool allDevices   = qSettings.value(QStringLiteral("allDevices"), false).toBool();
    const QString savedPath = qSettings.value(QStringLiteral("devicePath")).toString();

    ui->chbox_alldevices->blockSignals(true);
    ui->chbox_alldevices->setChecked(allDevices);
    ui->chbox_alldevices->blockSignals(false);

    populateDevices(allDevices);

    if (!savedPath.isEmpty()) {
        for (int i = 0; i < m_devicesConnected.size(); ++i) {
            if (m_devicesConnected[i].path == savedPath || m_devicesConnected[i].hidrawPath == savedPath) {
                ui->cbox_devices->setCurrentIndex(i);
                break;
            }
        }
    }
}

settings::~settings()
{
    delete ui;
}

void settings::populateDevices(bool allDevices)
{
    ui->cbox_devices->clear();
    m_devicesConnected = attackshark::AttackSharkDevice::enumerateDevices(allDevices);
    for (const auto &device : m_devicesConnected) {
        ui->cbox_devices->addItem(QStringLiteral("%1 (%2)").arg(device.name, device.hidrawPath));
    }
}

void settings::on_btn_refresh_clicked()
{
    ui->cbox_devices->setCurrentIndex(-1);
    populateDevices(ui->chbox_alldevices->isChecked());
}

void settings::on_chbox_alldevices_checkStateChanged(const Qt::CheckState &arg1)
{
    populateDevices(arg1 != Qt::Unchecked);
}

void settings::on_btn_applyProfilePreset_clicked()
{
    const int deviceIndex = ui->cbox_devices->currentIndex();
    if (deviceIndex < 0 || deviceIndex >= m_devicesConnected.size()) {
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("No device selected."));
        return;
    }

    const int presetIndex = ui->cbox_profilesPresets->currentIndex();
    if (presetIndex <= 0) {
        QMessageBox::information(this, QStringLiteral("Profiles"), QStringLiteral("Select a profile preset to apply."));
        return;
    }

    attackshark::MouseConfig cfg;
    if (presetIndex == 1) {
        // Battery Saver / Office
        cfg.dpiStages        = {{{400}, {800}, {1200}, {1600}, {2400}}};
        cfg.activeDpiStage   = 1; // 800 DPI
        cfg.pollingRate      = attackshark::PollingRate::Hz125;
        cfg.colorMode        = attackshark::ColorMode::Disabled;
        cfg.ledRed           = 0;
        cfg.ledGreen         = 0;
        cfg.ledBlue          = 0;
        cfg.keyResponseTimeMs = 10;
        cfg.angleSnap        = false;
        cfg.rippleControl    = false;
        cfg.sleepTimeMin     = 1;
        cfg.deepSleepTimeMin = 5;
    } else if (presetIndex == 2) {
        // Balanced / Everyday
        cfg.dpiStages        = {{{400}, {800}, {1600}, {2400}, {3200}}};
        cfg.activeDpiStage   = 2; // 1600 DPI
        cfg.pollingRate      = attackshark::PollingRate::Hz500;
        cfg.colorMode        = attackshark::ColorMode::Breathing;
        cfg.ledRed           = 0;
        cfg.ledGreen         = 180;
        cfg.ledBlue          = 255;
        cfg.keyResponseTimeMs = 8;
        cfg.angleSnap        = false;
        cfg.rippleControl    = true;
        cfg.sleepTimeMin     = 5;
        cfg.deepSleepTimeMin = 15;
    } else {
        // Competitive Gaming
        cfg.dpiStages        = {{{400}, {800}, {1600}, {3200}, {6400}}};
        cfg.activeDpiStage   = 2; // 1600 DPI
        cfg.pollingRate      = attackshark::PollingRate::Hz1000;
        cfg.colorMode        = attackshark::ColorMode::Static;
        cfg.ledRed           = 255;
        cfg.ledGreen         = 0;
        cfg.ledBlue          = 0;
        cfg.keyResponseTimeMs = 4;
        cfg.angleSnap        = false;
        cfg.rippleControl    = false;
        cfg.sleepTimeMin     = 10;
        cfg.deepSleepTimeMin = 30;
    }

    const QString devicePath = m_devicesConnected[deviceIndex].hidrawPath;
    attackshark::AttackSharkDevice dev;
    if (!dev.openDevice(devicePath)) {
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("Could not open device %1.").arg(devicePath));
        return;
    }

    QString errMsg;
    if (!dev.applyConfiguration(cfg, &errMsg)) {
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("Failed to apply profile: %1").arg(errMsg));
        return;
    }

    QSettings qSettings(kSettingsOrg, kSettingsApp);
    for (int i = 0; i < attackshark::kDpiStagesCount; ++i) {
        qSettings.setValue(QStringLiteral("dpiStage%1").arg(i + 1), cfg.dpiStages[i].dpi);
    }
    qSettings.setValue(QStringLiteral("activeDpiStage"), cfg.activeDpiStage);
    qSettings.setValue(QStringLiteral("pollingRate"),    static_cast<int>(cfg.pollingRate));
    qSettings.setValue(QStringLiteral("colorMode"),      static_cast<int>(cfg.colorMode));
    qSettings.setValue(QStringLiteral("ledRed"),         cfg.ledRed);
    qSettings.setValue(QStringLiteral("ledGreen"),       cfg.ledGreen);
    qSettings.setValue(QStringLiteral("ledBlue"),        cfg.ledBlue);
    qSettings.setValue(QStringLiteral("keyRespTime"),    cfg.keyResponseTimeMs);
    qSettings.setValue(QStringLiteral("angleSnap"),      cfg.angleSnap);
    qSettings.setValue(QStringLiteral("rippleControl"),  cfg.rippleControl);
    qSettings.setValue(QStringLiteral("sleepTime"),      cfg.sleepTimeMin);
    qSettings.setValue(QStringLiteral("deepSleepTime"),  cfg.deepSleepTimeMin);
    qSettings.sync();

    emit profileApplied();

    QMessageBox::information(this, QStringLiteral("Success"),
        QStringLiteral("Profile \"%1\" flashed to %2.")
            .arg(ui->cbox_profilesPresets->currentText(), devicePath));
}

void settings::on_buttonBox_accepted()
{
    const int index = ui->cbox_devices->currentIndex();
    if (index < 0 || index >= m_devicesConnected.size()) {
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Please select a device before confirming."));
        return;
    }

    const QString devicePath = m_devicesConnected[index].hidrawPath;

    QSettings qSettings(kSettingsOrg, kSettingsApp);
    qSettings.setValue(QStringLiteral("devicePath"), devicePath);
    qSettings.setValue(QStringLiteral("allDevices"), ui->chbox_alldevices->isChecked());
    qSettings.sync();

    emit deviceSelected(devicePath);
    accept();
}
