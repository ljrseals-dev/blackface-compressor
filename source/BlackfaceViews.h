// Blackface Compressor - VSTGUI views (knobs, buttons, VU meter) and the panel that holds them.
// Targets VSTGUI 4.11+ (VST3 SDK 3.7.4 or newer): uses the event based mouse API.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/uidescription/iuidescription.h"

namespace bf::ui {

class PanelView;

// What the views need from the controller.
class IHost {
 public:
  virtual ~IHost() = default;
  virtual double paramNorm(unsigned id) const = 0;
  virtual std::string paramText(unsigned id, double norm) const = 0;
  virtual double meterGrDb() const = 0;
  virtual double meterLevelDbfs() const = 0;
  virtual std::string presetLabel() const = 0;
  virtual void stepPreset(int dir) = 0;
  virtual void registerPanel(PanelView* p) = 0;
  virtual void unregisterPanel(PanelView* p) = 0;
};

class PanelView : public VSTGUI::CViewContainer {
 public:
  PanelView(IHost& host, VSTGUI::IControlListener* listener, const VSTGUI::IUIDescription* description);
  ~PanelView() override;

  // Controller -> UI. Call on the UI thread.
  void paramChanged(unsigned id, double norm);
  void presetChanged();

 private:
  IHost& host_;
  std::vector<VSTGUI::CControl*> controls_;
  VSTGUI::CView* nameView_ = nullptr;
};

}  // namespace bf::ui
