#pragma once

#include <algorithm>
#include <vector>

#include "CompressorChain.h"
#include "ParameterSmoother.h"

namespace iem1176 {

// Host-facing wrapper. Smoothing, preset crossfade, revision fade, and host
// bypass live here — never inside CompressorChain.
// Host bypass is a unity-gain dry crossfade. It must not call reset().
class PluginWrapper {
 public:
  void prepare(double sampleRate) {
    sr_ = sampleRate;
    main_.prepare(sampleRate);
    shadow_.prepare(sampleRate);
    smoother_.prepare(sampleRate);
    smoother_.reset(RevisionParameters{});
    apply(smoother_.advance());
  }

  void resetSmoother(const RevisionParameters& p) {
    smoother_.reset(p);
    apply(smoother_.advance());
  }

  void setTargets(const RevisionParameters& p) { smoother_.setTargets(p); }

  void loadPreset(const RevisionParameters& p) {
    shadow_ = main_;
    shadowParams_ = main_.parameters();
    shadowLaw_ = main_.law();
    smoother_.loadPreset(p);
    fading_ = true;
    fadeTotal_ = std::max(1, static_cast<int>(std::lround(0.030 * sr_)));
    fadePos_ = 0;
  }

  void setHostBypass(bool bypass) { bypassTarget_ = bypass ? 1.0 : 0.0; }

  bool hostBypass() const { return bypassTarget_ > 0.5; }

  void setExternalSidechain(const double* buffer, int numSamples) {
    ext_ = buffer;
    extN_ = buffer ? numSamples : 0;
  }

  void process(const double* in, double* out, int n) {
    for (int i = 0; i < n; ++i) {
      const auto step = smoother_.advance();
      if (step.resetChain) main_.reset();
      if (step.params.oversampling != main_.oversamplingFactor()) {
        // Re-prepare the oversampler only (detector state kept).
        main_.setParameters(step.params);
      }
      apply(step);
      const bool has = ext_ && i < extN_;
      const double ext = has ? ext_[i] : 0.0;
      double wet = main_.processSample(in[i], has, ext);
      if (fading_) {
        shadow_.setParameters(shadowParams_);
        shadow_.setEffectiveLaw(shadowLaw_);
        const double old = shadow_.processSample(in[i], has, ext);
        const double t = double(fadePos_) / double(fadeTotal_);
        const double g1 = std::sin(t * kPi * 0.5);
        const double g0 = std::cos(t * kPi * 0.5);
        wet = old * g0 + wet * g1;
        ++fadePos_;
        if (fadePos_ >= fadeTotal_) fading_ = false;
      }
      const double bCoef = 1.0 - std::exp(-1.0 / (0.010 * sr_));
      bypassMix_ += bCoef * (bypassTarget_ - bypassMix_);
      double y = wet * (1.0 - bypassMix_) + in[i] * bypassMix_;
      y *= step.outputFade;
      out[i] = y;
    }
    ext_ = nullptr;
    extN_ = 0;
  }

  CompressorChain& chain() { return main_; }
  const CompressorChain& chain() const { return main_; }
  int resetCount() const { return main_.resetCount(); }
  bool presetFadeActive() const { return fading_; }

 private:
  void apply(const ParameterSmoother::Step& step) {
    main_.setParameters(step.params);
    main_.setEffectiveLaw(step.law);
  }

  double sr_ = 48000;
  CompressorChain main_{};
  CompressorChain shadow_{};
  RevisionParameters shadowParams_{};
  RatioLaw shadowLaw_{};
  ParameterSmoother smoother_{};
  bool fading_ = false;
  int fadeTotal_ = 1;
  int fadePos_ = 0;
  double bypassMix_ = 0;
  double bypassTarget_ = 0;
  const double* ext_ = nullptr;
  int extN_ = 0;
};

}  // namespace iem1176
