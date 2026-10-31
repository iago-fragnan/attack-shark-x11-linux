#include "atsx11.h"
#include "./ui_atsx11.h"
#include "settings.h"

#include <QCloseEvent>
#include <QColorDialog>
#include <QIcon>
#include <QMessageBox>
#include <QSettings>
#include <algorithm>

namespace {
constexpr auto kSettingsOrg = "AttackShark";
constexpr auto kSettingsApp = "X11";
constexpr int kDefaultDpiStages[attackshark::kDpiStagesCount] = {400, 800, 1600, 3200, 5000};
constexpr int kAutoApplyDebounceMs = 350;
} // namespace

atsx11::atsx11(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::atsx11)
    , m_device(this)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(QStringLiteral(":/images/assets/mouse.png")));
    ui->btn_settings->setIcon(QIcon(QStringLiteral(":/images/assets/wgear.png")));
    ui->lbl_isCharging->setPixmap(QPixmap());

    m_autoApplyTimer = new QTimer(this);
    m_autoApplyTimer->setSingleShot(true);
    connect(m_autoApplyTimer, &QTimer::timeout, this, &atsx11::performAutoApply);

    m_dpiLedRestoreTimer = new QTimer(this);
    m_dpiLedRestoreTimer->setSingleShot(true);
    connect(m_dpiLedRestoreTimer, &QTimer::timeout, this, [this]() {
        const attackshark::MouseConfig cfg = collectUiConfig();
        if (cfg.colorMode != attackshark::ColorMode::StaticDpi) {
            m_device.sendColorReport(cfg);
        }
    });

    m_stageRadios = {
        ui->radio_dpiStage1,
        ui->radio_dpiStage2,
        ui->radio_dpiStage3,
        ui->radio_dpiStage4,
        ui->radio_dpiStage5
    };
    m_stageSpins = {
        ui->spin_dpiStage1,
        ui->spin_dpiStage2,
        ui->spin_dpiStage3,
        ui->spin_dpiStage4,
        ui->spin_dpiStage5
    };

    for (int i = 0; i < attackshark::kDpiStagesCount; ++i) {
        connect(m_stageRadios[i], &QRadioButton::toggled, this, [this](bool checked) {
            if (checked)
                onUserSettingChanged(true);
        });
        connect(m_stageSpins[i], &QSpinBox::valueChanged, this, [this]() {
            onUserSettingChanged(false);
        });
    }

    connect(ui->cbox_pollingrate, &QComboBox::currentIndexChanged, this, [this]() {
        onUserSettingChanged(true);
    });
    connect(ui->cbox_colormodes, &QComboBox::currentIndexChanged, this, [this]() {
        updateLedColorButton();
        onUserSettingChanged(true);
    });
    connect(ui->chbox_anglesnap, &QCheckBox::toggled, this, [this]() {
        onUserSettingChanged(true);
    });
    connect(ui->chbox_rippleCTRL, &QCheckBox::toggled, this, [this]() {
        onUserSettingChanged(true);
    });

    connect(&m_device, &attackshark::AttackSharkDevice::batteryStatusChanged,
            this, &atsx11::onBatteryStatusUpdated);
    connect(&m_device, &attackshark::AttackSharkDevice::connectionModeChanged,
            this, &atsx11::onConnectionModeChanged);
    connect(&m_device, &attackshark::AttackSharkDevice::activeDpiStageChanged,
            this, &atsx11::onHardwareDpiStageChanged);
    connect(&m_device, &attackshark::AttackSharkDevice::deviceDisconnected,
            this, &atsx11::onDeviceDisconnected);

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

void atsx11::onHardwareDpiStageChanged(int stageIndex)
{
    if (stageIndex < 0 || stageIndex >= attackshark::kDpiStagesCount)
        return;

    m_isHardwareUpdating = true;
    m_stageRadios[stageIndex]->setChecked(true);
    m_isHardwareUpdating = false;

    updateInfoLabels();
    saveSettings();

    const int activeDpi = m_stageSpins[stageIndex]->value();
    ui->lbl_debug->setText(
        QStringLiteral("<span style='color:#51cf66; font-weight:bold;'>\u26A1 DPI Switch: Stage %1 (%2 DPI)</span>")
            .arg(stageIndex + 1)
            .arg(activeDpi));

    // When the physical DPI button is pressed on the mouse, the firmware autonomously
    // turns the LED to the stage indicator color.
    // If the configured mode is StaticDpi, that's already what the user wants.
    // Otherwise (Breathing, Neon, Static, Disabled, BreathingDpi), we let the user
    // see the stage indicator for 1 second, then restore their configured lighting effect.
    const auto mode = static_cast<attackshark::ColorMode>(ui->cbox_colormodes->currentIndex());
    if (mode != attackshark::ColorMode::StaticDpi) {
        m_dpiLedRestoreTimer->start(1000);
    }
}

