#include "BlackfaceProcessor.h"

#include "BlackfaceIds.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace bf {

static int specIndex(ParamID id) {
  for (int i = 0; i < kNumSpecs; ++i)
    if (kSpecs[i].id == id) return i;
  return -1;
}

Processor::Processor() {
  setControllerClass(kControllerUID);
  for (int i = 0; i < kNumSpecs; ++i) norm_[i] = kSpecs[i].defNorm;
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
  tresult r = AudioEffect::initialize(context);
  if (r != kResultOk) return r;
  addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
  addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
  addAudioInput(STR16("Sidechain"), SpeakerArr::kStereo, kAux, 0);  // optional, off by default
  return kResultOk;
}

tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                 SpeakerArrangement* outputs, int32 numOuts) {
  if (numIns < 1 || numOuts != 1) return kResultFalse;
  const bool okMain = (inputs[0] == SpeakerArr::kStereo || inputs[0] == SpeakerArr::kMono) && outputs[0] == inputs[0];
  if (!okMain) return kResultFalse;
  if (numIns > 1 && !(inputs[1] == SpeakerArr::kStereo || inputs[1] == SpeakerArr::kMono)) return kResultFalse;
  return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 s) {
  return (s == kSample32 || s == kSample64) ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
  sampleRate_ = setup.sampleRate;
  maxBlock_ = std::max<int32>(setup.maxSamplesPerBlock, 64);
  engine_.prepare(sampleRate_, maxBlock_);
  pushSettings();
  return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state) {
  if (state) {
    engine_.prepare(sampleRate_, maxBlock_);
    pushSettings();
  }
  return AudioEffect::setActive(state);
}

void Processor::pushSettings() {
  engine_.setSettings(settingsFromNormalized(norm_));
  dirty_ = false;
}

void Processor::applyParameterChanges(IParameterChanges& changes) {
  const int32 n = changes.getParameterCount();
  for (int32 i = 0; i < n; ++i) {
    IParamValueQueue* q = changes.getParameterData(i);
    if (!q) continue;
    const int32 pts = q->getPointCount();
    if (pts <= 0) continue;
    int32 offset = 0;
    ParamValue v = 0;
    if (q->getPoint(pts - 1, offset, v) != kResultTrue) continue;
    const int idx = specIndex(q->getParameterId());
    if (idx >= 0) {
      norm_[idx] = clamp01(v);
      dirty_ = true;
    }
  }
}

template <typename T>
void Processor::run(ProcessData& data, T* const* in, T* const* out, int ch, T* const* sc, int scCh) {
  const int total = data.numSamples;
  for (int start = 0; start < total; start += maxBlock_) {
    const int n = std::min(maxBlock_, total - start);
    T* ip[2] = {nullptr, nullptr};
    T* op[2] = {nullptr, nullptr};
    T* sp[2] = {nullptr, nullptr};
    for (int c = 0; c < ch; ++c) {
      ip[c] = in[c] + start;
      op[c] = out[c] + start;
    }
    for (int c = 0; c < std::min(scCh, 2); ++c) sp[c] = sc[c] + start;
    engine_.process<T>(ip, op, ch, n, scCh > 0 ? sp : nullptr, scCh);
  }
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
  if (data.inputParameterChanges) applyParameterChanges(*data.inputParameterChanges);
  if (dirty_) pushSettings();

  if (data.numSamples > 0 && data.numInputs > 0 && data.numOutputs > 0) {
    AudioBusBuffers& in = data.inputs[0];
    AudioBusBuffers& out = data.outputs[0];
    const int ch = std::min<int>(std::min(in.numChannels, out.numChannels), 2);
    if (ch > 0) {
      const bool is32 = data.symbolicSampleSize == kSample32;
      int scCh = 0;
      AudioBusBuffers* scBus = nullptr;
      if (data.numInputs > 1 && data.inputs[1].numChannels > 0) {
        scBus = &data.inputs[1];
        scCh = scBus->numChannels;
      }
      if (is32) {
        run<float>(data, in.channelBuffers32, out.channelBuffers32, ch,
                   scBus ? scBus->channelBuffers32 : nullptr, scBus && scBus->channelBuffers32 ? scCh : 0);
      } else {
        run<double>(data, in.channelBuffers64, out.channelBuffers64, ch,
                    scBus ? scBus->channelBuffers64 : nullptr, scBus && scBus->channelBuffers64 ? scCh : 0);
      }
      out.silenceFlags = 0;
    }
  }

  // meters -> controller (hidden read-only parameters)
  if (data.outputParameterChanges) {
    const Meters& m = engine_.meters();
    const double gr = clamp01(m.grDb / 30.0);
    const double lv = clamp01((m.levelDbfs + 60.0) / 66.0);
    int32 qi = 0, pi = 0;
    if (IParamValueQueue* q = data.outputParameterChanges->addParameterData(kMeterGR, qi)) q->addPoint(0, gr, pi);
    if (IParamValueQueue* q = data.outputParameterChanges->addParameterData(kMeterOut, qi)) q->addPoint(0, lv, pi);
  }
  return kResultOk;
}

static constexpr int32 kStateVersion = 1;

tresult PLUGIN_API Processor::setState(IBStream* state) {
  if (!state) return kResultFalse;
  IBStreamer s(state, kLittleEndian);
  int32 version = 0, count = 0;
  if (!s.readInt32(version) || version != kStateVersion || !s.readInt32(count)) return kResultFalse;
  for (int i = 0; i < count && i < kNumSpecs; ++i) {
    double v = 0;
    if (!s.readDouble(v)) break;
    norm_[i] = clamp01(v);
  }
  dirty_ = true;
  return kResultOk;
}

tresult PLUGIN_API Processor::getState(IBStream* state) {
  if (!state) return kResultFalse;
  IBStreamer s(state, kLittleEndian);
  s.writeInt32(kStateVersion);
  s.writeInt32(kNumSpecs);
  for (int i = 0; i < kNumSpecs; ++i) s.writeDouble(norm_[i]);
  return kResultOk;
}

}  // namespace bf
