#ifndef DPISCALE_H
#define DPISCALE_H

namespace dpiscale {

constexpr int kMinDpi = 50;
constexpr int kMaxDpi = 26000;
constexpr int kStep = 50;
constexpr int kNumStages = 6;

double dpiToPos(int dpi);
int posToDpi(double pos, bool nearestStep);
int roundToStep(int dpi);
bool clampOrdered(int dpi[kNumStages]);

} // namespace dpiscale

#endif // DPISCALE_H