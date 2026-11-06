#include "BlackfaceViews.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "BlackfaceLayout.h"
#include "BlackfaceParams.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/events.h"

using namespace VSTGUI;

namespace bf::ui {
namespace {

const CColor kAmber(255, 190, 80, 255);
const CColor kLedOn(255, 196, 80, 255);

double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

CPoint polarPt(const CPoint& c, double r, double deg) {
  const double a = deg * 3.14159265358979323846 / 180.0;
  return CPoint(c.x + r * std::sin(a), c.y - r * std::cos(a));
}

void glow(CDrawContext* ctx, const CPoint& c, double r, uint8_t peakAlpha) {
  // soft LED bloom from stacked translucent discs
  for (int i = 6; i >= 1; --i) {
    const double rr = r * (1.0 + i * 0.55);
    ctx->setFillColor(CColor(255, 150, 30, uint8_t(peakAlpha / (i + 1))));
    ctx->drawEllipse(CRect(c.x - rr, c.y - rr, c.x + rr, c.y + rr), kDrawFilled);
  }
  ctx->setFillColor(kLedOn);
  ctx->drawEllipse(CRect(c.x - r, c.y - r, c.x + r, c.y + r), kDrawFilled);
}

// ---------------------------------------------------------------------------- knob
class KnobView : public CControl {
 public:
  KnobView(const CRect& size, IControlListener* l, int32_t tag, CBitmap* cap, double capSize, double ring,
           double half, double readoutDy, IHost& host)
  : CControl(size, l, tag), cap_(cap), capSize_(capSize), ring_(ring), half_(half), readoutDy_(readoutDy), host_(host) {
    setMin(0.f);
    setMax(1.f);
  }

  void draw(CDrawContext* ctx) override {
    const CRect vs = getViewSize();
    const CPoint c(vs.left + half_, vs.top + half_);
    const double v = clampd(getValue(), 0.0, 1.0);
    const double sweep = layout::kSweepDeg;
    ctx->setDrawMode(kAntiAliasing);
    ctx->setLineStyle(kLineSolid);

    if (cap_) cap_->draw(ctx, CRect(c.x - capSize_ / 2, c.y - capSize_ / 2, c.x + capSize_ / 2, c.y + capSize_ / 2));

    // track + value arc (polyline segments)
    const double w = capSize_ > 100 ? 5.0 : (capSize_ > 50 ? 4.0 : 3.0);
    ctx->setLineWidth(w);
    auto arc = [&](double n0, double n1, const CColor& col) {
      if (n1 <= n0) return;
      ctx->setFrameColor(col);
      const int steps = std::max(2, int((n1 - n0) * 90));
      CPoint prev = polarPt(c, ring_, -sweep + 2 * sweep * n0);
      for (int i = 1; i <= steps; ++i) {
        const double n = n0 + (n1 - n0) * i / steps;
        const CPoint p = polarPt(c, ring_, -sweep + 2 * sweep * n);
        ctx->drawLine(CDrawContext::LinePair(prev, p));
        prev = p;
      }
    };
    arc(0.0, 1.0, CColor(70, 70, 74, 255));
    arc(0.0, v, CColor(245, 160, 20, 255));

    // pointer
    const double a = -sweep + 2 * sweep * v;
    ctx->setLineWidth(capSize_ > 100 ? 3.2 : 2.4);
    ctx->setFrameColor(CColor(250, 250, 250, 255));
    ctx->drawLine(CDrawContext::LinePair(polarPt(c, capSize_ * 0.22, a), polarPt(c, capSize_ * 0.44, a)));

    // readout
    ctx->setFont(kNormalFontSmall);
    ctx->setFontColor(kAmber);
    const std::string t = host_.paramText(unsigned(getTag()), v);
    ctx->drawString(t.c_str(), CRect(c.x - 50, c.y + ring_ + readoutDy_ - 8, c.x + 50, c.y + ring_ + readoutDy_ + 8),
                    kCenterText);
    setDirty(false);
  }

