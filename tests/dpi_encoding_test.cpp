#include "../dpi.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_failures; } \
} while(0)

int main() {
    std::printf("--- convertDpiToBytes ---\n");
    {
        DpiEncode e;
        e = convertDpiToBytes(50);
        EXPECT(e.x == 0x01 && e.y == 0 && !e.isDouble, "50 -> 01/0");
        e = convertDpiToBytes(800);
        EXPECT(e.x == 0x12 && e.y == 0 && !e.isDouble, "800 -> 12/0");
        e = convertDpiToBytes(1600);
        EXPECT(e.x == 0x25 && e.y == 0 && !e.isDouble, "1600 -> 25/0");
        e = convertDpiToBytes(2400);
        EXPECT(e.x == 0x38 && e.y == 0 && !e.isDouble, "2400 -> 38/0");
        e = convertDpiToBytes(3200);
        EXPECT(e.x == 0x4b && e.y == 0 && !e.isDouble, "3200 -> 4b/0");
        e = convertDpiToBytes(5000);
        EXPECT(e.x == 0x75 && e.y == 0 && !e.isDouble, "5000 -> 75/0");
        e = convertDpiToBytes(6000);
        EXPECT(e.x == 0x46 && e.y == 1 && !e.isDouble, "6000 -> 46/1");
        e = convertDpiToBytes(10000);
        EXPECT(e.x == 0x75 && e.y == 1 && !e.isDouble, "10000 -> 75/1");
        e = convertDpiToBytes(10100);
        EXPECT(e.x == 0x76 && e.y == 0 && e.isDouble, "10100 -> 76/0 double");
        e = convertDpiToBytes(12000);
        EXPECT(e.x == 0x46 && e.y == 1 && e.isDouble, "12000 -> 46/1 double");
        e = convertDpiToBytes(20000);
        EXPECT(e.x == 0x75 && e.y == 1 && e.isDouble, "20000 -> 75/1 double");
        e = convertDpiToBytes(20100);
        EXPECT(e.x == 0xEB && e.y == 1 && e.isDouble, "20100 -> EB/1 double special");
        e = convertDpiToBytes(22000);
        EXPECT(e.x == 0xd0 && e.y == 0 && e.isDouble, "22000 -> d0/0 double");
        e = convertDpiToBytes(26000);
        EXPECT(e.x == 0xe4 && e.y == 0 && e.isDouble, "26000 -> e4/0 double");
        e = convertDpiToBytes(0);
        EXPECT(e.x == 0x01 && e.y == 0 && !e.isDouble, "0 clamped to 50");
        e = convertDpiToBytes(30000);
        EXPECT(e.x == 0xe4 && e.y == 0 && e.isDouble, "30000 clamped to 26000");
    }

    std::printf("--- buildDpiReport defaults ---\n");
    {
        const int dpi[6] = {800, 1600, 2400, 3200, 5000, 22000};
        uint8_t r[56] = {};
        buildDpiReport(dpi, 1, false, false, r);

        const uint8_t expected[56] = {
            0x04, 0x38, 0x01,                 // 0-2: header
            0x00, 0x00,                       // 3-4: angleSnap=0, ripple=0
            0x3f,                             // 5: active mask
            0x20, 0x00,                       // 6-7: double flag (bit5=stage6), triple=0
            0x12, 0x25, 0x38, 0x4b, 0x75, 0xd0, // 8-13: X stages
            0x00, 0x00, 0x00, 0x00, 0x00,    // 14-18
            0x00, 0x00, 0x00, 0x00, 0x00,    // 19-23
            0x01,                             // 24: active stage=1
            0xff, 0x00, 0x00, 0x00,          // 25-28: RGB stage1
            0xff, 0x00, 0x00, 0x00,          // 29-32: RGB stage2
            0xff, 0xff, 0xff, 0x00, 0x00,    // 33-37: RGB stage3
            0xff, 0xff, 0xff, 0x00,          // 38-41: RGB stage4
            0xff, 0xff, 0x40, 0x00,          // 42-45: RGB stage5
            0xff, 0xff, 0xff,                 // 46-48: RGB stage6
            0x02,                             // 49: DPI Active Indicator
            0x0f, 0x94,                       // 50-51: checksum = 0x0F94
            0x00, 0x00, 0x00, 0x00           // 52-55: padding
        };
        EXPECT(std::memcmp(r, expected, 56) == 0, "default report byte-for-byte");

        // Independent checksum verification
        uint16_t sum = 0;
        for (int i = 3; i <= 49; ++i)
            sum = static_cast<uint16_t>(sum + r[i]);
        EXPECT(r[50] == static_cast<uint8_t>(sum >> 8)
            && r[51] == static_cast<uint8_t>(sum & 0xFF),
            "checksum matches independent recompute");
    }

    std::printf("--- buildDpiReport with angle/ripple/active ---\n");
    {
        const int dpi[6] = {800, 1600, 2400, 3200, 5000, 22000};
        uint8_t r[56] = {};
        buildDpiReport(dpi, 3, true, true, r);

        EXPECT(r[3] == 0x01, "angleSnap on -> offset 3 = 0x01");
        EXPECT(r[4] == 0x01, "ripple on -> offset 4 = 0x01");
        EXPECT(r[24] == 0x03, "active stage 3 -> offset 24 = 0x03");

        uint16_t sum = 0;
        for (int i = 3; i <= 49; ++i)
            sum = static_cast<uint16_t>(sum + r[i]);
        EXPECT(r[50] == static_cast<uint8_t>(sum >> 8)
            && r[51] == static_cast<uint8_t>(sum & 0xFF),
            "checksum follows flag changes");
    }

    if (g_failures) {
        std::printf("\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll DPI encoding tests PASSED\n");
    return 0;
}
