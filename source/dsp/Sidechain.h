#pragma once

#include "DspMath.h"

namespace iem1176 {

// Peak detector + feed-back gain computer. Runs at the oversampled rate.
// CR-001: setHpfFrequency — one-pole HPF on the detector input. 0 disables.
class Sidechain {
 public:
  void prepare(double sampleRate) {
    sr_ = std::max(sampleRate, 1.0);
    recompute();
  }

  void reset() {
    env_ = 0;
    hpfX1_ = 0;
    hpfY1_ = 0;
    gain_ = 1;
    grDb_ = 0;
  }

  void setAttackSeconds(double seconds) {
    const double s = std::max(seconds, 1.0e-7);
    if (s == atkSec_) return;
    atkSec_ = s;
    atkRaw_ = 1.0 - std::exp(-1.0 / (atkSec_ * sr_));
    updateAtk();
  }

  // Blackface change: true ratios under feedback detection.
  // The detector reads the signal AFTER the gain cell, so a plain (1/R - 1) law
  // only reaches an effective ratio of about 2:1. In feedback mode the gain
  // computer uses GR_dB = (R - 1) * over_post_dB, which settles at an input/output
  // ratio of R, and the attack coefficient is limited so the loop stays stable.
  void setFeedbackMode(bool fb) {
    if (fb == fb_) return;
    fb_ = fb;
    updateAtk();
  }

  void setReleaseSeconds(double seconds) {
    const double s = std::max(seconds, 1.0e-5);
    if (s == relSec_) return;
    relSec_ = s;
    relCoef_ = 1.0 - std::exp(-1.0 / (relSec_ * sr_));
  }

  void setThresholdDb(double db) { threshDb_ = db; }
  void setRatio(double ratio) {
    ratio_ = std::max(ratio, 1.001);
    updateAtk();
  }

  void setHpfFrequency(double hz) {
    if (hz == hpfHz_) return;
    hpfHz_ = hz;
    if (hpfHz_ <= 0.0) {
      hpfA_ = 0;
      return;
    }
    hpfA_ = std::exp(-2.0 * kPi * hpfHz_ / sr_);
  }

  double hpfFrequency() const { return hpfHz_; }

  // Returns the linear FET target gain for this detector sample.
  double processSample(double detectorInput) {
    const double x = applyHpf(detectorInput);
    const double level = std::abs(x);
    const double coef = level > env_ ? atkCoef_ : relCoef_;
    env_ = flushDenorm(env_ + coef * (level - env_));
    const double thresh = dbToLin(threshDb_);
    double gr = 0.0;
    // Below (threshold - knee/2) nothing happens: skip the logarithm.
    if (env_ > thresh * kKneeFloor) {
      const double over = 20.0 * std::log10(env_ / thresh);
      const double slope = fb_ ? (ratio_ - 1.0) : (1.0 - 1.0 / ratio_);
      if (over >= 0.5 * kKneeDb) {
        gr = slope * over;
      } else if (over > -0.5 * kKneeDb) {
        const double t = over + 0.5 * kKneeDb;
        gr = slope * t * t / (2.0 * kKneeDb);
      }
    }
    gr = std::min(gr, 60.0);
    gain_ = gr > 0.0 ? dbToLin(-gr) : 1.0;
    grDb_ = gr;
    return gain_;
  }

  double envelope() const { return env_; }
  double gainReductionDb() const { return grDb_; }
  double currentGain() const { return gain_; }

  void setEnvelope(double v) { env_ = v; }
  double hpfX1() const { return hpfX1_; }
  double hpfY1() const { return hpfY1_; }
  void setHpfState(double x1, double y1) {
    hpfX1_ = x1;
    hpfY1_ = y1;
  }

 private:
  void updateAtk() {
    // Stability bound for the discrete feedback loop: c * (1 + slope) < 2.
    atkCoef_ = fb_ ? std::min(atkRaw_, 1.2 / ratio_) : atkRaw_;
  }

  void recompute() {
    const double a = atkSec_;
    const double r = relSec_;
    const double h = hpfHz_;
    atkSec_ = -1;
    relSec_ = -1;
    hpfHz_ = -1;
    setAttackSeconds(a);
    setReleaseSeconds(r);
    setHpfFrequency(h);
  }

  double applyHpf(double x) {
    if (hpfHz_ <= 0.0) return x;
    const double y = hpfA_ * (hpfY1_ + x - hpfX1_);
    hpfX1_ = x;
    hpfY1_ = flushDenorm(y);
    return hpfY1_;
  }

  double sr_ = 48000.0;
  double atkSec_ = 800.0e-6;
  double relSec_ = 0.3;
  double threshDb_ = -20.0;
  double ratio_ = 4.0;
  double hpfHz_ = 0.0;
  double atkCoef_ = 1.0;
  double atkRaw_ = 1.0;
  bool fb_ = true;
  static constexpr double kKneeDb = 4.0;
  static constexpr double kKneeFloor = 0.7943282347242815;  // -2 dB: below knee start
  double relCoef_ = 1.0;
  double hpfA_ = 0.0;
  double hpfX1_ = 0.0;
  double hpfY1_ = 0.0;
  double env_ = 0.0;
  double gain_ = 1.0;
  double grDb_ = 0.0;
};

}  // namespace iem1176