  void onMouseDownEvent(MouseDownEvent& e) override {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    const bool dbl = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDown_).count() < 350;
    lastDown_ = now;
    if (dbl) {
      beginEdit();
      setValue(getDefaultValue());
      valueChanged();
      endEdit();
      invalid();
      e.consumed = true;
      return;
    }
    dragging_ = true;
    startY_ = e.mousePosition.y;
    startValue_ = getValue();
    beginEdit();
    e.consumed = true;
  }

  void onMouseMoveEvent(MouseMoveEvent& e) override {
    if (!dragging_) return;
    const bool fine = e.modifiers.has(ModifierKey::Shift);
    const double perPx = fine ? 1.0 / 2000.0 : 1.0 / 220.0;
    const double v = clampd(startValue_ + (startY_ - e.mousePosition.y) * perPx, 0.0, 1.0);
    setValue(float(v));
    valueChanged();
    invalid();
    e.consumed = true;
  }

  void onMouseUpEvent(MouseUpEvent& e) override {
    if (!dragging_) return;
    dragging_ = false;
    endEdit();
    e.consumed = true;
  }

  void onMouseCancelEvent(MouseCancelEvent& e) override {
    if (!dragging_) return;
    dragging_ = false;
    endEdit();
    (void)e;
  }

  void onMouseWheelEvent(MouseWheelEvent& e) override {
    const double step = (e.modifiers.has(ModifierKey::Shift) ? 0.002 : 0.02) * (e.deltaY > 0 ? 1.0 : -1.0);
    if (e.deltaY == 0.0) return;
    beginEdit();
    setValue(float(clampd(getValue() + step, 0.0, 1.0)));
    valueChanged();
    endEdit();
    invalid();
    e.consumed = true;
  }

 private:
  SharedPointer<CBitmap> cap_;
  double capSize_, ring_, half_, readoutDy_;
  IHost& host_;
  bool dragging_ = false;
  double startY_ = 0, startValue_ = 0;
  std::chrono::steady_clock::time_point lastDown_{};
};

// ---------------------------------------------------------------------------- buttons
class ButtonView : public CControl {
 public:
  // index/steps: exclusive group member `index` of `steps+1` values; steps==1 && index==1 -> toggle.
  ButtonView(const CRect& size, IControlListener* l, int32_t tag, int index, int steps, bool led,
             std::function<bool()> forceLit = nullptr)
  : CControl(size, l, tag), index_(index), steps_(steps), led_(led), forceLit_(std::move(forceLit)) {
    setMin(0.f);
    setMax(1.f);
  }

  bool isLit() const {
    if (forceLit_ && forceLit_()) return true;
    return selected();
  }

  void draw(CDrawContext* ctx) override {
    if (!isLit()) {
      setDirty(false);
      return;
    }
    const CRect r = getViewSize();
    ctx->setDrawMode(kAntiAliasing);
    if (auto p = owned(ctx->createRoundRectGraphicsPath(r, 5.0))) {
      ctx->setFillColor(CColor(255, 170, 40, 46));
      ctx->drawGraphicsPath(p, CDrawContext::kPathFilled);
      ctx->setLineWidth(1.0);
      ctx->setFrameColor(CColor(255, 170, 40, 170));
      ctx->drawGraphicsPath(p, CDrawContext::kPathStroked);
    }
    if (led_) glow(ctx, CPoint(r.left + r.getWidth() / 2, r.top + 10), 3.6, 90);
    setDirty(false);
  }

  void onMouseDownEvent(MouseDownEvent& e) override {
    double v;
    if (steps_ == 1 && index_ == 1) v = selected() ? 0.0 : 1.0;  // toggle
    else v = double(index_) / double(steps_);
    beginEdit();
    setValue(float(v));
    valueChanged();
    endEdit();
    invalid();
    e.consumed = true;
  }

