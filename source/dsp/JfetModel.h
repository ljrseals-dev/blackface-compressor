#pragma once

#include "DspMath.h"

namespace iem1176 {

// Shunt divider: gain = Rds / (R5 + Rds), R5 = 27 kΩ.
// Vgs(off) and Idss windows below are the 2N5457 datasheet spreads this model
// is allowed to cite. The Rds-vs-gain law is a clamp fit, NOT a claim that the
// curve matches a 2N5457 outside these windows.
class JfetModel {
 public:
  static constexpr double kR5Ohms = 27000.0;
  static constexpr double kVgsOffMin = -6.0;
  static constexpr double kVgsOffMax = -0.5;
  static constexpr double kIdssMinA = 0.001;
  static constexpr double kIdssMaxA = 0.005;
  // Model clamps. UNVERIFIED as device measurements; they bound the divider.
  static constexpr double kRdsMinOhms = 400.0;
  static constexpr double kRdsMaxOhms = 1.0e8;

  void setTargetGain(double linearGain) {
    if (linearGain >= 0.999999) {
      rds_ = kRdsMaxOhms;
    } else {
      const double g = std::max(linearGain, 1.0e-8);
      rds_ = g * kR5Ohms / (1.0 - g);
      rds_ = std::clamp(rds_, kRdsMinOhms, kRdsMaxOhms);
    }
    gain_ = rds_ / (kR5Ohms + rds_);
  }

  double processSample(double x) const { return x * gain_; }
  double currentGain() const { return gain_; }
  double rdsOhms() const { return rds_; }

 private:
  double rds_ = kRdsMaxOhms;
  double gain_ = kRdsMaxOhms / (kR5Ohms + kRdsMaxOhms);
};

}  // namespace iem1176
