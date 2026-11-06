#pragma once

#include <vector>

#include "ClassABPushPull.h"
#include "JfetModel.h"
#include "OutputTransformer.h"
#include "Oversampler.h"
#include "RevisionParameters.h"
#include "Sidechain.h"

namespace iem1176 {

// Topology (Rev D model):
//   input trim → [8×] LN drive → JFET shunt → unity preamp →
//   class-AB (26 dB open loop) → output transformer → output gain → mix.
// Sidechain reads the previous oversampled post-gain-cell sample unless an
// external buffer is installed for this block (CR-002).
// This class does not smooth parameters and does not fade. That is the wrapper.
class CompressorChain {
 public:
  void prepare(double sampleRate) {
    sr_ = std::max(sampleRate, 1.0);
    sidechain_.prepare(sr_ * double(factor_));
    pushCoefs();
  }

  void reset() {
    sidechain_.reset();
    prevCell_ = 0;
    fb_ = 0;
    prevIn_ = 0;
    prevExt_ = 0;
    lastGr_ = 0;
    ext_ = nullptr;
    extN_ = 0;
    ++resetCount_;
  }

  int resetCount() const { return resetCount_; }

  void setParameters(const RevisionParameters& p) {
    params_ = p;
    const int f = snapOversampling(p.oversampling);
    if (f != factor_) {
      factor_ = f;
      prevIn_ = 0;
      prevExt_ = 0;
      sidechain_.prepare(sr_ * double(factor_));
    }
    inputGain_ = dbToLin(p.inputTrimDb);
    outputGain_ = dbToLin(p.outputGainDb);
    mix_ = std::clamp(p.mixPct / 100.0, 0.0, 1.0);
    ln_ = std::clamp(p.lnDrive, 0.0, 1.0);
    if (!blend_) law_ = effectiveLaw(p.ratio, p.allButtonsModifier);
    pushCoefs();
  }

  // Wrapper-only. Linear crossfade between ratio laws. Not used by presets.
  void setEffectiveLaw(const RatioLaw& law) {
    law_ = law;
    blend_ = true;
    pushCoefs();
  }

  void clearLawOverride() {
    blend_ = false;
    law_ = effectiveLaw(params_.ratio, params_.allButtonsModifier);
    pushCoefs();
  }

  // CR-002. Non-null buffer is the detector source for the next process() only.
  void setExternalSidechain(const double* buffer, int numSamples) {
    ext_ = buffer;
    extN_ = buffer ? std::max(numSamples, 0) : 0;
  }

  // CR-003. Forwards to Sidechain::setHpfFrequency.
  void setSidechainHpf(double hz) {
    params_.sidechainHpfHz = hz;
    sidechain_.setHpfFrequency(hz);
  }

  double processSample(double x, bool hasExt, double extSample) {
    const double trimmed = x * inputGain_;
    double wetAcc = 0;
    double grAcc = 0;
    for (int k = 1; k <= factor_; ++k) {
      const double t = double(k) / double(factor_);
      const double s = prevIn_ + (trimmed - prevIn_) * t;
      double detExt = 0;
      if (hasExt) detExt = prevExt_ + (extSample - prevExt_) * t;
      const Tick tk = tickHigh(s, hasExt, detExt);
      wetAcc += tk.wet;
      grAcc += tk.grDb;
    }
    prevIn_ = trimmed;
    if (hasExt) prevExt_ = extSample;
    const double wet = (wetAcc / double(factor_)) * outputGain_;
    lastGr_ = grAcc / double(factor_);
    return wet * mix_ + x * (1.0 - mix_);
  }

  void process(const double* in, double* out, int numSamples) {
    if (static_cast<int>(gr_.size()) < numSamples) gr_.resize(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i) {
      const bool has = ext_ != nullptr && i < extN_;
      out[i] = processSample(in[i], has, has ? ext_[i] : 0.0);
      gr_[static_cast<size_t>(i)] = lastGr_;
    }
    ext_ = nullptr;
    extN_ = 0;
  }

  double lastGainReductionDb() const { return lastGr_; }
  const double* gainReductionDb() const { return gr_.empty() ? nullptr : gr_.data(); }
  int oversamplingFactor() const { return factor_; }
  const RevisionParameters& parameters() const { return params_; }
  const RatioLaw& law() const { return law_; }
  double sampleRate() const { return sr_; }

 private:
  struct Tick {
    double wet;
    double grDb;
  };

  void pushCoefs() {
    const double atk = std::max(params_.attackUs * 1.0e-6 * law_.attackScale, 1.0e-6);
    const double rel = std::max(params_.releaseMs * 1.0e-3 * law_.releaseScale, 1.0e-4);
    sidechain_.setAttackSeconds(atk);
    sidechain_.setReleaseSeconds(rel);
    sidechain_.setThresholdDb(params_.thresholdDb + law_.thresholdShiftDb);
    sidechain_.setRatio(law_.ratio);
    sidechain_.setHpfFrequency(params_.sidechainHpfHz);
  }

  Tick tickHigh(double x, bool hasExt, double extSample) {
    const double detector = hasExt ? extSample : prevCell_;
    sidechain_.setFeedbackMode(!hasExt);
    const double target = sidechain_.processSample(detector);
    jfet_.setTargetGain(target);
    const double driven = ln_ > 1.0e-8 ? applyLn(x, ln_) : x;
    const double cell = jfet_.processSample(driven);
    const double err = cell - fb_;
    const double primary = ab_.processOpenLoop(err);
    const double secondary = xfmr_.secondaryFromPrimary(primary);
    fb_ = flushDenorm(kBetaPath * primary);
    prevCell_ = cell;
    const double g = jfet_.currentGain();
    Tick tk;
    tk.wet = secondary;
    tk.grDb = g >= 0.999999 ? 0.0 : -linToDb(g);
    return tk;
  }

  RevisionParameters params_{};
  RatioLaw law_ = lawFor(RatioPosition::R4);
  bool blend_ = false;
  double sr_ = 48000.0;
  int factor_ = 8;
  double inputGain_ = 1;
  double outputGain_ = 1;
  double mix_ = 1;
  double ln_ = 0;
  JfetModel jfet_{};
  ClassABPushPull ab_{};
  OutputTransformer xfmr_{};
  Sidechain sidechain_{};
  double prevCell_ = 0;
  double fb_ = 0;
  double prevIn_ = 0;
  double prevExt_ = 0;
  double lastGr_ = 0;
  int resetCount_ = 0;
  const double* ext_ = nullptr;
  int extN_ = 0;
  std::vector<double> gr_;
};

}  // namespace iem1176