 protected:
  virtual bool selected() const {
    if (steps_ == 1 && index_ == 1) return getValue() > 0.5f;
    return int(std::lround(double(getValue()) * steps_)) == index_;
  }

 private:
  int index_, steps_;
  bool led_;
  std::function<bool()> forceLit_;
};

// Power: lit when NOT bypassed; click toggles bypass.
class PowerView : public ButtonView {
 public:
  PowerView(const CRect& size, IControlListener* l, int32_t tag) : ButtonView(size, l, tag, 1, 1, false) {}
  void draw(CDrawContext* ctx) override {
    const CRect r = getViewSize();
    const CPoint c(r.left + r.getWidth() / 2, r.top + r.getHeight() / 2);
    ctx->setDrawMode(kAntiAliasing);
    if (selected()) {
      glow(ctx, c, 9.0, 120);
    } else {
      ctx->setFillColor(CColor(40, 26, 12, 255));
      ctx->drawEllipse(CRect(c.x - 9, c.y - 9, c.x + 9, c.y + 9), kDrawFilled);
    }
    setDirty(false);
  }

 protected:
  bool selected() const override { return getValue() < 0.5f; }  // bypass off = powered
};

// ---------------------------------------------------------------------------- preset widgets
class ArrowView : public CView {
 public:
  ArrowView(const CRect& size, IHost& host, int dir) : CView(size), host_(host), dir_(dir) {}
  void draw(CDrawContext* ctx) override {
    const CRect r = getViewSize();
    const double cx = r.left + r.getWidth() / 2, cy = r.top + r.getHeight() / 2;
    CDrawContext::PointList pts;
    pts.push_back(CPoint(cx - dir_ * 4, cy - 6));
    pts.push_back(CPoint(cx - dir_ * 4, cy + 6));
    pts.push_back(CPoint(cx + dir_ * 5, cy));
    ctx->setDrawMode(kAntiAliasing);
    ctx->setFillColor(CColor(226, 218, 196, 255));
    ctx->drawPolygon(pts, kDrawFilled);
    setDirty(false);
  }
  void onMouseDownEvent(MouseDownEvent& e) override {
    host_.stepPreset(dir_);
    e.consumed = true;
  }

 private:
  IHost& host_;
  int dir_;
};

class NameView : public CView {
 public:
  NameView(const CRect& size, IHost& host) : CView(size), host_(host) {}
  void draw(CDrawContext* ctx) override {
    ctx->setFont(kNormalFont);
    ctx->setFontColor(kAmber);
    ctx->drawString(host_.presetLabel().c_str(), getViewSize(), kCenterText);
    setDirty(false);
  }
  void onMouseWheelEvent(MouseWheelEvent& e) override {
    if (e.deltaY == 0.0) return;
    host_.stepPreset(e.deltaY > 0 ? -1 : 1);
    e.consumed = true;
  }

 private:
  IHost& host_;
};

// ---------------------------------------------------------------------------- VU meter
class VuView : public CView {
 public:
  VuView(const CRect& size, IHost& host, CBitmap* face, CBitmap* glass)
  : CView(size), host_(host), face_(face), glass_(glass) {}

  bool attached(CView* parent) override {
    if (!CView::attached(parent)) return false;
    timer_ = makeOwned<CVSTGUITimer>([this](CVSTGUITimer*) { tick(); }, 16);
    return true;
  }
  bool removed(CView* parent) override {
    timer_ = nullptr;
    return CView::removed(parent);
  }

  static double vuPos(double vu) { return std::pow(10.0, vu / 20.0) / std::pow(10.0, 3.0 / 20.0); }

