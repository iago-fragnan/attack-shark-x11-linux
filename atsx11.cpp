#include "atsx11.h"
#include "./ui_atsx11.h"
#include <QMessageBox>
#include "hook.h"

#include <algorithm>


atsx11::atsx11(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::atsx11)
{
    ui->setupUi(this);
    devicesConnected = getDevices(false);
    for (const auto& device : devicesConnected) {
        ui->cbox_devices->addItem(QString("%1 (%2)").arg(device.second).arg(device.first));
    }
    ui->lbl_batteryinfo->setEnabled(false);
    ui->pbar_batteryinfo->setEnabled(false);
}

atsx11::~atsx11()
{
    delete ui;
}


void atsx11::on_btn_apply_clicked()
{
    QString device = ui->cbox_devices->currentText();
    if(device == ""){
        ui->lbl_debug->setText("<html><head/><body><p><span style='color:red;'>Error: device not found</span></p></body></html>");
        QMessageBox::warning(this, "Error", "Device not selected.");
        return;
    }
    int color = ui->cbox_colormodes->currentIndex();
    int prate = ui->cbox_pollingrate->currentIndex();

    QList<QPair<QString, QString>> deviceSelected;
    for (const auto& pair : devicesConnected) {
        if (QString("%1 (%2)").arg(pair.second).arg(pair.first) == device) {
            deviceSelected.append(pair);
        }
    }


    ui->lbl_debug->setText("<html><head/><body><p><span style='color:green;'>Sucessfully applied</span></p></body></html>");
    QMessageBox::information(this, "Debug", "Device: " + deviceSelected.first().first + "\nColor: "+ QString::number(color) +"\nPolling Rate: " + QString::number(prate));
}


void atsx11::on_cbox_devices_currentIndexChanged(int index)
{
    ui->lbl_debug->setText("Battery information update");
    ui->lbl_batteryinfo->setEnabled(true);
    ui->pbar_batteryinfo->setEnabled(true);
    ui->pbar_batteryinfo->setValue(getBatteryInfo());
}


void atsx11::on_btn_refresh_clicked()
{
    ui->cbox_devices->setCurrentIndex(-1);
    ui->cbox_devices->clear();

    ui->lbl_batteryinfo->setEnabled(false);
    ui->pbar_batteryinfo->setEnabled(false);
    ui->pbar_batteryinfo->setValue(-1);

    if(ui->chbox_alldevices->isChecked()){
        devicesConnected = getDevices(true);
    } else {
        devicesConnected = getDevices(false);
    }
    int quantity = 0;
    for (const auto& device : devicesConnected) {
        QString displayText = QString("%1 (%2)").arg(device.second).arg(device.first);
        ui->cbox_devices->addItem(displayText);
        quantity++;
    }

    ui->lbl_debug->setText("Refreshed devices list (" + QString::number(quantity) + " devices).");

}

// arg1
// 0 = Unchecked
// 2 = Checked (?)
void atsx11::on_chbox_alldevices_stateChanged(int arg1)
{
    if(arg1 == 0){
        devicesConnected = getDevices(false);
        ui->lbl_debug->setText("Listing only mice devices.");
    } else {
        devicesConnected = getDevices(true);
        ui->lbl_debug->setText("Listing all devices.");
    }
    ui->cbox_devices->clear();
    int quantity = 0;
    for (const auto& device : devicesConnected) {
        QString displayText = QString("%1 (%2)").arg(device.second).arg(device.first);
        ui->cbox_devices->addItem(displayText);
        quantity++;
    }
    ui->lbl_debug->setText("Listing all devices (" + QString::number(quantity) + ").");

}

