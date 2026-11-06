// Blackface Compressor - parameter table shared by processor, controller and UI.
// No SDK dependencies: this header is also used by the host-less test.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace bf {

inline constexpr double kPiD = 3.14159265358979323846;

enum ParamId : unsigned int {
  kInput = 100,
  kOutput = 101,
  kAttack = 102,
  kRelease = 103,
  kRatio = 104,
  kAll = 105,
  kMeterMode = 106,
  kMix = 107,
  kHpf = 108,
  kColor = 109,
  kQuality = 110,
  kDetector = 111,
  kBypass = 112,
  // read-only, processor -> controller
  kMeterGR = 200,
  kMeterOut = 201
};

enum class Curve { Linear, LogDown };  // LogDown: norm 0 = maxV, norm 1 = minV

struct ParamSpec {
  ParamId id;
  const char* name;
  const char* units;
  double minV, maxV;
  double defNorm;
  int stepCount;             // 0 = continuous, N = N+1 discrete values
  Curve curve;
  const char* const* names;  // discrete value names (stepCount+1 entries) or nullptr
};

inline const char* const kRatioNames[] = {"4:1", "8:1", "12:1", "20:1"};
inline const char* const kMeterNames[] = {"GR", "+4", "+8", "OFF"};
inline const char* const kQualityNames[] = {"1x", "2x", "4x", "8x"};
inline const char* const kDetectorNames[] = {"Internal", "Link", "External"};
inline const char* const kOnOff[] = {"Off", "On"};

// Order = persisted order (state version 1). Append only.
inline const ParamSpec kSpecs[] = {
    {kInput, "Input", "dB", 0.0, 40.0, 0.25, 0, Curve::Linear, nullptr},
    {kOutput, "Output", "dB", -24.0, 12.0, 0.5, 0, Curve::Linear, nullptr},
    {kAttack, "Attack", "us", 20.0, 800.0, 0.5, 0, Curve::LogDown, nullptr},
    {kRelease, "Release", "ms", 50.0, 1100.0, 0.5, 0, Curve::LogDown, nullptr},
    {kRatio, "Ratio", "", 0, 3, 0.0, 3, Curve::Linear, kRatioNames},
    {kAll, "All Buttons", "", 0, 1, 0.0, 1, Curve::Linear, kOnOff},
    {kMeterMode, "Meter", "", 0, 3, 0.0, 3, Curve::Linear, kMeterNames},
    {kMix, "Mix", "%", 0.0, 100.0, 1.0, 0, Curve::Linear, nullptr},
    {kHpf, "Detector HPF", "Hz", 0.0, 300.0, 0.0, 0, Curve::Linear, nullptr},
    {kColor, "Color", "", 0.0, 1.0, 0.25, 0, Curve::Linear, nullptr},
    {kQuality, "Oversampling", "", 0, 3, 1.0, 3, Curve::Linear, kQualityNames},
    {kDetector, "Detector", "", 0, 2, 0.0, 2, Curve::Linear, kDetectorNames},
    {kBypass, "Bypass", "", 0, 1, 0.0, 1, Curve::Linear, kOnOff},
};
inline constexpr int kNumSpecs = int(sizeof(kSpecs) / sizeof(kSpecs[0]));

inline const ParamSpec* findSpec(unsigned id) {
  for (const auto& s : kSpecs)
    if (s.id == id) return &s;
  return nullptr;
}

inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

inline double toPlain(const ParamSpec& s, double norm) {
  norm = clamp01(norm);
  if (s.stepCount > 0) return std::floor(norm * s.stepCount + 0.5);
  if (s.curve == Curve::LogDown) return s.maxV * std::pow(s.minV / s.maxV, norm);
  return s.minV + (s.maxV - s.minV) * norm;
}

inline double toNorm(const ParamSpec& s, double plain) {
  if (s.stepCount > 0) return clamp01(plain / double(s.stepCount));
  if (s.curve == Curve::LogDown)
    return clamp01(std::log(std::max(plain, 1e-9) / s.maxV) / std::log(s.minV / s.maxV));
  return clamp01((plain - s.minV) / (s.maxV - s.minV));
}

