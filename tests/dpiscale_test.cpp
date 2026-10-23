#include "../dpiscale.h"
#include <cmath>
#include <cstdio>

static int g_failures = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_failures; } \
} while (0)

#define EXPECT_CLOSE(a, b, tol, msg) do { \
    const double _a = (a), _b = (b); \
    if (std::abs(_a - _b) > (tol)) { \
        std::printf("  FAIL: %s (%g vs %g)\n", msg, _a, _b); \
        ++g_failures; \
    } \
} while (0)

int main()
{
    std::printf("--- dpiToPos ---\n");
    {
        EXPECT(dpiscale::dpiToPos(50) == 0.0, "50 -> 0.0");
        EXPECT(dpiscale::dpiToPos(26000) == 1.0, "26000 -> 1.0");
        const double p1 = dpiscale::dpiToPos(400);
        const double p2 = dpiscale::dpiToPos(1000);
        const double p3 = dpiscale::dpiToPos(12000);
        EXPECT(p1 > 0.0 && p1 < p2 && p2 < 0.5 && p3 < 1.0,
            "log scale: defaults well-spaced in (0,1)");
        EXPECT(dpiscale::dpiToPos(0) == 0.0, "below-min clamps to 0");
        EXPECT(dpiscale::dpiToPos(30000) == 1.0, "above-max clamps to 1");
    }

    std::printf("--- posToDpi ---\n");
    {
        EXPECT(dpiscale::posToDpi(0.0, false) == 50, "0 -> 50");
        EXPECT(dpiscale::posToDpi(1.0, false) == 26000, "1 -> 26000");
        EXPECT(dpiscale::posToDpi(-0.5, false) == 50, "negative clamps to 50");
        EXPECT(dpiscale::posToDpi(2.0, false) == 26000, ">1 clamps to 26000");

        const int sample[6] = {800, 1600, 2400, 3200, 5000, 22000};
        for (int v : sample) {
            const double pos = dpiscale::dpiToPos(v);
            EXPECT_CLOSE(dpiscale::posToDpi(pos, false), v, 1.0,
                "log round-trip within 1 DPI");
        }
        EXPECT(dpiscale::posToDpi(0.5, true) % 50 == 0,
            "nearestStep returns a multiple of 50");
        EXPECT(dpiscale::posToDpi(0.123456789, true) % 50 == 0,
            "nearestStep at arbitrary pos is a multiple of 50");
    }

    std::printf("--- roundToStep ---\n");
    {
        EXPECT(dpiscale::roundToStep(999) == 1000, "999 -> 1000");
        EXPECT(dpiscale::roundToStep(12551) == 12550, "12551 -> 12550");
        EXPECT(dpiscale::roundToStep(50) == 50, "50 -> 50");
        EXPECT(dpiscale::roundToStep(26000) == 26000, "26000 -> 26000");
        EXPECT(dpiscale::roundToStep(49) == 50, "49 -> 50");
        EXPECT(dpiscale::roundToStep(26010) == 26000, "26010 -> 26000");
    }

    std::printf("--- clampOrdered ---\n");
    {
        int a[6] = {800, 1600, 2400, 3200, 5000, 22000};
        EXPECT(!dpiscale::clampOrdered(a), "already ordered -> false");

        int b[6] = {800, 800, 2400, 3200, 5000, 22000};
        EXPECT(dpiscale::clampOrdered(b), "duplicate -> changed");
        EXPECT(b[1] == 850, "duplicate resolved up by one step (850)");

        int c[6] = {0, 1000, 99999, 3200, 5000, 22000};
        EXPECT(dpiscale::clampOrdered(c), "out-of-range -> changed");
        EXPECT(c[0] == 50 && c[2] == 26000, "out-of-range values clamped");

        int d[6] = {5000, 4000, 3000, 2000, 1000, 800};
        EXPECT(dpiscale::clampOrdered(d), "descending -> changed");
        for (int i = 1; i < 6; ++i)
            EXPECT(d[i] > d[i - 1], "forced strictly ascending");

        int e[6] = {26000, 26000, 26000, 26000, 26000, 26000};
        dpiscale::clampOrdered(e);
        EXPECT(e[5] == 26000, "ceiling keeps max (duplicates allowed at max)");
    }

    if (g_failures) {
        std::printf("\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll dpiscale tests PASSED\n");
    return 0;
}