#pragma once

namespace iem1176 {

enum class RatioPosition { R4 = 0, R8 = 1, R12 = 2, R20 = 3, All = 4 };

struct RatioLaw {
  double ratio = 4.0;
  double thresholdShiftDb = 0.0;
  double attackScale = 1.0;
  double releaseScale = 1.0;
};

inline const char* ratioId(RatioPosition p) {
  switch (p) {
    case RatioPosition::R4:
      return "R4";
    case RatioPosition::R8:
      return "R8";
    case RatioPosition::R12:
      return "R12";
    case RatioPosition::R20:
      return "R20";
    case RatioPosition::All:
      return "All";
  }
  return "R4";
}

inline bool ratioFromId(const char* s, RatioPosition& out) {
  if (!s) return false;
  if (s[0] == 'R' && s[1] == '4' && s[2] == 0) {
    out = RatioPosition::R4;
    return true;
  }
  if (s[0] == 'R' && s[1] == '8' && s[2] == 0) {
    out = RatioPosition::R8;
    return true;
  }
  if (s[0] == 'R' && s[1] == '1' && s[2] == '2' && s[3] == 0) {
    out = RatioPosition::R12;
    return true;
  }
  if (s[0] == 'R' && s[1] == '2' && s[2] == '0' && s[3] == 0) {
    out = RatioPosition::R20;
    return true;
  }
  if (s[0] == 'A' && s[1] == 'l' && s[2] == 'l' && s[3] == 0) {
    out = RatioPosition::All;
    return true;
  }
  return false;
}

inline RatioLaw lawFor(RatioPosition position) {
  switch (position) {
    case RatioPosition::R4:
      return {4.0, 0.0, 1.0, 1.0};
    case RatioPosition::R8:
      return {8.0, 0.0, 1.0, 1.0};
    case RatioPosition::R12:
      return {12.0, 0.0, 1.0, 1.0};
    case RatioPosition::R20:
      return {20.0, 0.0, 1.0, 1.0};
    case RatioPosition::All:
      // Model contract, not a physical-unit measurement.
      return {18.0, -3.0, 0.35, 0.60};
  }
  return {4.0, 0.0, 1.0, 1.0};
}

// Modifier engaged => All law. The stored R4/R8/R12/R20 position does not affect DSP.
inline RatioLaw effectiveLaw(RatioPosition position, bool allButtonsModifier) {
  if (allButtonsModifier || position == RatioPosition::All) return lawFor(RatioPosition::All);
  return lawFor(position);
}

inline RatioLaw lerpLaw(const RatioLaw& a, const RatioLaw& b, double t) {
  const double u = t < 0 ? 0 : (t > 1 ? 1 : t);
  RatioLaw o;
  o.ratio = a.ratio + (b.ratio - a.ratio) * u;
  o.thresholdShiftDb = a.thresholdShiftDb + (b.thresholdShiftDb - a.thresholdShiftDb) * u;
  o.attackScale = a.attackScale + (b.attackScale - a.attackScale) * u;
  o.releaseScale = a.releaseScale + (b.releaseScale - a.releaseScale) * u;
  return o;
}

}  // namespace iem1176
