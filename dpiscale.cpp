#include "dpiscale.h"
#include <algorithm>
#include <cmath>

namespace dpiscale {

double dpiToPos(int dpi)
{
    const int v = std::clamp(dpi, kMinDpi, kMaxDpi);
    return (std::log(static_cast<double>(v)) - std::log(static_cast<double>(kMinDpi)))
         / (std::log(static_cast<double>(kMaxDpi)) - std::log(static_cast<double>(kMinDpi)));
}

int posToDpi(double pos, bool nearestStep)
{
    const double c = std::clamp(pos, 0.0, 1.0);
    const double raw = std::exp(c * (std::log(static_cast<double>(kMaxDpi))
                                     - std::log(static_cast<double>(kMinDpi)))
                                + std::log(static_cast<double>(kMinDpi)));
    int v = static_cast<int>(std::llround(raw));
    v = std::clamp(v, kMinDpi, kMaxDpi);
    return nearestStep ? roundToStep(v) : v;
}

int roundToStep(int dpi)
{
    const double stepped = std::round(static_cast<double>(dpi) / kStep) * kStep;
    return std::clamp(static_cast<int>(std::llround(stepped)), kMinDpi, kMaxDpi);
}

bool clampOrdered(int dpi[kNumStages])
{
    bool changed = false;
    for (int i = 0; i < kNumStages; ++i) {
        const int v = std::clamp(dpi[i], kMinDpi, kMaxDpi);
        if (v != dpi[i]) {
            dpi[i] = v;
            changed = true;
        }
    }
    for (int i = 1; i < kNumStages; ++i) {
        if (dpi[i] <= dpi[i - 1]) {
            const int v = std::min(dpi[i - 1] + kStep, kMaxDpi);
            if (v != dpi[i]) {
                dpi[i] = v;
                changed = true;
            }
        }
    }
    return changed;
}

} // namespace dpiscale