void atsx11::onUserSettingChanged(bool immediate)
{
    if (m_isLoadingSettings || m_isHardwareUpdating)
        return;

    updateInfoLabels();

    if (!m_device.isConnected())
        return;

    if (immediate) {
        if (m_autoApplyTimer->isActive())
            m_autoApplyTimer->stop();
        performAutoApply();
    } else {
        m_autoApplyTimer->start(kAutoApplyDebounceMs);
    }
}

void atsx11::performAutoApply()
{
    if (m_isLoadingSettings || m_isHardwareUpdating || !m_device.isConnected())
        return;

    const attackshark::MouseConfig cfg = collectUiConfig();
    QString errMsg;
    const bool ok = m_device.applyConfiguration(cfg, &errMsg);

    if (ok) {
        saveSettings();
        ui->lbl_debug->setText(
            QStringLiteral("<span style='color:#51cf66;'>Applied to mouse (Stage %1 — %2 DPI)</span>")
                .arg(cfg.activeDpiStage + 1)
                .arg(cfg.activeDpiValue()));
    } else {
        ui->lbl_debug->setText(
            QStringLiteral("<span style='color:#ff6b6b;'>Auto-apply error: %1</span>").arg(errMsg));
    }
}

attackshark::MouseConfig atsx11::collectUiConfig() const
{
    attackshark::MouseConfig cfg;
    for (int i = 0; i < attackshark::kDpiStagesCount; ++i) {
        cfg.dpiStages[i].dpi = m_stageSpins[i]->value();
        if (m_stageRadios[i]->isChecked())
            cfg.activeDpiStage = i;
    }

    cfg.pollingRate       = static_cast<attackshark::PollingRate>(ui->cbox_pollingrate->currentIndex());
    cfg.colorMode         = static_cast<attackshark::ColorMode>(ui->cbox_colormodes->currentIndex());
    cfg.ledRed            = static_cast<uint8_t>(m_ledColor.red());
    cfg.ledGreen          = static_cast<uint8_t>(m_ledColor.green());
    cfg.ledBlue           = static_cast<uint8_t>(m_ledColor.blue());
    cfg.angleSnap         = ui->chbox_anglesnap->isChecked();
    cfg.rippleControl     = ui->chbox_rippleCTRL->isChecked();
    cfg.keyResponseTimeMs = ui->sld_keyresptime->value();
    cfg.sleepTimeMin      = ui->sldr_sleeptime->value();
    cfg.deepSleepTimeMin  = ui->sldr_deepSleepTime->value();

    if (cfg.keyResponseTimeMs % 2 != 0)
        cfg.keyResponseTimeMs += 1;

    return cfg;
}

void atsx11::applyConfigToUi(const attackshark::MouseConfig &cfg)
{
    m_isLoadingSettings = true;

    for (int i = 0; i < attackshark::kDpiStagesCount; ++i) {
        m_stageSpins[i]->setValue(cfg.dpiStages[i].clampedDpi());
        m_stageRadios[i]->setChecked(i == cfg.activeDpiStage);
    }

    const int pollIdx = std::clamp(static_cast<int>(cfg.pollingRate), 0, ui->cbox_pollingrate->count() - 1);
    ui->cbox_pollingrate->setCurrentIndex(pollIdx);

    const int colorIdx = std::clamp(static_cast<int>(cfg.colorMode), 0, ui->cbox_colormodes->count() - 1);
    ui->cbox_colormodes->setCurrentIndex(colorIdx);
    m_ledColor = QColor(cfg.ledRed, cfg.ledGreen, cfg.ledBlue);
    updateLedColorButton();

    ui->chbox_anglesnap->setChecked(cfg.angleSnap);
    ui->chbox_rippleCTRL->setChecked(cfg.rippleControl);

    ui->sldr_sleeptime->setValue(cfg.sleepTimeMin);
    ui->lbl_sleepTimerValue->setText(QStringLiteral("Sleep: %1 min").arg(cfg.sleepTimeMin));

    ui->sldr_deepSleepTime->setValue(cfg.deepSleepTimeMin);
    ui->lbl_deepsleeptimeValue->setText(QStringLiteral("Deep Sleep: %1 min").arg(cfg.deepSleepTimeMin));

    ui->sld_keyresptime->setValue(cfg.keyResponseTimeMs);
    ui->lbl_kreptimeValueDisplay->setText(QStringLiteral("%1 ms").arg(cfg.keyResponseTimeMs));

    m_isLoadingSettings = false;
    updateInfoLabels();
}