  void tick() {
    const int mode = int(std::lround(host_.paramNorm(kMeterMode) * 3.0));
    double pos;
    switch (mode) {
      case 0: pos = vuPos(-host_.meterGrDb()); break;                // GR: rests at 0 VU, swings left
      case 1: pos = vuPos(host_.meterLevelDbfs() + 18.0); break;      // +4  : 0 VU = -18 dBFS
      case 2: pos = vuPos(host_.meterLevelDbfs() + 14.0); break;      // +8  : 0 VU = -14 dBFS
      default: pos = 0.0; break;                                      // OFF : needle parked left
    }
    pos = clampd(pos, 0.0, 1.04);
    const double next = shown_ + (pos - shown_) * 0.40;
    if (std::fabs(next - shown_) > 0.0004) {
      shown_ = next;
      invalid();
    }
  }

  void draw(CDrawContext* ctx) override {
    const CRect vs = getViewSize();
    const CRect oldClip = ctx->getClipRect();
    CRect clip(vs);
    clip.bound(oldClip);
    ctx->setClipRect(clip);

    if (face_) face_->draw(ctx, vs);
    const CPoint pivot(vs.left + layout::kVuPivot.x, vs.top + layout::kVuPivot.y);
    const double deg = -layout::kVuSweepDeg + 2.0 * layout::kVuSweepDeg * shown_;
    const CPoint tip = polarPt(pivot, layout::kVuNeedle, deg);
    ctx->setDrawMode(kAntiAliasing);
    ctx->setLineStyle(kLineSolid);
    ctx->setLineWidth(2.0);
    ctx->setFrameColor(CColor(0, 0, 0, 60));  // soft shadow
    ctx->drawLine(CDrawContext::LinePair(CPoint(pivot.x + 2, pivot.y + 2), CPoint(tip.x + 2, tip.y + 2)));
    ctx->setLineWidth(1.7);
    ctx->setFrameColor(CColor(20, 16, 14, 255));
    ctx->drawLine(CDrawContext::LinePair(pivot, tip));
    ctx->setFillColor(CColor(20, 18, 16, 255));
    ctx->drawEllipse(CRect(pivot.x - 8, pivot.y - 8, pivot.x + 8, pivot.y + 8), kDrawFilled);
    if (glass_) glass_->draw(ctx, vs);

    ctx->setClipRect(oldClip);
    setDirty(false);
  }

 private:
  IHost& host_;
  SharedPointer<CBitmap> face_, glass_;
  SharedPointer<CVSTGUITimer> timer_;
  double shown_ = 0.708;
};

}  // namespace

