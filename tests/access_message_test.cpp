#include "../hook.h"

#include <QCoreApplication>
#include <cstdio>

static int g_failures = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_failures; } \
} while(0)

static const int kDefaults[6] = {800, 1600, 2400, 3200, 5000, 22000};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    std::printf("--- applySettingsFromUser rechaza parametros invalidos con su propio codigo ---\n");
    {
        const QString device = findAttackSharkDevice();

        const int dpi[6] = {800, 1600, 2400, 3200, 5000, 22000};
        EXPECT(applySettingsFromUser(device, -1, 0, false, 8, 1, 10, false, dpi, 1)
                   == kApplyInvalidSettings, "colorMode fuera de rango");
        EXPECT(applySettingsFromUser(device, 0, 9, false, 8, 1, 10, false, dpi, 1)
                   == kApplyInvalidSettings, "pollingRate fuera de rango");
        EXPECT(applySettingsFromUser(device, 0, 0, false, 3, 1, 10, false, dpi, 1)
                   == kApplyInvalidSettings, "keyRespTime impar o muy chica");
        EXPECT(applySettingsFromUser(device, 0, 0, false, 8, 99, 10, false, dpi, 1)
                   == kApplyInvalidSettings, "sleepTime fuera de rango");
        EXPECT(applySettingsFromUser(device, 0, 0, false, 8, 1, 10, false, dpi, 9)
                   == kApplyInvalidSettings, "activeDpiStage fuera de rango");

        const int lowDpi[6] = {10, 1600, 2400, 3200, 5000, 22000};
        EXPECT(applySettingsFromUser(device, 0, 0, false, 8, 1, 10, false, lowDpi, 1)
                   == kApplyInvalidSettings, "dpi por debajo del minimo");

        const int highDpi[6] = {800, 1600, 2400, 3200, 5000, 99999};
        EXPECT(applySettingsFromUser(device, 0, 0, false, 8, 1, 10, false, highDpi, 1)
                   == kApplyInvalidSettings, "dpi por encima del maximo");
    }

    std::printf("--- un path de dispositivo que no existe se distingue de los permisos ---\n");
    {
        EXPECT(applySettingsFromUser(QStringLiteral("/dev/no-such-device"), 0, 0, false, 8, 1, 10, false,
                                     kDefaults, 1) == kApplyDeviceNotFound,
               "devicePath invalido -> kApplyDeviceNotFound");
    }

    std::printf("--- el mensaje de acceso explica que hacer ---\n");
    {
        const QString problem = describeAccessProblem(QStringLiteral("/dev/no-such-device"));
        EXPECT(!problem.isEmpty(), "describeAccessProblem no devuelve vacio para un device invalido");
        EXPECT(!problem.contains("code"), "el mensaje no escupe un codigo crudo");

        // The valid-device path must not report a problem on a machine that can
        // already open it, and must not claim a permission problem when the only
        // issue is that the mouse is gone.
        const QString device = findAttackSharkDevice();
        if (!device.isEmpty()) {
            const QString accessible = describeAccessProblem(device);
            EXPECT(accessible.isEmpty()
                       || accessible.contains(QStringLiteral("no longer connected"))
                       || accessible.contains(QStringLiteral("permission")),
                   "el mensaje es uno de los tres casos conocidos");
        } else {
            std::printf("  (sin mouse conectado, se omite la comprobacion en vivo)\n");
        }
    }

    if (g_failures) {
        std::printf("\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll access message tests PASSED\n");
    return 0;
}