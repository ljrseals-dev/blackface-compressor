#include "BlackfaceController.h"

#include "BlackfaceIds.h"
#include "BlackfaceParams.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace bf {
namespace {

class BfParameter : public Parameter {
 public:
  BfParameter(const ParamSpec& s)
  : Parameter(UString128(s.name).getText(), s.id, UString128(s.units).getText(), s.defNorm, s.stepCount,
              s.id == kBypass ? (ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass)
                              : (ParameterInfo::kCanAutomate | (s.names ? ParameterInfo::kIsList : 0))),
    spec_(s) {}

  void toString(ParamValue v, String128 out) const SMTG_OVERRIDE {
    UString(out, 128).fromAscii(bf::toText(spec_, v).c_str());
  }
  bool fromString(const TChar* text, ParamValue& v) const SMTG_OVERRIDE {
    char buf[128];
    UString(const_cast<TChar*>(text), 128).toAscii(buf, 128);
    double n = 0;
    if (!bf::fromText(spec_, buf, n)) return false;
    v = n;
    return true;
  }
  ParamValue toPlain(ParamValue n) const SMTG_OVERRIDE { return bf::toPlain(spec_, n); }
  ParamValue toNormalized(ParamValue p) const SMTG_OVERRIDE { return bf::toNorm(spec_, p); }

 private:
  const ParamSpec& spec_;
};

}  // namespace

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
  tresult r = EditController::initialize(context);
  if (r != kResultOk) return r;
  for (const auto& s : kSpecs) parameters.addParameter(new BfParameter(s));
  parameters.addParameter(STR16("GR Meter"), nullptr, 0, 0,
                          ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden, kMeterGR);
  parameters.addParameter(STR16("Out Meter"), nullptr, 0, 0,
                          ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden, kMeterOut);
  return kResultOk;
}

tresult PLUGIN_API Controller::terminate() { return EditController::terminate(); }

tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
  if (!state) return kResultFalse;
  IBStreamer s(state, kLittleEndian);
  int32 version = 0, count = 0;
  if (!s.readInt32(version) || version != 1 || !s.readInt32(count)) return kResultFalse;
  for (int i = 0; i < count && i < kNumSpecs; ++i) {
    double v = 0;
    if (!s.readDouble(v)) break;
    EditController::setParamNormalized(kSpecs[i].id, clamp01(v));
  }
  return kResultOk;
}

tresult PLUGIN_API Controller::setState(IBStream* state) {
  if (!state) return kResultFalse;
  IBStreamer s(state, kLittleEndian);
  int32 p = 0, m = 0;
  if (s.readInt32(p)) preset_ = std::clamp<int>(p, 0, kNumPresets - 1);
  if (s.readInt32(m)) modified_ = m != 0;
  return kResultOk;
}

tresult PLUGIN_API Controller::getState(IBStream* state) {
  if (!state) return kResultFalse;
  IBStreamer s(state, kLittleEndian);
  s.writeInt32(preset_);
  s.writeInt32(modified_ ? 1 : 0);
  return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name) {
  if (FIDStringsEqual(name, ViewType::kEditor))
    return new VSTGUI::VST3Editor(this, "view", "blackface.uidesc");
  return nullptr;
}

tresult PLUGIN_API Controller::setParamNormalized(ParamID tag, ParamValue value) {
  const tresult r = EditController::setParamNormalized(tag, value);
  if (tag == kMeterGR) {
    grDb_.store(value * 30.0);
  } else if (tag == kMeterOut) {
    levelDbfs_.store(value * 66.0 - 60.0);
  } else if (panel_) {
    panel_->paramChanged(tag, value);
  }
  return r;
}

VSTGUI::CView* Controller::createCustomView(VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes&,
                                            const VSTGUI::IUIDescription* description, VSTGUI::VST3Editor*) {
  if (name && std::string(name) == "BlackfacePanel") return new ui::PanelView(*this, this, description);
  return nullptr;
}

void Controller::controlBeginEdit(VSTGUI::CControl* c) {
  if (c->getTag() >= 0) beginEdit(ParamID(c->getTag()));
}

void Controller::controlEndEdit(VSTGUI::CControl* c) {
  if (c->getTag() >= 0) endEdit(ParamID(c->getTag()));
}

void Controller::valueChanged(VSTGUI::CControl* c) {
  if (c->getTag() < 0) return;
  const ParamID id = ParamID(c->getTag());
  const double v = clamp01(c->getValue());
  setParamNormalized(id, v);
  performEdit(id, v);
  if (!applyingPreset_ && id != kMeterMode && id != kBypass && !modified_) {
    modified_ = true;
    if (panel_) panel_->presetChanged();
  }
}

double Controller::paramNorm(unsigned id) const { return const_cast<Controller*>(this)->getParamNormalized(id); }

std::string Controller::paramText(unsigned id, double norm) const {
  const ParamSpec* s = findSpec(id);
  return s ? toText(*s, norm) : std::string();
}

std::string Controller::presetLabel() const {
  return std::string(kPresets[std::clamp(preset_, 0, kNumPresets - 1)].name) + (modified_ ? " *" : "");
}

void Controller::stepPreset(int dir) { applyPreset((preset_ + dir + kNumPresets) % kNumPresets); }

void Controller::applyPreset(int index) {
  const Preset& p = kPresets[index];
  struct V { unsigned id; double plain; };
  const V vals[] = {{kInput, p.inputDb},   {kOutput, p.outputDb}, {kAttack, p.attackUs}, {kRelease, p.releaseMs},
                    {kRatio, double(p.ratio)}, {kAll, p.all ? 1.0 : 0.0}, {kMix, p.mixPct}, {kHpf, p.hpfHz},
                    {kColor, p.color}};
  applyingPreset_ = true;
  for (const V& v : vals) {
    const ParamSpec* s = findSpec(v.id);
    const double n = toNorm(*s, v.plain);
    beginEdit(v.id);
    setParamNormalized(v.id, n);
    performEdit(v.id, n);
    endEdit(v.id);
  }
  applyingPreset_ = false;
  preset_ = index;
  modified_ = false;
  if (panel_) panel_->presetChanged();
}

}  // namespace bf
