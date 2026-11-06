#pragma once

#include "DspMath.h"

namespace iem1176 {

// Primary:secondary = 15:1, tertiary = 0.5, feedback divider = 0.5.
// Feedback itself is closed in CompressorChain (it encloses the AB stage).
// Turns ratios are spec constants, not measurements from a physical unit.
class OutputTransformer {
 public:
  static constexpr double kPrimaryToSecondary = kTurns;
  static constexpr double kTertiaryTurns = kTertiary;
  static constexpr double kFeedbackDivider = iem1176::kFeedbackDivider;

  double secondaryFromPrimary(double primary) const { return primary * kInvTurns; }
  double tertiaryFromPrimary(double primary) const { return primary * kTertiary * kInvTurns; }
};

}  // namespace iem1176
