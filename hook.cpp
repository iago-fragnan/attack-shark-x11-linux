#include "hook.h"
#include <cstdint>
#include <qobject.h>

#include <libudev.h>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QList>
#include <QPair>
#include <QDebug>


x11mouse::x11mouse() {
    uint8_t POLLING_RATES[4][2] = {
        {0x08, 0xF7}, // 125 Hz (0)
        {0x04, 0xFB}, // 250 Hz (1)
        {0x02, 0xFD}, // 500 Hz (2)
        {0x01, 0xFE}, // 1000 Hz (3)
    };

    uint8_t COLOR_MODES[4][15] = {
        // Disabled (0)
        {0x05, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        // Breathing (1)
        {0x05, 0x0F, 0x01, 0x20, 0x01, 0xA8, 0x00, 0x00,
         0xFF, 0x01, 0x06, 0x01, 0xCF, 0x00, 0x00},

        // Neon (2)
        {0x05, 0x0F, 0x01, 0x30, 0x01, 0xA8, 0x00, 0x00,
         0xFF, 0x01, 0x06, 0x01, 0xDF, 0x00, 0x00},

        // Color Breathing (3)
        {0x05, 0x0F, 0x01, 0x40, 0x01, 0xA8, 0x00, 0x00,
         0xFF, 0x01, 0x06, 0x01, 0xEF, 0x00, 0x00},
        };

}


QList<QPair<QString, QString>> getDevices(bool allDevices)
{
    QList<QPair<QString, QString>> deviceList;

    struct udev *udev = udev_new();
    if (!udev)
        return deviceList;

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;

    udev_list_entry_foreach(entry, devices) {
        const char *path = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(udev, path);

        if (!dev)
            continue;

        const char *devnode = udev_device_get_devnode(dev);
        if (!devnode || (!allDevices && !QString::fromUtf8(devnode).startsWith("/dev/input/mouse"))) {
            udev_device_unref(dev);
            continue;
        }

        QString deviceName;
        const char *nameProp = udev_device_get_property_value(dev, "NAME");
        
        if (nameProp) {
            deviceName = QString::fromUtf8(nameProp).remove('"');
        } else {
            QFile file(QString::fromUtf8(udev_device_get_syspath(dev)) + "/device/name");
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                deviceName = QTextStream(&file).readLine();
            }
        }

        if (deviceName.isEmpty())
            deviceName = QStringLiteral("Unknown");

        deviceList.append({QString::fromUtf8(devnode), deviceName});

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    return deviceList;
}

// sel_deviceID = pathName
// sel_colormode = ID of the array of the colormodee
// sel_prate = ID of the array of the polling rate
int setConfig(QString sel_deviceID, int sel_colormode, int sel_prate){

    return 0;
}

int getBatteryInfo(){
    return -1;
    // return rand() % 100 + 1;
}