void atsx11::on_btn_ledColor_clicked()
{
    const QColor chosen = QColorDialog::getColor(m_ledColor, this, tr("Select LED Color"));
    if (chosen.isValid()) {
        m_ledColor = chosen;
        updateLedColorButton();
        onUserSettingChanged(true);
    }
}

void atsx11::updateLedColorButton()
{
    const auto mode = static_cast<attackshark::ColorMode>(ui->cbox_colormodes->currentIndex());
    const bool supportsColor = (mode == attackshark::ColorMode::Static || mode == attackshark::ColorMode::Breathing);
    ui->btn_ledColor->setEnabled(supportsColor);

    if (supportsColor) {
        const QString hex = m_ledColor.name().toUpper();
        const double luminance = (0.299 * m_ledColor.red() + 0.587 * m_ledColor.green() + 0.114 * m_ledColor.blue()) / 255.0;
        const QString textColor = (luminance > 0.5) ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF");
        ui->btn_ledColor->setStyleSheet(
            QStringLiteral("QPushButton { background-color: %1; color: %2; font-weight: bold; border: 1px solid #555; border-radius: 4px; padding: 4px; }")
                .arg(hex, textColor));
        ui->btn_ledColor->setText(hex);
        ui->btn_ledColor->setToolTip(tr("Current color: %1. Click to change.").arg(hex));
    } else if (mode == attackshark::ColorMode::StaticDpi || mode == attackshark::ColorMode::BreathingDpi) {
        ui->btn_ledColor->setStyleSheet(QString());
        ui->btn_ledColor->setText(tr("DPI Stage Color"));
        ui->btn_ledColor->setToolTip(tr("Color is automatically determined by the active DPI stage."));
    } else {
        ui->btn_ledColor->setStyleSheet(QString());
        ui->btn_ledColor->setText(tr("N/A (%1)").arg(ui->cbox_colormodes->currentText()));
        ui->btn_ledColor->setToolTip(tr("Color selection is available only in Static and Breathing modes."));
    }
}

void atsx11::updateInfoLabels()
{
    const attackshark::MouseConfig cfg = collectUiConfig();
    ui->lbl_activeDpiValue->setText(
        QStringLiteral("%1 DPI (S%2)")
            .arg(cfg.activeDpiValue())
            .arg(cfg.activeDpiStage + 1));
    ui->lbl_PLRValue->setText(ui->cbox_pollingrate->currentText());
    ui->lbl_infoLEDValue->setText(ui->cbox_colormodes->currentText());
    ui->info_ripplectrlValue->setText(
        QStringLiteral("AS: %1 | RC: %2")
            .arg(cfg.angleSnap ? QStringLiteral("On") : QStringLiteral("Off"),
                 cfg.rippleControl ? QStringLiteral("On") : QStringLiteral("Off")));
}

void atsx11::connectToDevice(const QString &devicePath)
{
    if (!m_device.openDevice(devicePath)) {
        ui->btn_apply->setEnabled(false);
        ui->lbl_batteryinfo->setEnabled(false);
        ui->pbar_batteryinfo->setEnabled(false);
        ui->lbl_isCharging->setEnabled(false);
        ui->lbl_debug->setText(
            QStringLiteral("<span style='color:#ff6b6b;'>Device not found or permission denied (%1)</span>")
                .arg(devicePath));
        return;
    }

    ui->btn_apply->setEnabled(true);
    ui->lbl_batteryinfo->setEnabled(true);
    ui->pbar_batteryinfo->setEnabled(true);

    const attackshark::BatteryStatus initial = m_device.pollBatteryOnce(300);
    if (initial.isValid()) {
        onBatteryStatusUpdated(initial);
    }
}

void atsx11::onConnectionModeChanged(bool isWired, const QString &hidrawPath)
{
    ui->btn_apply->setEnabled(true);
    ui->lbl_batteryinfo->setEnabled(true);
    ui->pbar_batteryinfo->setEnabled(true);

    if (isWired) {
        ui->lbl_debug->setText(
            QStringLiteral("<span style='color:#51cf66;'>Connected via USB Cable (Charging) — %1</span>")
                .arg(hidrawPath));
    } else {
        ui->lbl_debug->setText(
            QStringLiteral("Connected via 2.4G Wireless — %1").arg(hidrawPath));
    }
}

