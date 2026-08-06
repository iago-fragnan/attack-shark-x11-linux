#include "settings.h"
#include "ui_settings.h"
#include "hook.h"
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
    const bool allDevices  = qSettings.value(QStringLiteral("allDevices"), false).toBool();
    const QString savedPath = qSettings.value(QStringLiteral("devicePath")).toString();

    ui->chbox_alldevices->blockSignals(true);
    ui->chbox_alldevices->setChecked(allDevices);
    ui->chbox_alldevices->blockSignals(false);

    populateDevices(allDevices);

    if (!savedPath.isEmpty()) {
        for (int i = 0; i < devicesConnected.size(); ++i) {
            if (devicesConnected[i].first == savedPath) {
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
    devicesConnected = getDevices(allDevices);
    for (const auto &device : devicesConnected) {
        ui->cbox_devices->addItem(QString("%1 (%2)").arg(device.second).arg(device.first));
    }
}

void settings::on_btn_refresh_clicked()
{
    ui->cbox_devices->setCurrentIndex(-1);
    populateDevices(ui->chbox_alldevices->isChecked());
}

void settings::on_chbox_alldevices_checkStateChanged(const Qt::CheckState &arg1)
{
    ui->cbox_devices->clear();
    populateDevices(arg1 != Qt::Unchecked);
}

void settings::on_btn_applyProfilePreset_clicked()
{
    const int deviceIndex = ui->cbox_devices->currentIndex();
    if (deviceIndex < 0 || deviceIndex >= devicesConnected.size()) {
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("No device selected."));
        return;
    }
    const QString devicePath = devicesConnected[deviceIndex].first;

    struct ProfileValues {
        int  pollingRate;
        int  keyRespTime;
        bool angleSnap;
        bool rippleControl;
        int  colorMode;
        int  sleepTime;
        int  deepSleepTime;
    };

    static const ProfileValues kProfiles[] = {
        { 0, 10, false, false, 0,  1,  5 },
        { 2,  8, true,  true,  0,  5, 15 },
        { 3,  4, true,  true,  -1, 10, 30 },
    };

    const int presetIndex = ui->cbox_profilesPresets->currentIndex();
    if (presetIndex <= 0) {
        QMessageBox::information(this, QStringLiteral("Profiles"), QStringLiteral("Select a profile preset to apply."));
        return;
    }

    const ProfileValues &p = kProfiles[presetIndex - 1];

    const int colorMode = (p.colorMode >= 0) ? p.colorMode : 0;

    const int result = applySettingsFromUser(
        devicePath,
        colorMode,
        p.pollingRate,
        p.angleSnap,
        p.keyRespTime,
        p.sleepTime,
        p.deepSleepTime,
        p.rippleControl);

    if (result != 0) {
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("Failed to apply profile (code %1).").arg(result));
        return;
    }

    QSettings qSettings(kSettingsOrg, kSettingsApp);
    qSettings.setValue(QStringLiteral("pollingRate"),   p.pollingRate);
    qSettings.setValue(QStringLiteral("keyRespTime"),   p.keyRespTime);
    qSettings.setValue(QStringLiteral("angleSnap"),     p.angleSnap);
    qSettings.setValue(QStringLiteral("rippleControl"), p.rippleControl);
    qSettings.setValue(QStringLiteral("sleepTime"),     p.sleepTime);
    qSettings.setValue(QStringLiteral("deepSleepTime"), p.deepSleepTime);
    if (p.colorMode >= 0)
        qSettings.setValue(QStringLiteral("colorMode"), p.colorMode);
    qSettings.sync();

    emit profileApplied();

    QMessageBox::information(this, QStringLiteral("Success"),
        QStringLiteral("Profile \"%1\" applied to %2.")
            .arg(ui->cbox_profilesPresets->currentText(), devicePath));
}


void settings::on_buttonBox_accepted()
{
    const int index = ui->cbox_devices->currentIndex();
    if (index < 0 || index >= devicesConnected.size()) {
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Please select a device before confirming."));
        return;
    }

    const QString devicePath = devicesConnected[index].first;

    QSettings qSettings(kSettingsOrg, kSettingsApp);
    qSettings.setValue(QStringLiteral("devicePath"), devicePath);
    qSettings.setValue(QStringLiteral("allDevices"), ui->chbox_alldevices->isChecked());
    qSettings.sync();

    emit deviceSelected(devicePath);

    accept();
}

