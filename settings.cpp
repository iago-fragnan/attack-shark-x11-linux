#include "settings.h"
#include "ui_settings.h"
#include "core/profile_store.h"
#include "device/attackshark_device.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QStyle>

namespace {
constexpr auto kSettingsOrg = "AttackShark";
constexpr auto kSettingsApp = "X11";
constexpr int kBuiltinPresetCount = 4;

QString autostartFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/attackshark-x11.desktop");
}

bool setAutostart(bool enabled)
{
    const QString path = autostartFilePath();
    if (!enabled)
        return !QFile::exists(path) || QFile::remove(path);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    QTextStream(&file)
        << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=Attack Shark X11\n"
        << "Exec=" << QCoreApplication::applicationFilePath() << " --minimized\n"
        << "Icon=attackshark-x11\n"
        << "Terminal=false\n"
        << "X-GNOME-Autostart-enabled=true\n";
    return true;
}
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

    ui->chkbox_minimizeTray->setChecked(qSettings.value(QStringLiteral("minimizeToTray"), true).toBool());
    ui->chkbox_autostartup->setChecked(QFile::exists(autostartFilePath()));

    populateDevices(allDevices);
    populateProfiles();

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
    const bool hasDevice = deviceIndex >= 0 && deviceIndex < m_devicesConnected.size();

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
    } else if (presetIndex < kBuiltinPresetCount) {
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
    } else {
        cfg = attackshark::profiles::load(ui->cbox_profilesPresets->currentText());
    }

    QSettings qSettings(kSettingsOrg, kSettingsApp);
    attackshark::profiles::writeConfig(qSettings, cfg);
    if (hasDevice)
        qSettings.setValue(QStringLiteral("devicePath"), m_devicesConnected[deviceIndex].hidrawPath);
    qSettings.sync();

    emit profileApplied(ui->cbox_profilesPresets->currentText());
}

void settings::on_btn_saveProfile_clicked()
{
    bool accepted = false;
    const QString input = QInputDialog::getText(this, QStringLiteral("Save Profile"),
        QStringLiteral("Profile name:"), QLineEdit::Normal, QString(), &accepted);
    if (!accepted)
        return;

    const QString name = attackshark::profiles::sanitizeName(input);
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Invalid profile name."));
        return;
    }

    const bool isBuiltin = ui->cbox_profilesPresets->findText(name, Qt::MatchFixedString) >= 0
        && !attackshark::profiles::exists(name);
    if (isBuiltin) {
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("\"%1\" is a built-in profile name.").arg(name));
        return;
    }

    if (attackshark::profiles::exists(name)
        && QMessageBox::question(this, QStringLiteral("Save Profile"),
               QStringLiteral("Profile \"%1\" already exists. Overwrite?").arg(name)) != QMessageBox::Yes)
        return;

    const QSettings qSettings(kSettingsOrg, kSettingsApp);
    if (!attackshark::profiles::save(name, attackshark::profiles::readConfig(qSettings))) {
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("Could not save profile \"%1\".").arg(name));
        return;
    }

    populateProfiles(name);
}

void settings::on_btn_deleteProfile_clicked()
{
    const int index = ui->cbox_profilesPresets->currentIndex();
    if (index < kBuiltinPresetCount) {
        QMessageBox::information(this, QStringLiteral("Profiles"),
            QStringLiteral("Select a saved profile to delete."));
        return;
    }

    const QString name = ui->cbox_profilesPresets->currentText();
    if (QMessageBox::question(this, QStringLiteral("Delete Profile"),
            QStringLiteral("Delete profile \"%1\"?").arg(name)) != QMessageBox::Yes)
        return;

    if (!attackshark::profiles::remove(name))
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("Could not delete profile \"%1\".").arg(name));
    populateProfiles();
}

void settings::populateProfiles(const QString &selectName)
{
    while (ui->cbox_profilesPresets->count() > kBuiltinPresetCount)
        ui->cbox_profilesPresets->removeItem(kBuiltinPresetCount);

    ui->cbox_profilesPresets->addItems(attackshark::profiles::list());

    const int index = ui->cbox_profilesPresets->findText(selectName, Qt::MatchFixedString);
    ui->cbox_profilesPresets->setCurrentIndex(std::max(index, 0));
}

void settings::on_buttonBox_accepted()
{
    QSettings qSettings(kSettingsOrg, kSettingsApp);
    qSettings.setValue(QStringLiteral("minimizeToTray"), ui->chkbox_minimizeTray->isChecked());
    qSettings.setValue(QStringLiteral("allDevices"), ui->chbox_alldevices->isChecked());

    const int index = ui->cbox_devices->currentIndex();
    const bool hasDevice = index >= 0 && index < m_devicesConnected.size();
    if (hasDevice)
        qSettings.setValue(QStringLiteral("devicePath"), m_devicesConnected[index].hidrawPath);
    qSettings.sync();

    if (!setAutostart(ui->chkbox_autostartup->isChecked())) {
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("Could not update the autostart entry."));
    }

    emit minimizeToTrayChanged();
    if (hasDevice)
        emit deviceSelected(m_devicesConnected[index].hidrawPath);
    accept();
}
