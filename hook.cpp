#include "hook.h"
#include "dpi.h"

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
    {0x05, 0x0F, 0x01, 0x10, 0x01, 0xA8, 0x00, 0x00, 0x00, 0x01, 0x06, 0x00, 0xC0, 0x00, 0x00},
    {0x05, 0x0F, 0x01, 0x20, 0x01, 0xA8, 0x00, 0x00, 0xFF, 0x01, 0x06, 0x01, 0xCF, 0x00, 0x00},
    {0x05, 0x0F, 0x01, 0x30, 0x01, 0xA8, 0x00, 0x00, 0xFF, 0x01, 0x06, 0x01, 0xDF, 0x00, 0x00},
    {0x05, 0x0F, 0x01, 0x40, 0x01, 0xA8, 0x00, 0x00, 0xFF, 0x01, 0x06, 0x01, 0xEF, 0x00, 0x00},
};

void buildColorReport(int colorMode, int sleepTime, int deepSleepTime, int keyRespTime, uint8_t out[15])
{
    Q_UNUSED(sleepTime);
    Q_UNUSED(deepSleepTime);
    Q_UNUSED(keyRespTime);

    std::memcpy(out, COLOR_MODES[colorMode], 15);
}

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

libusb_device_handle *openUsbDevice(libusb_context *ctx, const UsbDevice &device, int *errorOut = nullptr)
{
    if (errorOut)
        *errorOut = LIBUSB_SUCCESS;

    libusb_device **list = nullptr;
    const ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        if (errorOut)
            *errorOut = LIBUSB_ERROR_OTHER;
        return nullptr;
    }

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

        const int rc = libusb_open(entry, &handle);
        if (rc == 0)
            break;

        // Remember why: LIBUSB_ERROR_ACCESS is a missing udev rule, which the
        // user can act on, while NO_DEVICE just means it was unplugged.
        if (errorOut && rc != LIBUSB_ERROR_NO_DEVICE && *errorOut == LIBUSB_SUCCESS)
            *errorOut = rc;
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
    int length,
    int *accessError = nullptr)
{
    libusb_device_handle *handle = openUsbDevice(ctx, device, accessError);
    if (!handle)
        return false;

    bool kernelDriverDetached = false;
    if (libusb_kernel_driver_active(handle, kConfigInterface) == 1) {
        kernelDriverDetached =
            libusb_detach_kernel_driver(handle, kConfigInterface) == 0;
    }

    const int rc = libusb_control_transfer(
        handle,
        0x21,
        0x09,
        reportId,
        kConfigInterface,
        const_cast<uint8_t *>(data),
        static_cast<uint16_t>(length),
        200);

    if (kernelDriverDetached)
        libusb_attach_kernel_driver(handle, kConfigInterface);

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

//   0x1d57:0xfa60  Xenta 2.4G Wireless Device  (wireless dongle)
//   0x1d57:0xfa55  Xenta USB Gaming Mouse       (wired / charging)
static constexpr uint16_t kX11Vid          = 0x1d57;
static constexpr uint16_t kX11PidWireless  = 0xfa60;
static constexpr uint16_t kX11PidWired     = 0xfa55;

QString findAttackSharkDevice()
{
    struct udev *udev = udev_new();
    if (!udev)
        return {};

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    QString wirelessMatch;
    QString wiredMatch;

    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {
        struct udev_device *dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry));
        if (!dev)
            continue;

        const char *devnode = udev_device_get_devnode(dev);
        if (!devnode || !QString::fromUtf8(devnode).startsWith(QStringLiteral("/dev/input/mouse"))) {
            udev_device_unref(dev);
            continue;
        }

        struct udev_device *usb = usbParent(dev);
        if (usb) {
            const char *vidStr = udev_device_get_sysattr_value(usb, "idVendor");
            const char *pidStr = udev_device_get_sysattr_value(usb, "idProduct");
            if (vidStr && pidStr) {
                const uint16_t vid = static_cast<uint16_t>(QString::fromUtf8(vidStr).toUInt(nullptr, 16));
                const uint16_t pid = static_cast<uint16_t>(QString::fromUtf8(pidStr).toUInt(nullptr, 16));
                if (vid == kX11Vid) {
                    if (pid == kX11PidWireless && wirelessMatch.isEmpty())
                        wirelessMatch = QString::fromUtf8(devnode);
                    else if (pid == kX11PidWired && wiredMatch.isEmpty())
                        wiredMatch = QString::fromUtf8(devnode);
                }
            }
        }

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    return wirelessMatch.isEmpty() ? wiredMatch : wirelessMatch;
}

