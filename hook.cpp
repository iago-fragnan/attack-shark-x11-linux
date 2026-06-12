#include "hook.h"

#include <cstdint>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>

#include <libudev.h>
#include <libusb-1.0/libusb.h>
#include <QFile>
#include <QTextStream>
#include <QDebug>

namespace {

constexpr int kConfigInterface = 2;
constexpr useconds_t kReportDelayUs = 300000;

struct UsbDevice {
    uint16_t vid = 0;
    uint16_t pid = 0;
    uint8_t bus = 0;
    uint8_t address = 0;
};

const uint8_t POLLING_RATES[4][2] = {
    {0x08, 0xF7}, // 125 Hz
    {0x04, 0xFB}, // 250 Hz
    {0x02, 0xFD}, // 500 Hz
    {0x01, 0xFE}, // 1000 Hz
};

const uint8_t COLOR_MODES[4][15] = {
    {0x05, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x05, 0x0F, 0x01, 0x20, 0x01, 0xA8, 0x00, 0x00, 0xFF, 0x01, 0x06, 0x01, 0xCF, 0x00, 0x00},
    {0x05, 0x0F, 0x01, 0x30, 0x01, 0xA8, 0x00, 0x00, 0xFF, 0x01, 0x06, 0x01, 0xDF, 0x00, 0x00},
    {0x05, 0x0F, 0x01, 0x40, 0x01, 0xA8, 0x00, 0x00, 0xFF, 0x01, 0x06, 0x01, 0xEF, 0x00, 0x00},
};

struct udev_device *usbParent(struct udev_device *dev)
{
    while (dev) {
        const char *subsystem = udev_device_get_subsystem(dev);
        const char *devtype = udev_device_get_devtype(dev);
        if (subsystem && devtype
            && std::strcmp(subsystem, "usb") == 0
            && std::strcmp(devtype, "usb_device") == 0) {
            return dev;
        }
        dev = udev_device_get_parent(dev);
    }
    return nullptr;
}

bool resolveUsbDevice(struct udev *udev, const QString &inputPath, UsbDevice &device)
{
    struct stat st {};
    if (stat(inputPath.toUtf8().constData(), &st) != 0)
        return false;

    struct udev_device *input = udev_device_new_from_devnum(udev, 'c', st.st_rdev);
    if (!input)
        return false;

    struct udev_device *usb = usbParent(input);
    if (!usb) {
        udev_device_unref(input);
        return false;
    }

    const char *vid = udev_device_get_sysattr_value(usb, "idVendor");
    const char *pid = udev_device_get_sysattr_value(usb, "idProduct");
    const char *bus = udev_device_get_sysattr_value(usb, "busnum");
    const char *addr = udev_device_get_sysattr_value(usb, "devnum");
    const bool ok = vid && pid && bus && addr;

    if (ok) {
        device.vid = static_cast<uint16_t>(QString::fromUtf8(vid).toUInt(nullptr, 16));
        device.pid = static_cast<uint16_t>(QString::fromUtf8(pid).toUInt(nullptr, 16));
        device.bus = static_cast<uint8_t>(QString::fromUtf8(bus).toUInt());
        device.address = static_cast<uint8_t>(QString::fromUtf8(addr).toUInt());
    }

    udev_device_unref(input);
    return ok;
}

libusb_device_handle *openUsbDevice(libusb_context *ctx, const UsbDevice &device)
{
    libusb_device **list = nullptr;
    const ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0)
        return nullptr;

    libusb_device_handle *handle = nullptr;

    for (ssize_t i = 0; i < count; ++i) {
        libusb_device *entry = list[i];
        if (libusb_get_bus_number(entry) != device.bus
            || libusb_get_device_address(entry) != device.address) {
            continue;
        }

        libusb_device_descriptor desc {};
        if (libusb_get_device_descriptor(entry, &desc) != 0)
            continue;
        if (desc.idVendor != device.vid || desc.idProduct != device.pid)
            continue;

        if (libusb_open(entry, &handle) == 0)
            break;

        handle = nullptr;
    }

    libusb_free_device_list(list, 1);
    return handle;
}

bool sendReport(
    libusb_context *ctx,
    const UsbDevice &device,
    uint16_t reportId,
    const uint8_t *data,
    int length)
{
    libusb_device_handle *handle = openUsbDevice(ctx, device);
    if (!handle)
        return false;

    if (libusb_kernel_driver_active(handle, kConfigInterface) == 1)
        libusb_detach_kernel_driver(handle, kConfigInterface);

    const int rc = libusb_control_transfer(
        handle,
        0x21,
        0x09,
        reportId,
        kConfigInterface,
        const_cast<uint8_t *>(data),
        static_cast<uint16_t>(length),
        200);

    libusb_close(handle);
    return rc == length || rc == LIBUSB_ERROR_TIMEOUT;
}

} // namespace