inline int toIndex(const ParamSpec& s, double norm) { return int(toPlain(s, norm)); }

inline std::string toText(const ParamSpec& s, double norm) {
  char b[64];
  const double v = toPlain(s, norm);
  if (s.names) return s.names[int(v)];
  switch (s.id) {
    case kInput:
    case kOutput: std::snprintf(b, sizeof b, "%+.1f dB", v); break;
    case kAttack: std::snprintf(b, sizeof b, "%.0f us", v); break;
    case kRelease: std::snprintf(b, sizeof b, "%.0f ms", v); break;
    case kMix: std::snprintf(b, sizeof b, "%.0f %%", v); break;
    case kHpf:
      if (v < 1.0) return "OFF";
      std::snprintf(b, sizeof b, "%.0f Hz", v);
      break;
    case kColor: std::snprintf(b, sizeof b, "%.0f %%", v * 100.0); break;
    default: std::snprintf(b, sizeof b, "%.2f", v); break;
  }
  return b;
}

// Accepts "12", "12 dB", "250us" ... returns false when no number is present.
inline bool fromText(const ParamSpec& s, const std::string& text, double& norm) {
  if (s.names) {
    for (int i = 0; i <= s.stepCount; ++i)
      if (text == s.names[i]) {
        norm = toNorm(s, i);
        return true;
      }
  }
  if (s.id == kHpf && (text == "OFF" || text == "Off" || text == "off")) {
    norm = 0.0;
    return true;
  }
  char* end = nullptr;
  double v = std::strtod(text.c_str(), &end);
  if (end == text.c_str()) return false;
  if (s.id == kColor) v = v > 1.0 ? v / 100.0 : v;
  norm = toNorm(s, v);
  return true;
}

// ---- Factory presets (normalized via plain values so they survive range edits) ----
struct Preset {
  const char* name;
  double inputDb, outputDb, attackUs, releaseMs;
  int ratio;  // 0..3
  bool all;
  double mixPct, hpfHz, color;
};

inline const Preset kPresets[] = {
    {"Init", 10, -6, 126, 235, 0, false, 100, 0, 0.25},
    {"Lead Vocal", 14, -8, 300, 250, 1, false, 100, 90, 0.25},
    {"Vocal Presence", 9, -5, 400, 180, 0, false, 100, 150, 0.15},
    {"Vocal Warmth", 12, -7, 500, 400, 0, false, 100, 60, 0.45},
    {"Spoken Word", 11, -6, 250, 300, 1, false, 100, 80, 0.10},
    {"Rap Lead", 18, -11, 200, 120, 1, false, 100, 100, 0.35},
    {"Vocal Crush Parallel", 28, -16, 60, 110, 3, true, 40, 80, 0.50},
    {"Snare Crack", 16, -9, 800, 90, 1, false, 100, 0, 0.30},
    {"Kick Control", 12, -7, 700, 160, 2, false, 100, 60, 0.20},
    {"Drum Room Smash", 30, -17, 30, 70, 3, true, 100, 0, 0.60},
    {"Drum Bus Glue", 8, -4, 600, 450, 0, false, 100, 80, 0.15},
    {"Bass Finger Even", 13, -7, 450, 300, 1, false, 100, 0, 0.40},
    {"Mix Bus Glue", 6, -3, 800, 700, 0, false, 100, 120, 0.10},
    {"Mix Bus Parallel", 20, -10, 120, 200, 2, false, 35, 100, 0.30},
    {"Broadcast Voice", 15, -9, 220, 200, 1, false, 100, 110, 0.20},
    {"All-In Special", 26, -15, 40, 90, 3, true, 100, 0, 0.55},
};
inline constexpr int kNumPresets = int(sizeof(kPresets) / sizeof(kPresets[0]));

}  // namespace bf
