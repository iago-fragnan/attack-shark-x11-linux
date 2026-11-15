#ifndef CORE_PROFILE_STORE_H
#define CORE_PROFILE_STORE_H

#include "core/types.h"

#include <QSettings>
#include <QString>
#include <QStringList>

namespace attackshark::profiles {

MouseConfig readConfig(const QSettings &qSettings);
void writeConfig(QSettings &qSettings, const MouseConfig &cfg);

QString sanitizeName(const QString &name);
QStringList list();
bool exists(const QString &name);
bool save(const QString &name, const MouseConfig &cfg);
MouseConfig load(const QString &name);
bool remove(const QString &name);

} // namespace attackshark::profiles

#endif // CORE_PROFILE_STORE_H

