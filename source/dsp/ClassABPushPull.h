#pragma once

#include "DspMath.h"

namespace iem1176 {

// 26 dB open-loop push-pull. Symmetric soft clip => odd harmonics. UNVERIFIED knee.
class ClassABPushPull {
 public:
  static constexpr double kOpenLoopDb = kAolDb;

  double processOpenLoop(double x) const {
    const double y = x * kAol;
    return kAbKnee * std::tanh(y / kAbKnee);
  }
};

}  // namespace iem1176
