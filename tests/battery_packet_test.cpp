#include "../hook.h"
#include <cstdio>
#include <cstring>

static int g_failures = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_failures; } \
} while(0)

static void expectPacket(const uint8_t *data, int len, int status, int percent, const char *msg)
{
    const BatteryInfo b = decodeBatteryPacket(data, len);
    EXPECT(b.status == status && b.percent == percent, msg);
}

int main()
{
    std::printf("--- paquete real capturado del Attack Shark X11 ---\n");
    {
        // 30 paquetes seguidos, todos identicos, con el mouse en el dock:
        // status 0x02 FULLY_CHARGED, 0x64 = 100%.
        const uint8_t pkt[] = {0x03, 0x55, 0x40, 0x02, 0x64};
        expectPacket(pkt, 5, kBatteryStatusFullyCharged, 100,
                     "03 55 40 02 64 -> FULLY_CHARGED, 100%");
    }

    std::printf("--- status normal ---\n");
    {
        const uint8_t pkt[] = {0x03, 0x55, 0x40, 0x01, 0x50};
        expectPacket(pkt, 5, kBatteryStatusNormal, 80,
                     "03 55 40 01 50 -> NORMAL, 80%");
    }

    std::printf("--- cargando ---\n");
    {
        const uint8_t pkt[] = {0x03, 0x55, 0x40, 0x03, 0x32};
        expectPacket(pkt, 5, kBatteryStatusCharging, 50,
                     "03 55 40 03 32 -> CHARGING, 50%");
    }

    std::printf("--- porcentaje fuera de rango ---\n");
    {
        const uint8_t high[] = {0x03, 0x55, 0x40, 0x01, 0xFA};
        expectPacket(high, 5, kBatteryStatusNormal, 100,
                     "0xFA = 250 se recorta a 100");
    }

    std::printf("--- status invalido se rechaza ---\n");
    {
        const uint8_t pkt[] = {0x03, 0x55, 0x40, 0x00, 0x50};
        expectPacket(pkt, 5, kBatteryStatusUnknown, -1, "status 0 -> sin datos");
    }
    {
        const uint8_t pkt[] = {0x03, 0x55, 0x40, 0x04, 0x50};
        expectPacket(pkt, 5, kBatteryStatusUnknown, -1, "status 4 -> sin datos");
    }

    std::printf("--- otro tipo de mensaje en el mismo endpoint ---\n");
    {
        // El mouse alterna este paquete en el endpoint de bateria.
        const uint8_t other[] = {0x03, 0x55, 0x50, 0x00, 0x06};
        expectPacket(other, 5, kBatteryStatusUnknown, -1,
                     "msgtype 0x50 no es de bateria");
    }

    std::printf("--- paquetes cortados o con firma mala ---\n");
    {
        const uint8_t shortPkt[] = {0x03, 0x55, 0x40, 0x01};
        expectPacket(shortPkt, 4, kBatteryStatusUnknown, -1, "len<5 -> sin datos");
    }
    {
        const uint8_t badSig[] = {0x11, 0x22, 0x33, 0x44, 0x55};
        expectPacket(badSig, 5, kBatteryStatusUnknown, -1, "firma mala -> sin datos");
    }
    {
        const uint8_t zeros[7] = {};
        expectPacket(zeros, 7, kBatteryStatusUnknown, -1, "buffer cero -> sin datos");
    }

    EXPECT(!decodeBatteryPacket(nullptr, 5).valid(), "nullptr -> sin datos");

    if (g_failures) {
        std::printf("\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll battery packet tests PASSED\n");
    return 0;
}