// ---------------------------------------------------------------------------- panel
PanelView::PanelView(IHost& host, IControlListener* listener, const IUIDescription* desc)
: CViewContainer(CRect(0, 0, layout::kWidth, layout::kHeight)), host_(host) {
  using namespace layout;
  if (desc) setBackground(desc->getBitmap("panel"));
  setTransparency(false);

  auto track = [&](CControl* c, unsigned id) {
    const ParamSpec* s = findSpec(id);
    c->setDefaultValue(s ? float(s->defNorm) : 0.f);
    c->setValue(float(host_.paramNorm(id)));
    controls_.push_back(c);
    addView(c);
  };
  auto knob = [&](unsigned id, const Pt& c, double cap, double ring, double half, double down, double readoutDy,
                  const char* bitmap) {
    const CRect r(c.x - half, c.y - half, c.x + half, c.y + down);
    track(new KnobView(r, listener, int32_t(id), desc ? desc->getBitmap(bitmap) : nullptr, cap, ring, half, readoutDy,
                       host_),
          id);
  };
  knob(kInput, kInputCenter, kInputCap, kInputRing, kInputViewHalf, kInputViewDown, 22, "knob_big");
  knob(kOutput, kOutputCenter, kOutputCap, kOutputRing, kOutputViewHalf, kOutputViewDown, 22, "knob_big");
  knob(kAttack, kAttackCenter, kAttackCap, kAttackRing, kAttackViewHalf, kAttackViewDown, 12, "knob_small");
  knob(kRelease, kReleaseCenter, kReleaseCap, kReleaseRing, kReleaseViewHalf, kReleaseViewDown, 12, "knob_small");
  knob(kMix, kMixCenter, kMixCap, kMixRing, kMixViewHalf, kMixViewDown, 13, "knob_mini");
  knob(kHpf, kHpfCenter, kHpfCap, kHpfRing, kHpfViewHalf, kHpfViewDown, 13, "knob_mini");
  knob(kColor, kColorCenter, kColorCap, kColorRing, kColorViewHalf, kColorViewDown, 13, "knob_mini");

  auto rectAt = [](const Rect& first, double step, bool vertical, int i) {
    const double dx = vertical ? 0 : step * i, dy = vertical ? step * i : 0;
    return CRect(first.x + dx, first.y + dy, first.x + dx + first.w, first.y + dy + first.h);
  };
  // ratio (exclusive) + ALL (toggle)
  auto allOn = [this] { return host_.paramNorm(kAll) > 0.5; };
  for (int i = 0; i < 4; ++i)
    track(new ButtonView(rectAt(kRatioBtnFirst, kRatioBtnStep, kRatioBtnVertical, i), listener, int32_t(kRatio), i, 3,
                         true, allOn),
          kRatio);
  track(new ButtonView(rectAt(kRatioBtnFirst, kRatioBtnStep, kRatioBtnVertical, 4), listener, int32_t(kAll), 1, 1, true),
        kAll);
  for (int i = 0; i < 4; ++i)
    track(new ButtonView(rectAt(kMeterBtnFirst, kMeterBtnStep, kMeterBtnVertical, i), listener, int32_t(kMeterMode), i, 3,
                         true),
          kMeterMode);
  for (int i = 0; i < 3; ++i)
    track(new ButtonView(rectAt(kDetBtnFirst, kDetBtnStep, kDetBtnVertical, i), listener, int32_t(kDetector), i, 2, false),
          kDetector);
  for (int i = 0; i < 4; ++i)
    track(new ButtonView(rectAt(kOsBtnFirst, kOsBtnStep, kOsBtnVertical, i), listener, int32_t(kQuality), i, 3, false),
          kQuality);
  track(new PowerView(CRect(kPowerCenter.x - kPowerRadius, kPowerCenter.y - kPowerRadius, kPowerCenter.x + kPowerRadius,
                            kPowerCenter.y + kPowerRadius),
                      listener, int32_t(kBypass)),
        kBypass);

  // preset
  addView(new ArrowView(CRect(kPrevBtn.x, kPrevBtn.y, kPrevBtn.x + kPrevBtn.w, kPrevBtn.y + kPrevBtn.h), host_, -1));
  addView(new ArrowView(CRect(kNextBtn.x, kNextBtn.y, kNextBtn.x + kNextBtn.w, kNextBtn.y + kNextBtn.h), host_, +1));
  nameView_ = new NameView(CRect(kNameBox.x, kNameBox.y, kNameBox.x + kNameBox.w, kNameBox.y + kNameBox.h), host_);
  addView(nameView_);

  // VU meter on top of the panel
  addView(new VuView(CRect(kVuWindow.x, kVuWindow.y, kVuWindow.x + kVuWindow.w, kVuWindow.y + kVuWindow.h), host_,
                     desc ? desc->getBitmap("vu_face") : nullptr, desc ? desc->getBitmap("vu_glass") : nullptr));

  host_.registerPanel(this);
}

PanelView::~PanelView() { host_.unregisterPanel(this); }

void PanelView::paramChanged(unsigned id, double norm) {
  for (CControl* c : controls_) {
    if (unsigned(c->getTag()) == id) {
      c->setValue(float(norm));
      c->invalid();
    } else if (id == kAll && unsigned(c->getTag()) == kRatio) {
      c->invalid();  // ratio buttons follow the ALL modifier
    }
  }
}

void PanelView::presetChanged() {
  if (nameView_) nameView_->invalid();
}

}  // namespace bf::ui
