#pragma once

#include <algorithm>
#include <cmath>

namespace iem1176 {

constexpr double kPi = 3.14159265358979323846;
constexpr double kAolDb = 26.0;
// 10^(26/20). Literal so hosts with different libm still share the same gain.
constexpr double kAol = 19.952623149688797;
constexpr double kTurns = 15.0;
constexpr double kTertiary = 0.5;
constexpr double kFeedbackDivider = 0.5;
// fb = betaPath * primary, subtracted at the AB input. UNVERIFIED vs a physical unit.
constexpr double kBetaPath = (kFeedbackDivider * kTertiary) / kTurns;
constexpr double kInvTurns = 1.0 / kTurns;
// Soft-clip knee of the open-loop AB stage. UNVERIFIED.
constexpr double kAbKnee = 48.0;

inline double dbToLin(double db) { return std::pow(10.0, db / 20.0); }

inline double linToDb(double lin) {
  return 20.0 * std::log10(std::max(lin, 1.0e-12));
}

inline double flushDenorm(double x) { return std::abs(x) < 1.0e-30 ? 0.0 : x; }

// Even-order shaper used by applyLn. Unity small-signal gain. UNVERIFIED curve.
inline double shapeLn(double x, double drive) {
  const double d = std::clamp(drive, 0.0, 1.0);
  const double bias = 0.2 * d;
  const double k = 1.0 + 2.0 * d;
  const double tb = std::tanh(bias);
  const double ss = k * (1.0 - tb * tb);
  const double y = std::tanh(k * x + bias) - tb;
  return ss > 1.0e-12 ? y / ss : x;
}

// Parallel blend. Drive adds even harmonics without hard-capping program level.
inline double applyLn(double x, double drive) {
  const double d = std::clamp(drive, 0.0, 1.0);
  if (d <= 1.0e-8) return x;
  return x + d * (shapeLn(x, 1.0) - x);
}

inline int snapOversampling(int factor) {
  const int allowed[] = {1, 2, 4, 8};
  int best = 1;
  int bestDist = 100;
  for (int a : allowed) {
    const int d = std::abs(a - factor);
    if (d < bestDist) {
      bestDist = d;
      best = a;
    }
  }
  return best;
}

}  // namespace iem1176
