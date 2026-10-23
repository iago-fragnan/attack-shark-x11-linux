#include "systemtray.h"
#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QString>
#include <QSystemTrayIcon>

systemtray::systemtray(const QIcon &icon, QWidget *window, QObject *parent)
    : QObject(parent)
    , m_tray(new QSystemTrayIcon(icon, this))
    , m_menu(new QMenu(window))
{
    QAction *open = m_menu->addAction(QStringLiteral("Open"));
    QAction *quit = m_menu->addAction(QStringLiteral("Quit"));

    connect(open, &QAction::triggered, this, &systemtray::restoreRequested);
    connect(quit, &QAction::triggered, this, &systemtray::quitRequested);
    connect(m_tray, &QSystemTrayIcon::activated, this, &systemtray::onActivated);

    m_tray->setToolTip(QStringLiteral("Attack Shark X11"));
    m_tray->setContextMenu(m_menu);
    m_tray->show();
}

void systemtray::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
        emit restoreRequested();
}
