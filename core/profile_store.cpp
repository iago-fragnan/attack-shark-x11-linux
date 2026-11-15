#include "core/profile_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace attackshark::profiles {

namespace {
constexpr int kDefaultDpiStages[kDpiStagesCount] = {400, 800, 1600, 3200, 5000};

QString profilesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/AttackShark/profiles");
}

QString profilePath(const QString &name)
{
    return QStringLiteral("%1/%2.ini").arg(profilesDir(), name);
}
} // namespace

MouseConfig readConfig(const QSettings &qSettings)
{
    MouseConfig cfg;
    for (int i = 0; i < kDpiStagesCount; ++i) {
        cfg.dpiStages[i].dpi = qSettings.value(QStringLiteral("dpiStage%1").arg(i + 1), kDefaultDpiStages[i]).toInt();
    }
    cfg.activeDpiStage    = std::clamp(qSettings.value(QStringLiteral("activeDpiStage"), 2).toInt(), 0, kDpiStagesCount - 1);
    cfg.colorMode         = static_cast<ColorMode>(qSettings.value(QStringLiteral("colorMode"), static_cast<int>(ColorMode::Static)).toInt());
    cfg.ledRed            = static_cast<uint8_t>(qSettings.value(QStringLiteral("ledRed"), 255).toUInt());
    cfg.ledGreen          = static_cast<uint8_t>(qSettings.value(QStringLiteral("ledGreen"), 0).toUInt());
    cfg.ledBlue           = static_cast<uint8_t>(qSettings.value(QStringLiteral("ledBlue"), 0).toUInt());
    cfg.pollingRate       = static_cast<PollingRate>(qSettings.value(QStringLiteral("pollingRate"), 3).toInt());
    cfg.angleSnap         = qSettings.value(QStringLiteral("angleSnap"), false).toBool();
    cfg.rippleControl     = qSettings.value(QStringLiteral("rippleControl"), false).toBool();
    cfg.sleepTimeMin      = qSettings.value(QStringLiteral("sleepTime"), 5).toInt();
    cfg.deepSleepTimeMin  = qSettings.value(QStringLiteral("deepSleepTime"), 10).toInt();
    cfg.keyResponseTimeMs = qSettings.value(QStringLiteral("keyRespTime"), 8).toInt();
    return cfg;
}

void writeConfig(QSettings &qSettings, const MouseConfig &cfg)
{
    for (int i = 0; i < kDpiStagesCount; ++i) {
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
}

QString sanitizeName(const QString &name)
{
    static const QRegularExpression invalidChars(QStringLiteral("[\\\\/:*?\"<>|.]"));
    QString clean = name;
    return clean.remove(invalidChars).simplified();
}

QStringList list()
{
    QStringList names;
    const QFileInfoList files = QDir(profilesDir()).entryInfoList({QStringLiteral("*.ini")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : files)
        names << file.completeBaseName();
    return names;
}

bool exists(const QString &name)
{
    return QFile::exists(profilePath(name));
}

bool save(const QString &name, const MouseConfig &cfg)
{
    if (!QDir().mkpath(profilesDir()))
        return false;

    QSettings file(profilePath(name), QSettings::IniFormat);
    writeConfig(file, cfg);
    file.sync();
    return file.status() == QSettings::NoError;
}

MouseConfig load(const QString &name)
{
    return readConfig(QSettings(profilePath(name), QSettings::IniFormat));
}

bool remove(const QString &name)
{
    return QFile::remove(profilePath(name));
}

} // namespace attackshark::profiles