QList<QPair<QString, QString>> getDevices(bool allDevices)
{
    QList<QPair<QString, QString>> devices;

    struct udev *udev = udev_new();
    if (!udev)
        return devices;

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {
        struct udev_device *dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry));
        if (!dev)
            continue;

        const char *devnode = udev_device_get_devnode(dev);
        if (!devnode || (!allDevices && !QString::fromUtf8(devnode).startsWith("/dev/input/mouse"))) {
            udev_device_unref(dev);
            continue;
        }

        QString name;
        if (const char *nameProp = udev_device_get_property_value(dev, "NAME")) {
            name = QString::fromUtf8(nameProp).remove('"');
        } else {
            QFile file(QString::fromUtf8(udev_device_get_syspath(dev)) + "/device/name");
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
                name = QTextStream(&file).readLine();
        }

        devices.append({QString::fromUtf8(devnode), name.isEmpty() ? QStringLiteral("Unknown") : name});
        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    return devices;
}

int setConfig(QString devicePath, int colorMode, int pollingRate)
{
    if (colorMode < 0 || colorMode >= 4 || pollingRate < 0 || pollingRate >= 4)
        return -1;

    struct udev *udev = udev_new();
    if (!udev)
        return -2;

    UsbDevice device;
    if (!resolveUsbDevice(udev, devicePath, device)) {
        udev_unref(udev);
        return -3;
    }
    udev_unref(udev);

    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) != 0)
        return -2;

    const uint8_t pollingReport[9] = {
        0x06, 0x09, 0x01,
        POLLING_RATES[pollingRate][0],
        POLLING_RATES[pollingRate][1],
        0x00, 0x00, 0x00, 0x00,
    };

    const bool pollingOk = sendReport(ctx, device, 0x0306, pollingReport, sizeof(pollingReport));
    usleep(kReportDelayUs);
    const bool colorOk = sendReport(ctx, device, 0x0305, COLOR_MODES[colorMode], sizeof(COLOR_MODES[colorMode]));

    libusb_exit(ctx);
    return pollingOk && colorOk ? 0 : -5;
}


// https://github.com/HarukaYamamoto0/attack-shark-x11-driver/blob/80fb0e69e59f2aa7185951622bcdadeb0bd4af0e/docs/battery-status.md
int getBatteryInfo(const QString &devicePath)
{
    constexpr uint8_t  kBatteryIface    = 2;
    constexpr uint8_t  kBatteryEndpoint = 0x83;
    constexpr int      kTransferTimeout = 500;  // ms
    constexpr int      kBufSize         = 64;
    constexpr uint8_t  kSig0 = 0x03, kSig1 = 0x55, kSig2 = 0x40, kSig3 = 0x01;
    constexpr int      kMinPacketLen    = 5;

    struct udev *udev = udev_new();
    if (!udev)
        return -1;

    UsbDevice device;
    if (!resolveUsbDevice(udev, devicePath, device)) {
        udev_unref(udev);
        return -1;
    }
    udev_unref(udev);

    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) != 0)
        return -1;

    libusb_device_handle *handle = openUsbDevice(ctx, device);
    if (!handle) {
        libusb_exit(ctx);
        return -1;
    }

    int battery = -1;

    bool driverWasAttached = false;
    if (libusb_kernel_driver_active(handle, kBatteryIface) == 1) {
        if (libusb_detach_kernel_driver(handle, kBatteryIface) == 0)
            driverWasAttached = true;
        else {
            libusb_close(handle);
            libusb_exit(ctx);
            return -1;
        }
    }

    if (libusb_claim_interface(handle, kBatteryIface) != 0) {
        if (driverWasAttached)
            libusb_attach_kernel_driver(handle, kBatteryIface);
        libusb_close(handle);
        libusb_exit(ctx);
        return -1;
    }
    constexpr int kMaxRetries = 5;
    for (int attempt = 0; attempt < kMaxRetries && battery < 0; ++attempt) {
        uint8_t buf[kBufSize] = {};
        int transferred = 0;
        const int rc = libusb_interrupt_transfer(
            handle,
            kBatteryEndpoint,
            buf,
            kBufSize,
            &transferred,
            kTransferTimeout);

        if (rc != 0 && rc != LIBUSB_ERROR_TIMEOUT)
            break;

        if (transferred >= kMinPacketLen &&
            buf[0] == kSig0 &&
            buf[1] == kSig1 &&
            buf[2] == kSig2 &&
            buf[3] == kSig3)
        {
            battery = static_cast<int>(buf[4]);
        }
    }

    libusb_release_interface(handle, kBatteryIface);
    if (driverWasAttached)
        libusb_attach_kernel_driver(handle, kBatteryIface);

    libusb_close(handle);
    libusb_exit(ctx);
    return battery;
}
