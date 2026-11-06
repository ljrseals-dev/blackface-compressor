// Blackface Compressor - VST3 audio processor.
#pragma once

#include "BlackfaceEngine.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

namespace bf {

class Processor : public Steinberg::Vst::AudioEffect {
 public:
  Processor();

  static Steinberg::FUnknown* createInstance(void*) {
    return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor);
  }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs,
                                                   Steinberg::int32 numIns,
                                                   Steinberg::Vst::SpeakerArrangement* outputs,
                                                   Steinberg::int32 numOuts) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;

 private:
  template <typename T>
  void run(Steinberg::Vst::ProcessData& data, T* const* in, T* const* out, int ch, T* const* sc, int scCh);
  void applyParameterChanges(Steinberg::Vst::IParameterChanges& changes);
  void pushSettings();

  Engine engine_;
  double norm_[kNumSpecs];
  bool dirty_ = true;
  double sampleRate_ = 44100.0;
  int maxBlock_ = 4096;
};

}  // namespace bf