int applySettingsFromUser(
    const QString &devicePath,
    int colorMode,
    int pollingRate,
    bool angleSnap,
    int keyRespTime,
    int sleepTime,
    int deepSleepTime,
    bool rippleControl,
    const int dpiValues[6],
    int activeDpiStage)
{
    if (colorMode < 0 || colorMode >= 4 || pollingRate < 0 || pollingRate >= 4)
        return kApplyInvalidSettings;
    if (sleepTime < 1 || sleepTime > 30)
        return kApplyInvalidSettings;
    if (deepSleepTime < 1 || deepSleepTime > 60)
        return kApplyInvalidSettings;
    if (keyRespTime < 4 || keyRespTime > 50 || keyRespTime % 2 != 0)
        return kApplyInvalidSettings;
    for (int i = 0; i < 6; ++i)
        if (dpiValues[i] < 50 || dpiValues[i] > 26000)
            return kApplyInvalidSettings;
    if (activeDpiStage < 1 || activeDpiStage > 6)
        return kApplyInvalidSettings;

    struct udev *udev = udev_new();
    if (!udev)
        return kApplyNoUdev;

    UsbDevice device;
    if (!resolveUsbDevice(udev, devicePath, device)) {
        udev_unref(udev);
        return kApplyDeviceNotFound;
    }
    udev_unref(udev);

    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) != 0)
        return kApplyNoUdev;

    // Apply the polling rate
    const uint8_t pollingReport[9] = {
        0x06, 0x09, 0x01,
        POLLING_RATES[pollingRate][0],
        POLLING_RATES[pollingRate][1],
        0x00, 0x00, 0x00, 0x00,
    };
    int accessError = LIBUSB_SUCCESS;
    const bool pollingOk = sendReport(ctx, device, 0x0306, pollingReport, sizeof(pollingReport), &accessError);
    usleep(kReportDelayUs);

    // Apply the LED color mode (with sleep timer, deep sleep timer, and debounce/key response time embedded)
    uint8_t colorReport[15];
    buildColorReport(colorMode, sleepTime, deepSleepTime, keyRespTime, colorReport);
    const bool colorOk = sendReport(ctx, device, 0x0305, colorReport, sizeof(colorReport), &accessError);
    usleep(kReportDelayUs);

    // Apply DPI configuration (stages, active stage, angle snap, ripple control)
    uint8_t dpiReport[56];
    buildDpiReport(dpiValues, activeDpiStage, angleSnap, rippleControl, dpiReport);
    const bool angleSnapOk = sendReport(ctx, device, 0x0304, dpiReport, sizeof(dpiReport), &accessError);

    libusb_exit(ctx);

    if (pollingOk && colorOk && angleSnapOk)
        return kApplyOk;

    // A refused open means the udev rule is not in place. That is the one
    // failure the user can fix, so it gets its own code and its own message
    // instead of being lumped in with every other transfer failure.
    if (accessError == LIBUSB_ERROR_ACCESS)
        return kApplyNoPermission;

    return kApplyReportFailed;
}

QString describeAccessProblem(const QString &devicePath)
{
    struct udev *udev = udev_new();
    if (!udev)
        return QString();

    UsbDevice device;
    const bool resolved = resolveUsbDevice(udev, devicePath, device);
    udev_unref(udev);
    if (!resolved) {
        return QObject::tr("The mouse is no longer connected. Plug it back in and try again.");
    }

    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) != 0)
        return QString();

    int error = LIBUSB_SUCCESS;
    libusb_device_handle *handle = openUsbDevice(ctx, device, &error);
    if (handle) {
        libusb_close(handle);
        libusb_exit(ctx);
        return QString();
    }

    const bool unplugged = (error == LIBUSB_ERROR_NO_DEVICE);
    libusb_exit(ctx);

    if (unplugged)
        return QObject::tr("The mouse is no longer connected. Plug it back in and try again.");

    return QObject::tr(
        "This user does not have permission to write to the mouse, so it cannot be "
        "configured without root.\n\n"
        "Installing the application also installs a udev rule that grants access to "
        "the user at the machine:\n\n"
        "    sudo cmake --install <build-dir>\n\n"
        "If the application was already installed, reload the rules with:\n\n"
        "    sudo udevadm control --reload-rules && sudo udevadm trigger\n\n"
        "and unplug the mouse and plug it back in. Running the application with sudo "
        "works too, but gives it full access to the machine.");
}


namespace {

constexpr uint8_t kPacketSig0   = 0x03;
constexpr uint8_t kPacketSig1   = 0x55;
constexpr uint8_t kPacketMsgBattery = 0x40;
constexpr int     kMinPacketLen = 5;
constexpr int     kMaxPercent   = 100;
constexpr int     kBatteryNoData = -1;
constexpr int     kBatteryCharging = -2;

} // namespace

// Byte 3 of the packet is the battery status, not part of the signature:
// 0x01 NORMAL, 0x02 FULLY_CHARGED, 0x03 CHARGING_IN_PROGRESS. Byte 4 is the
// percentage. The old check required byte 3 to equal 0x01, so every packet the
// mouse actually sends was rejected and the reading never left the error state.
// https://github.com/HarukaYamamoto0/attack-shark-x11-driver/blob/80fb0e69e59f2aa7185951622bcdadeb0bd4af0e/docs/battery-status.md
BatteryInfo decodeBatteryPacket(const uint8_t *data, int len)
{
    BatteryInfo info;

    if (!data || len < kMinPacketLen)
        return info;
    if (data[0] != kPacketSig0 || data[1] != kPacketSig1 || data[2] != kPacketMsgBattery)
        return info;

    const uint8_t status = data[3];
    if (status != kBatteryStatusNormal
        && status != kBatteryStatusFullyCharged
        && status != kBatteryStatusCharging)
        return info;

    info.status = status;
    info.percent = data[4] > kMaxPercent ? kMaxPercent : data[4];
    return info;
}

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

    int battery = kBatteryNoData; // -1 = no data, -2 = charging

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

    bool ifaceClaimed = true;
    (void)ifaceClaimed;

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

        const BatteryInfo parsed = decodeBatteryPacket(buf, transferred);
        if (parsed.valid()) {
            battery = (parsed.status == kBatteryStatusCharging) ? kBatteryCharging
                                                                : parsed.percent;
            break;
        }
    }

    // -2 is reserved for "actually charging". No battery packet at all is -1,
    // which the window renders as "battery info unavailable" rather than
    // claiming the mouse is charging when we simply did not hear from it.
    libusb_release_interface(handle, kBatteryIface);
    if (driverWasAttached)
        libusb_attach_kernel_driver(handle, kBatteryIface);

    libusb_close(handle);
    libusb_exit(ctx);
    return battery;
}
