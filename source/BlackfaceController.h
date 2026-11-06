// Blackface Compressor - VST3 edit controller + editor delegate.
#pragma once

#include <atomic>

#include "BlackfaceViews.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

namespace bf {

class Controller : public Steinberg::Vst::EditController,
                   public VSTGUI::VST3EditorDelegate,
                   public VSTGUI::IControlListener,
                   public ui::IHost {
 public:
  static Steinberg::FUnknown* createInstance(void*) {
    return static_cast<Steinberg::Vst::IEditController*>(new Controller);
  }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag,
                                                   Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;

  // VST3EditorDelegate
  VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
                                  const VSTGUI::IUIDescription* description, VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;

  // IControlListener
  void valueChanged(VSTGUI::CControl* control) SMTG_OVERRIDE;
  void controlBeginEdit(VSTGUI::CControl* control) SMTG_OVERRIDE;
  void controlEndEdit(VSTGUI::CControl* control) SMTG_OVERRIDE;

  // ui::IHost
  double paramNorm(unsigned id) const override;
  std::string paramText(unsigned id, double norm) const override;
  double meterGrDb() const override { return grDb_.load(); }
  double meterLevelDbfs() const override { return levelDbfs_.load(); }
  std::string presetLabel() const override;
  void stepPreset(int dir) override;
  void registerPanel(ui::PanelView* p) override { panel_ = p; }
  void unregisterPanel(ui::PanelView* p) override {
    if (panel_ == p) panel_ = nullptr;
  }


 private:
  void applyPreset(int index);

  ui::PanelView* panel_ = nullptr;
  std::atomic<double> grDb_{0.0};
  std::atomic<double> levelDbfs_{-90.0};
  int preset_ = 0;
  bool modified_ = false;
  bool applyingPreset_ = false;
};

}  // namespace bf
