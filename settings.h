#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

namespace Ui {
class settings;
}

class settings : public QDialog
{
    Q_OBJECT

public:
    explicit settings(QWidget *parent = nullptr);
    ~settings();

signals:
    void deviceSelected(const QString &devicePath);
    void profileApplied();

private slots:
    void on_btn_refresh_clicked();
    void on_chbox_alldevices_checkStateChanged(const Qt::CheckState &arg1);
    void on_btn_applyProfilePreset_clicked();
    void on_buttonBox_accepted();

private:
    void populateDevices(bool allDevices);

    Ui::settings *ui;
    QList<QPair<QString, QString>> devicesConnected;
};

#endif // SETTINGS_H
