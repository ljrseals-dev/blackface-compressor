// Blackface Compressor - host-independent stereo engine.
// Wraps two PluginWrapper (FET compressor core) instances, adds stereo link,
// external sidechain key, and VU / GR meter ballistics. No SDK dependencies.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "BlackfaceParams.h"
#include "dsp/PluginWrapper.h"

namespace bf {

// Fixed internal threshold. The hardware has no threshold knob: INPUT drives the
// signal into a fixed threshold, OUTPUT restores level.
inline constexpr double kThresholdDbfs = -3.0;

struct Settings {
  double inputDb = 10.0;
  double outputDb = -6.0;
  double attackUs = 126.0;
  double releaseMs = 235.0;
  int ratio = 0;  // 0..3 -> 4,8,12,20
  bool all = false;
  double mixPct = 100.0;
  double hpfHz = 0.0;
  double color = 0.25;
  int quality = 2;   // 0..3 -> 1,2,4,8x
  int detector = 0;  // 0 internal, 1 stereo link, 2 external key
  bool bypass = false;
};

struct Meters {
  double grDb = 0.0;      // ballistic gain reduction, >= 0
  double levelDbfs = -90; // ballistic output level (VU-style), dBFS
};

inline Settings settingsFromNormalized(const double* norm /* indexed like kSpecs */) {
  Settings s;
  s.inputDb = toPlain(kSpecs[0], norm[0]);
  s.outputDb = toPlain(kSpecs[1], norm[1]);
  s.attackUs = toPlain(kSpecs[2], norm[2]);
  s.releaseMs = toPlain(kSpecs[3], norm[3]);
  s.ratio = int(toPlain(kSpecs[4], norm[4]));
  s.all = toPlain(kSpecs[5], norm[5]) > 0.5;
  // kSpecs[6] = meter mode (UI only)
  s.mixPct = toPlain(kSpecs[7], norm[7]);
  s.hpfHz = toPlain(kSpecs[8], norm[8]);
  s.color = toPlain(kSpecs[9], norm[9]);
  s.quality = int(toPlain(kSpecs[10], norm[10]));
  s.detector = int(toPlain(kSpecs[11], norm[11]));
  s.bypass = toPlain(kSpecs[12], norm[12]) > 0.5;
  return s;
}

class Engine {
 public:
  void prepare(double sampleRate, int maxBlock) {
    sr_ = sampleRate;
    maxBlock_ = std::max(maxBlock, 1);
    for (auto& v : buf_) v.assign(size_t(maxBlock_), 0.0);
    key_.assign(size_t(maxBlock_), 0.0);
    for (auto& w : wrap_) w.prepare(sr_);
    vuCoef_ = 1.0 - std::exp(-1.0 / (0.065 * sr_));  // ~300 ms to 99 %
    grAtk_ = 1.0 - std::exp(-double(kChunk) / (0.008 * sr_));
    grRel_ = 1.0 - std::exp(-double(kChunk) / (0.120 * sr_));
    if (haveSettings_) {
      const auto p = toParams(settings_);
      for (auto& w : wrap_) w.resetSmoother(p);
    }
    meters_ = Meters{};
    vuAcc_ = 0;
  }

  void setSettings(const Settings& s) {
    const auto p = toParams(s);
    if (!haveSettings_) {
      haveSettings_ = true;
      for (auto& w : wrap_) w.resetSmoother(p);
    } else {
      for (auto& w : wrap_) w.setTargets(p);
    }
    for (auto& w : wrap_) w.setHostBypass(s.bypass);
    settings_ = s;
  }

  const Meters& meters() const { return meters_; }

  // in/out: 1 or 2 channels. sc: optional external key (1 or 2 channels), may be null.
  template <typename T>
  void process(T* const* in, T* const* out, int numCh, int n, T* const* sc, int scCh) {
    if (n <= 0 || numCh <= 0) return;
    n = std::min(n, maxBlock_);
    const int ch = std::min(numCh, 2);
    for (int c = 0; c < ch; ++c)
      for (int i = 0; i < n; ++i) buf_[c][size_t(i)] = double(in[c][i]);

    // detector key (feed-forward) for Link / External
    const double kin = dbToLin(settings_.inputDb);
    const bool useExt = settings_.detector == 2 && sc && scCh > 0;
    const bool useLink = settings_.detector == 1 && ch == 2;
    const bool keyed = useExt || useLink;
    if (keyed) {
      for (int i = 0; i < n; ++i) {
        double k;
        if (useExt) {
          k = std::fabs(double(sc[0][i]));
          if (scCh > 1) k = std::max(k, std::fabs(double(sc[1][i])));
        } else {
          k = std::max(std::fabs(buf_[0][size_t(i)]), std::fabs(buf_[1][size_t(i)]));
        }
        key_[size_t(i)] = k * kin;
      }
    }

    for (int start = 0; start < n; start += kChunk) {
      const int len = std::min(kChunk, n - start);
      double grMax = 0;
      for (int c = 0; c < ch; ++c) {
        if (keyed) wrap_[c].setExternalSidechain(key_.data() + start, len);
        wrap_[c].process(buf_[c].data() + start, buf_[2 + c].data() + start, len);
        grMax = std::max(grMax, wrap_[c].chain().lastGainReductionDb());
      }
      const double coef = grMax > meters_.grDb ? grAtk_ : grRel_;
      meters_.grDb += coef * (grMax - meters_.grDb);
    }

    // output + VU level (mean-rectified, x1.11 so a sine reads its RMS)
    for (int i = 0; i < n; ++i) {
      double a = std::fabs(buf_[2][size_t(i)]);
      if (ch == 2) a = 0.5 * (a + std::fabs(buf_[3][size_t(i)]));
      vuAcc_ += vuCoef_ * (a - vuAcc_);
    }
    meters_.levelDbfs = 20.0 * std::log10(std::max(vuAcc_ * 1.1107, 1e-6));

    for (int c = 0; c < ch; ++c)
      for (int i = 0; i < n; ++i) out[c][i] = T(buf_[2 + c][size_t(i)]);
  }

 private:
  static constexpr int kChunk = 32;

  static iem1176::RevisionParameters toParams(const Settings& s) {
    using iem1176::RatioPosition;
    static const RatioPosition pos[4] = {RatioPosition::R4, RatioPosition::R8,
                                         RatioPosition::R12, RatioPosition::R20};
    static const int osf[4] = {1, 2, 4, 8};
    iem1176::RevisionParameters p;
    p.revision = iem1176::Revision::RevD;
    p.inputTrimDb = s.inputDb;
    p.ratio = pos[std::clamp(s.ratio, 0, 3)];
    p.allButtonsModifier = s.all;
    p.attackUs = s.attackUs;
    p.releaseMs = s.releaseMs;
    p.thresholdDb = kThresholdDbfs;
    p.outputGainDb = s.outputDb;
    p.sidechainHpfHz = s.hpfHz;
    p.mixPct = s.mixPct;
    p.lnDrive = s.color;
    p.oversampling = osf[std::clamp(s.quality, 0, 3)];
    return p;
  }

  double sr_ = 48000;
  int maxBlock_ = 1;
  bool haveSettings_ = false;
  Settings settings_{};
  iem1176::PluginWrapper wrap_[2];
  std::vector<double> buf_[4];
  std::vector<double> key_;
  Meters meters_{};
  double vuCoef_ = 0.001, grAtk_ = 0.1, grRel_ = 0.01, vuAcc_ = 0;

  static double dbToLin(double db) { return std::pow(10.0, db / 20.0); }
};

}  // namespace bf
