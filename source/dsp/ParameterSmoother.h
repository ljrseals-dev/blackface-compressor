#pragma once

#include <algorithm>
#include <cmath>

#include "RevisionParameters.h"

namespace iem1176 {

// Wrapper-layer smoothing. CompressorChain never sees this class.
// Continuous params: one-pole, 20 ms.
// Ratio / All-modifier: 5 ms linear law crossfade.
// Revision change: flags a full chain reset and a 30 ms output fade.
// Preset loads snap (the wrapper crossfades outputs for 30 ms).
class ParameterSmoother {
 public:
  struct Step {
    RevisionParameters params;
    RatioLaw law;
    bool resetChain = false;
    double outputFade = 1.0;
  };

  void prepare(double sampleRate) {
    sr_ = std::max(sampleRate, 1.0);
    coef_ = 1.0 - std::exp(-1.0 / (0.020 * sr_));
  }

  void reset(const RevisionParameters& p) {
    target_ = p;
    curIn_ = p.inputTrimDb;
    curAtk_ = p.attackUs;
    curRel_ = p.releaseMs;
    curThr_ = p.thresholdDb;
    curOut_ = p.outputGainDb;
    curMix_ = p.mixPct;
    curLn_ = p.lnDrive;
    hpf_ = p.sidechainHpfHz;
    os_ = p.oversampling;
    revision_ = p.revision;
    ratio_ = p.ratio;
    all_ = p.allButtonsModifier;
    lawFrom_ = effectiveLaw(p.ratio, p.allButtonsModifier);
    lawTo_ = lawFrom_;
    lawT_ = 1;
    lawLeft_ = 0;
    revLeft_ = 0;
    revTotal_ = 1;
    pendingReset_ = false;
  }

  void setTargets(const RevisionParameters& p) {
    if (p.revision != revision_) {
      revision_ = p.revision;
      revTotal_ = std::max(1, static_cast<int>(std::lround(0.030 * sr_)));
      revLeft_ = revTotal_;
      pendingReset_ = true;
      // Snap continuous values; the output fade covers the discontinuity.
      snapContinuous(p);
    }
    if (p.ratio != ratio_ || p.allButtonsModifier != all_) {
      lawFrom_ = currentLaw();
      ratio_ = p.ratio;
      all_ = p.allButtonsModifier;
      lawTo_ = effectiveLaw(ratio_, all_);
      lawLeft_ = std::max(1, static_cast<int>(std::lround(0.005 * sr_)));
      lawSpan_ = lawLeft_;
      lawT_ = 0;
    }
    hpf_ = p.sidechainHpfHz;
    os_ = p.oversampling;
    target_ = p;
  }

  // Preset load: snap. Output crossfade is the wrapper's job.
  void loadPreset(const RevisionParameters& p) {
    reset(p);
    target_ = p;
  }

  Step advance() {
    curIn_ += coef_ * (target_.inputTrimDb - curIn_);
    curAtk_ += coef_ * (target_.attackUs - curAtk_);
    curRel_ += coef_ * (target_.releaseMs - curRel_);
    curThr_ += coef_ * (target_.thresholdDb - curThr_);
    curOut_ += coef_ * (target_.outputGainDb - curOut_);
    curMix_ += coef_ * (target_.mixPct - curMix_);
    curLn_ += coef_ * (target_.lnDrive - curLn_);

    Step s;
    s.resetChain = false;
    s.outputFade = 1;
    if (revLeft_ > 0) {
      const int pos = revTotal_ - revLeft_;
      const double u = double(pos) / double(revTotal_);
      s.outputFade = u < 0.5 ? (1.0 - u * 2.0) : ((u - 0.5) * 2.0);
      if (pendingReset_ && pos >= revTotal_ / 2) {
        s.resetChain = true;
        pendingReset_ = false;
      }
      --revLeft_;
    }
    if (lawLeft_ > 0) {
      lawT_ = 1.0 - double(lawLeft_) / double(lawSpan_);
      --lawLeft_;
      if (lawLeft_ == 0) lawT_ = 1;
    } else {
      lawT_ = 1;
    }

    s.params.revision = revision_;
    s.params.inputTrimDb = curIn_;
    s.params.ratio = ratio_;
    s.params.allButtonsModifier = all_;
    s.params.attackUs = curAtk_;
    s.params.releaseMs = curRel_;
    s.params.thresholdDb = curThr_;
    s.params.outputGainDb = curOut_;
    s.params.sidechainHpfHz = hpf_;
    s.params.mixPct = curMix_;
    s.params.lnDrive = curLn_;
    s.params.oversampling = os_;
    s.law = currentLaw();
    return s;
  }

  RatioLaw currentLaw() const { return lerpLaw(lawFrom_, lawTo_, lawT_); }

 private:
  void snapContinuous(const RevisionParameters& p) {
    curIn_ = p.inputTrimDb;
    curAtk_ = p.attackUs;
    curRel_ = p.releaseMs;
    curThr_ = p.thresholdDb;
    curOut_ = p.outputGainDb;
    curMix_ = p.mixPct;
    curLn_ = p.lnDrive;
  }

  double sr_ = 48000;
  double coef_ = 0.001;
  RevisionParameters target_{};
  double curIn_ = 0, curAtk_ = 800, curRel_ = 300, curThr_ = -24, curOut_ = 0, curMix_ = 100,
         curLn_ = 0;
  double hpf_ = 0;
  int os_ = 8;
  Revision revision_ = Revision::RevD;
  RatioPosition ratio_ = RatioPosition::R4;
  bool all_ = false;
  RatioLaw lawFrom_{};
  RatioLaw lawTo_{};
  double lawT_ = 1;
  int lawLeft_ = 0;
  int lawSpan_ = 1;
  int revLeft_ = 0;
  int revTotal_ = 1;
  bool pendingReset_ = false;
};

}  // namespace iem1176
