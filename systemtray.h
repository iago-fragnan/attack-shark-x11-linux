#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QObject>
#include <QSystemTrayIcon>

class QIcon;
class QMenu;
class QWidget;

class systemtray : public QObject
{
    Q_OBJECT

public:
    explicit systemtray(const QIcon &icon, QWidget *window, QObject *parent = nullptr);

signals:
    void restoreRequested();
    void quitRequested();

private:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
};

#endif // SYSTEMTRAY_H
