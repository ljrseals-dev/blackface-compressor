#pragma once

#include "RatioSwitch.h"
#include "Revision.h"

namespace iem1176 {

struct RevisionParameters {
  Revision revision = Revision::RevD;
  double inputTrimDb = 0.0;
  RatioPosition ratio = RatioPosition::R4;
  bool allButtonsModifier = false;
  double attackUs = 800.0;
  double releaseMs = 300.0;
  double thresholdDb = -24.0;
  double outputGainDb = 0.0;
  double sidechainHpfHz = 0.0;
  double mixPct = 100.0;
  double lnDrive = 0.0;
  int oversampling = 8;
};

inline bool sameDiscrete(const RevisionParameters& a, const RevisionParameters& b) {
  return a.revision == b.revision && a.ratio == b.ratio &&
         a.allButtonsModifier == b.allButtonsModifier && a.oversampling == b.oversampling &&
         a.sidechainHpfHz == b.sidechainHpfHz;
}

}  // namespace iem1176