void atsx11::loadSettings()
{
    QSettings qSettings(kSettingsOrg, kSettingsApp);

    attackshark::MouseConfig cfg;
    for (int i = 0; i < attackshark::kDpiStagesCount; ++i) {
        cfg.dpiStages[i].dpi = qSettings.value(
            QStringLiteral("dpiStage%1").arg(i + 1),
            kDefaultDpiStages[i]).toInt();
    }
    cfg.activeDpiStage    = std::clamp(qSettings.value(QStringLiteral("activeDpiStage"), 2).toInt(), 0, attackshark::kDpiStagesCount - 1);
    cfg.colorMode         = static_cast<attackshark::ColorMode>(qSettings.value(QStringLiteral("colorMode"), static_cast<int>(attackshark::ColorMode::Static)).toInt());
    cfg.ledRed            = static_cast<uint8_t>(qSettings.value(QStringLiteral("ledRed"), 255).toUInt());
    cfg.ledGreen          = static_cast<uint8_t>(qSettings.value(QStringLiteral("ledGreen"), 0).toUInt());
    cfg.ledBlue           = static_cast<uint8_t>(qSettings.value(QStringLiteral("ledBlue"), 0).toUInt());
    cfg.pollingRate       = static_cast<attackshark::PollingRate>(qSettings.value(QStringLiteral("pollingRate"), 3).toInt());
    cfg.angleSnap         = qSettings.value(QStringLiteral("angleSnap"), false).toBool();
    cfg.rippleControl     = qSettings.value(QStringLiteral("rippleControl"), false).toBool();
    cfg.sleepTimeMin      = qSettings.value(QStringLiteral("sleepTime"), 5).toInt();
    cfg.deepSleepTimeMin  = qSettings.value(QStringLiteral("deepSleepTime"), 10).toInt();
    cfg.keyResponseTimeMs = qSettings.value(QStringLiteral("keyRespTime"), 8).toInt();

    applyConfigToUi(cfg);

    QString devPath = qSettings.value(QStringLiteral("devicePath")).toString();
    const QString resolved = attackshark::AttackSharkDevice::resolveConfigHidrawPath(devPath);
    if (!resolved.isEmpty()) {
        devPath = resolved;
    } else {
        devPath = attackshark::AttackSharkDevice::findDefaultDevicePath();
    }

    if (!devPath.isEmpty()) {
        qSettings.setValue(QStringLiteral("devicePath"), devPath);
        connectToDevice(devPath);
    } else {
        ui->lbl_debug->setText(QStringLiteral("No Attack Shark X11 detected. Click 'Device & Profiles' to select."));
    }
}

void atsx11::saveSettings()
{
    QSettings qSettings(kSettingsOrg, kSettingsApp);
    const attackshark::MouseConfig cfg = collectUiConfig();

    for (int i = 0; i < attackshark::kDpiStagesCount; ++i) {
        qSettings.setValue(QStringLiteral("dpiStage%1").arg(i + 1), cfg.dpiStages[i].dpi);
    }
    qSettings.setValue(QStringLiteral("activeDpiStage"), cfg.activeDpiStage);
    qSettings.setValue(QStringLiteral("colorMode"),      static_cast<int>(cfg.colorMode));
    qSettings.setValue(QStringLiteral("ledRed"),         cfg.ledRed);
    qSettings.setValue(QStringLiteral("ledGreen"),       cfg.ledGreen);
    qSettings.setValue(QStringLiteral("ledBlue"),        cfg.ledBlue);
    qSettings.setValue(QStringLiteral("pollingRate"),    static_cast<int>(cfg.pollingRate));
    qSettings.setValue(QStringLiteral("angleSnap"),      cfg.angleSnap);
    qSettings.setValue(QStringLiteral("rippleControl"),  cfg.rippleControl);
    qSettings.setValue(QStringLiteral("sleepTime"),      cfg.sleepTimeMin);
    qSettings.setValue(QStringLiteral("deepSleepTime"),  cfg.deepSleepTimeMin);
    qSettings.setValue(QStringLiteral("keyRespTime"),    cfg.keyResponseTimeMs);
    qSettings.sync();
}

void atsx11::on_btn_apply_clicked()
{
    if (!m_device.isConnected()) {
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("No device connected. Open 'Device & Profiles' to select your mouse."));
        return;
    }

    const attackshark::MouseConfig cfg = collectUiConfig();
    ui->btn_apply->setEnabled(false);
    ui->lbl_debug->setText(QStringLiteral("Flashing configuration to mouse\u2026"));
    QCoreApplication::processEvents();

    QString errMsg;
    const bool ok = m_device.applyConfiguration(cfg, &errMsg);
    ui->btn_apply->setEnabled(true);

    if (!ok) {
        ui->lbl_debug->setText(
            QStringLiteral("<span style='color:#ff6b6b;'>Error: %1</span>").arg(errMsg));
        QMessageBox::warning(this, QStringLiteral("Configuration Error"),
            QStringLiteral("Failed to apply settings to %1:\n%2")
                .arg(m_device.currentHidrawPath(), errMsg));
        return;
    }

    saveSettings();
    updateInfoLabels();
    ui->lbl_debug->setText(
        QStringLiteral("<span style='color:#51cf66;'>Settings flashed to %1 (Active: %2 DPI)</span>")
            .arg(m_device.currentHidrawPath())
            .arg(cfg.activeDpiValue()));
}

