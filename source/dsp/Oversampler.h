#pragma once

#include "DspMath.h"

namespace iem1176 {

// Linear interpolation across `factor` evaluation points, boxcar-averaged back
// down. Oversampling is present. Alias-rejection figures are intentionally
// not claimed.
class Oversampler {
 public:
  void prepare(int factor) { factor_ = snapOversampling(factor); }
  void reset() { prev_ = 0; }
  int factor() const { return factor_; }

  template <typename Fn>
  double processSample(double x, Fn&& fn) {
    const int n = factor_;
    double acc = 0;
    for (int k = 1; k <= n; ++k) {
      const double t = double(k) / double(n);
      const double s = prev_ + (x - prev_) * t;
      acc += fn(s);
    }
    prev_ = x;
    return acc / double(n);
  }

 private:
  int factor_ = 8;
  double prev_ = 0;
};

}  // namespace iem1176