void atsx11::onBatteryStatusUpdated(const attackshark::BatteryStatus &status)
{
    if (status.isCharging) {
        ui->lbl_isCharging->setEnabled(true);
        ui->lbl_isCharging->setPixmap(QPixmap(QStringLiteral(":/images/assets/lightning.png")));
    } else {
        ui->lbl_isCharging->setEnabled(false);
        ui->lbl_isCharging->setPixmap(QPixmap());
    }

    if (status.percentage >= 0 && status.percentage <= 100) {
        ui->pbar_batteryinfo->setValue(status.percentage);
        if (status.isCharging) {
            ui->pbar_batteryinfo->setFormat(QStringLiteral("%p% (Charging)"));
            ui->lbl_batteryinfo->setText(
                QStringLiteral("Charging (%1%)").arg(status.percentage));
            ui->lbl_debug->setText(
                QStringLiteral("<span style='color:#51cf66;'>Connected (%1) — Charging (%2%)</span>")
                    .arg(m_device.currentHidrawPath())
                    .arg(status.percentage));
        } else {
            ui->pbar_batteryinfo->setFormat(QStringLiteral("%p%"));
            ui->lbl_batteryinfo->setText(
                QStringLiteral("Battery (%1%)").arg(status.percentage));
            ui->lbl_debug->setText(
                QStringLiteral("Connected (%1) — Battery: %2%")
                    .arg(m_device.currentHidrawPath())
                    .arg(status.percentage));
        }
    } else if (status.isCharging) {
        ui->pbar_batteryinfo->setValue(100);
        ui->pbar_batteryinfo->setFormat(QStringLiteral("Charging (USB)"));
        ui->lbl_batteryinfo->setText(QStringLiteral("Charging via USB \u26A1"));
        ui->lbl_debug->setText(
            QStringLiteral("<span style='color:#51cf66;'>Connected (%1) — Charging via USB</span>")
                .arg(m_device.currentHidrawPath()));
    } else {
        ui->pbar_batteryinfo->setValue(0);
        ui->pbar_batteryinfo->setFormat(QStringLiteral("Waiting\u2026"));
        ui->lbl_batteryinfo->setText(QStringLiteral("Battery Status (Waiting\u2026)"));
    }
}

void atsx11::onDeviceDisconnected()
{
    ui->btn_apply->setEnabled(false);
    ui->lbl_batteryinfo->setEnabled(false);
    ui->pbar_batteryinfo->setEnabled(false);
    ui->lbl_isCharging->setEnabled(false);
    ui->lbl_debug->setText(QStringLiteral("<span style='color:#ffa94d;'>Device disconnected</span>"));
}

void atsx11::on_sld_keyresptime_valueChanged(int value)
{
    if (value % 2 != 0) {
        value += 1;
        ui->sld_keyresptime->blockSignals(true);
        ui->sld_keyresptime->setValue(value);
        ui->sld_keyresptime->blockSignals(false);
    }
    ui->lbl_kreptimeValueDisplay->setText(QStringLiteral("%1 ms").arg(value));
    onUserSettingChanged(false);
}

void atsx11::on_sldr_sleeptime_valueChanged(int value)
{
    ui->lbl_sleepTimerValue->setText(QStringLiteral("Sleep: %1 min").arg(value));
    onUserSettingChanged(false);
}

void atsx11::on_sldr_deepSleepTime_valueChanged(int value)
{
    ui->lbl_deepsleeptimeValue->setText(QStringLiteral("Deep Sleep: %1 min").arg(value));
    onUserSettingChanged(false);
}

void atsx11::on_btn_settings_clicked()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new settings(this);
        connect(m_settingsDialog, &settings::deviceSelected,
                this, &atsx11::onSettingsDeviceSelected);
        connect(m_settingsDialog, &settings::profileApplied,
                this, &atsx11::reloadSettingsUi);
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void atsx11::onSettingsDeviceSelected(const QString &devicePath)
{
    connectToDevice(devicePath);
}

void atsx11::reloadSettingsUi()
{
    loadSettings();
    performAutoApply();
}
