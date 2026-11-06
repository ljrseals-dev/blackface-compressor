// Host-less test for the Blackface engine. Build: see tests/run_tests.sh
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "BlackfaceEngine.h"

static int g_fail = 0, g_pass = 0;
static void check(bool c, const std::string& n) {
  std::printf("%s %s\n", c ? "PASS" : "FAIL", n.c_str());
  (c ? g_pass : g_fail)++;
}

static const double kSr = 48000.0;

// steady-state helpers ------------------------------------------------------
struct Run {
  std::vector<float> l, r, ol, orr;
};

static Run render(bf::Engine& e, double ampL, double ampR, double seconds, double freq = 1000.0,
                  const std::vector<float>* keyL = nullptr) {
  const int total = int(seconds * kSr), blk = 256;
  Run o;
  o.l.resize(total); o.r.resize(total); o.ol.resize(total); o.orr.resize(total);
  for (int i = 0; i < total; ++i) {
    const double ph = std::sin(2 * bf::kPiD * freq * i / kSr);
    o.l[i] = float(ampL * ph);
    o.r[i] = float(ampR * ph);
  }
  for (int s = 0; s < total; s += blk) {
    const int n = std::min(blk, total - s);
    float* in[2] = {o.l.data() + s, o.r.data() + s};
    float* out[2] = {o.ol.data() + s, o.orr.data() + s};
    float* sc[2] = {nullptr, nullptr};
    std::vector<float> k;
    if (keyL) {
      sc[0] = const_cast<float*>(keyL->data()) + s;
      sc[1] = sc[0];
    }
    e.process<float>(in, out, 2, n, keyL ? sc : nullptr, keyL ? 2 : 0);
  }
  return o;
}

static double peakTail(const std::vector<float>& v, double frac = 0.25) {
  double p = 0;
  for (size_t i = size_t(v.size() * (1.0 - frac)); i < v.size(); ++i) p = std::max(p, double(std::fabs(v[i])));
  return p;
}
static double db(double x) { return 20.0 * std::log10(std::max(x, 1e-9)); }

static bf::Settings base() {
  bf::Settings s;
  s.inputDb = 0; s.outputDb = 0; s.attackUs = 100; s.releaseMs = 200; s.ratio = 0;
  s.all = false; s.mixPct = 100; s.hpfHz = 0; s.color = 0; s.quality = 1; s.detector = 0;
  return s;
}

static double steadyGain(bf::Settings s, double ampDb, double* grOut = nullptr) {
  bf::Engine e;
  e.prepare(kSr, 256);
  e.setSettings(s);
  const double amp = std::pow(10.0, ampDb / 20.0);
  auto r = render(e, amp, amp, 2.0);
  if (grOut) *grOut = e.meters().grDb;
  return db(peakTail(r.ol)) - ampDb;
}

int main() {
  // 1. parameter mapping round trip
  for (int i = 0; i < bf::kNumSpecs; ++i) {
    const auto& sp = bf::kSpecs[i];
    for (double n : {0.0, 0.25, 0.5, 0.75, 1.0}) {
      double p = bf::toPlain(sp, n), n2 = bf::toNorm(sp, p);
      if (sp.stepCount == 0 && std::fabs(n - n2) > 1e-9) { check(false, std::string("roundtrip ") + sp.name); }
    }
    double nn = 0;
    std::string t = bf::toText(sp, sp.defNorm);
    check(bf::fromText(sp, t, nn) || sp.id == bf::kColor, std::string("text parse ") + sp.name);
  }
  check(std::fabs(bf::toPlain(bf::kSpecs[2], 0.0) - 800.0) < 1e-9 && std::fabs(bf::toPlain(bf::kSpecs[2], 1.0) - 20.0) < 1e-9,
        "attack range 800..20 us (7 = fastest)");
  check(std::fabs(bf::toPlain(bf::kSpecs[3], 0.0) - 1100.0) < 1e-9 && std::fabs(bf::toPlain(bf::kSpecs[3], 1.0) - 50.0) < 1e-9,
        "release range 1100..50 ms (7 = fastest)");

  // 2. small signal, input 0: transparent-ish (no GR)
  {
    double gr = 0;
    double g = steadyGain(base(), -30.0, &gr);
    check(std::fabs(g) < 0.6 && gr < 0.2, "quiet signal passes at ~unity, no GR (" + std::to_string(g) + " dB)");
  }

  // 3. GR rises with input drive; ratio order 4 < 8 < 12 < 20 at same drive
  {
    double lastGr = -1; bool mono = true;
    for (double in : {0.0, 10.0, 20.0, 30.0}) {
      auto s = base(); s.inputDb = in;
      double gr; steadyGain(s, -12.0, &gr);
      std::printf("   input +%2.0f dB -> GR %.2f dB\n", in, gr);
      if (gr < lastGr - 0.05) mono = false;
      lastGr = gr;
    }
    check(mono && lastGr > 3.0, "GR grows monotonically with INPUT");
    double g[4];
    for (int r = 0; r < 4; ++r) {
      auto s = base(); s.inputDb = 20; s.ratio = r;
      steadyGain(s, -12.0, &g[r]);
      std::printf("   ratio idx %d -> GR %.2f dB\n", r, g[r]);
    }
    check(g[0] < g[1] && g[1] < g[2] && g[2] < g[3] + 0.01, "GR ordering 4:1 < 8:1 < 12:1 <= 20:1");
    auto s = base(); s.inputDb = 20; s.ratio = 0; s.all = true;
    double ga; steadyGain(s, -12.0, &ga);
    std::printf("   all-buttons -> GR %.2f dB\n", ga);
    check(ga > g[0], "ALL buttons compresses harder than 4:1");
  }

  // 4. attack speed: fast attack catches a burst more than slow attack
  {
    auto run = [&](double atkUs) {
      bf::Engine e; auto s = base(); s.inputDb = 24; s.attackUs = atkUs; s.releaseMs = 500;
      e.prepare(kSr, 256); e.setSettings(s);
      auto r = render(e, 0.5, 0.5, 0.01);  // 10 ms burst
      return peakTail(r.ol, 1.0);
    };
    check(run(20) < run(800), "fast attack (20us) clamps a 10 ms burst more than slow attack (800us)");
  }

  // 5. release speed
  {
    auto grAfter = [&](double relMs) {
      bf::Engine e; auto s = base(); s.inputDb = 24; s.releaseMs = relMs;
      e.prepare(kSr, 256); e.setSettings(s);
      render(e, 0.5, 0.5, 1.0);
      render(e, 0.001, 0.001, 0.15);
      return e.meters().grDb;
    };
    check(grAfter(50) < grAfter(1100), "fast release recovers faster than slow release");
  }

  // 6. mix 0 = dry; bypass = dry
  {
    auto s = base(); s.inputDb = 30; s.mixPct = 0;
    bf::Engine e; e.prepare(kSr, 256); e.setSettings(s);
    auto r = render(e, 0.3, 0.3, 0.5);
    double d = 0; for (size_t i = 0; i < r.l.size(); ++i) d = std::max(d, double(std::fabs(r.l[i] - r.ol[i])));
    check(d < 1e-4, "MIX 0 % is dry");
    s.mixPct = 100; s.bypass = true;
    bf::Engine e2; e2.prepare(kSr, 256); e2.setSettings(s);
    auto r2 = render(e2, 0.3, 0.3, 1.0);
    d = 0; for (size_t i = r2.l.size() / 2; i < r2.l.size(); ++i) d = std::max(d, double(std::fabs(r2.l[i] - r2.ol[i])));
    check(d < 1e-3, "BYPASS settles to dry");
  }

  // 7. stereo link vs internal: loud L, quiet R
  {
    auto run = [&](int det) {
      auto s = base(); s.inputDb = 20; s.detector = det;
      bf::Engine e; e.prepare(kSr, 256); e.setSettings(s);
      auto r = render(e, 0.5, 0.05, 1.5);
      return db(peakTail(r.orr)) - db(0.05);
    };
    double gi = run(0), gl = run(1);
    std::printf("   quiet-R gain: internal %.2f dB, linked %.2f dB\n", gi, gl);
    check(gl < gi - 1.0, "LINK: loud L channel pulls quiet R channel down");
  }

  // 8. external key ducks program
  {
    auto s = base(); s.inputDb = 20; s.detector = 2;
    const int total = int(1.5 * kSr);
    std::vector<float> key(total, 0.0f);
    for (int i = total / 2; i < total; ++i) key[i] = float(0.5 * std::sin(2 * bf::kPiD * 200.0 * i / kSr));
    bf::Engine e; e.prepare(kSr, 256); e.setSettings(s);
    auto r = render(e, 0.05, 0.05, 1.5, 1000.0, &key);
    double before = peakTail(std::vector<float>(r.ol.begin(), r.ol.begin() + total / 2), 0.25);
    double after = peakTail(r.ol, 0.25);
    std::printf("   external key: before %.1f dB, during key %.1f dB\n", db(before), db(after));
    check(after < before * 0.8, "EXTERNAL key ducks the program");
    auto s2 = s; s2.detector = 2;
    bf::Engine e2; e2.prepare(kSr, 256); e2.setSettings(s2);
    auto r2 = render(e2, 0.05, 0.05, 0.5);  // no sidechain connected
    check(std::isfinite(peakTail(r2.ol)), "EXTERNAL with no key connected stays finite");
  }

  // 9. meters
  {
    auto s = base(); s.inputDb = 20; s.ratio = 3;
    bf::Engine e; e.prepare(kSr, 256); e.setSettings(s);
    render(e, 0.25, 0.25, 1.0);
    auto m = e.meters();
    std::printf("   meters: GR %.2f dB, level %.1f dBFS\n", m.grDb, m.levelDbfs);
    check(m.grDb > 3 && m.levelDbfs > -40 && m.levelDbfs < 6, "meters report plausible GR and level");
    render(e, 0.0, 0.0, 1.5);
    check(e.meters().grDb < 0.5 && e.meters().levelDbfs < -50, "meters fall back after signal stops");
  }

  // 10. oversampling settings all finite, mono works
  {
    for (int q = 0; q < 4; ++q) {
      auto s = base(); s.inputDb = 30; s.quality = q; s.color = 0.6; s.hpfHz = 120; s.all = true;
      bf::Engine e; e.prepare(kSr, 128); e.setSettings(s);
      auto r = render(e, 0.9, 0.9, 0.5, 220.0);
      bool fin = true; for (float v : r.ol) if (!std::isfinite(v)) fin = false;
      check(fin, "finite output, oversampling idx " + std::to_string(q));
    }
    bf::Engine e; e.prepare(kSr, 64); e.setSettings(base());
    std::vector<float> m(64, 0.1f), o(64);
    float* in[1] = {m.data()}; float* out[1] = {o.data()};
    e.process<float>(in, out, 1, 64, nullptr, 0);
    check(std::isfinite(o[63]), "mono path runs");
  }

  // 11. presets load into sane ranges
  {
    bool ok = true;
    for (int i = 0; i < bf::kNumPresets; ++i) {
      const auto& p = bf::kPresets[i];
      ok &= p.inputDb >= 0 && p.inputDb <= 40 && p.outputDb >= -24 && p.outputDb <= 12 &&
            p.attackUs >= 20 && p.attackUs <= 800 && p.releaseMs >= 50 && p.releaseMs <= 1100 &&
            p.ratio >= 0 && p.ratio <= 3 && p.mixPct >= 0 && p.mixPct <= 100 && p.hpfHz >= 0 && p.hpfHz <= 300;
    }
    check(ok, "all factory presets inside parameter ranges");
  }


  // 13. canary-driven additions (bugs the first 35 checks missed)
  {
    // output gain is exact and has the right sign
    for (double og : {-12.0, 6.0}) {
      auto s = base(); s.outputDb = og;
      double g = steadyGain(s, -40.0);
      check(std::fabs(g - og) < 0.6, "OUTPUT " + std::to_string(int(og)) + " dB gives that gain on a quiet signal (" + std::to_string(g) + ")");
    }
    // each ratio lands near the ideal GR = over * (1 - 1/R) (+11 dB over threshold)
    const double R[4] = {4, 8, 12, 20};
    double gr[4];
    for (int r = 0; r < 4; ++r) {
      auto s = base(); s.inputDb = 20; s.ratio = r;
      steadyGain(s, -12.0, &gr[r]);
      const double ideal = 11.0 * (1.0 - 1.0 / R[r]);
      check(std::fabs(gr[r] - ideal) < 1.5, "ratio " + std::to_string(int(R[r])) + ":1 GR " + std::to_string(gr[r]) + " near ideal " + std::to_string(ideal));
    }
    check(gr[3] > gr[2] + 0.15, "20:1 is measurably stronger than 12:1");
    // link works from either channel
    auto run = [&](double aL, double aR, int det) {
      auto s = base(); s.inputDb = 20; s.detector = det;
      bf::Engine e; e.prepare(kSr, 256); e.setSettings(s);
      auto r = render(e, aL, aR, 1.5);
      return std::make_pair(db(peakTail(r.ol)) - db(aL), db(peakTail(r.orr)) - db(aR));
    };
    auto a = run(0.05, 0.5, 0), b = run(0.05, 0.5, 1);
    check(b.first < a.first - 1.0, "LINK: loud R channel pulls quiet L channel down");
    // oversampling setting actually changes the processing
    auto q = [&](int quality) {
      auto s = base(); s.inputDb = 30; s.color = 0.8; s.quality = quality;
      bf::Engine e; e.prepare(kSr, 256); e.setSettings(s);
      return render(e, 0.9, 0.9, 0.3, 11000.0).ol;
    };
    auto o1 = q(0), o8 = q(3);
    double d2 = 0; for (size_t i = 0; i < o1.size(); ++i) d2 += double(o1[i] - o8[i]) * double(o1[i] - o8[i]);
    check(std::sqrt(d2 / o1.size()) > 1e-3, "1x and 8x oversampling produce different output on a hot 11 kHz tone");
  }

  // 12. CPU
  {
    auto s = base(); s.inputDb = 20; s.quality = 3;
    bf::Engine e; e.prepare(kSr, 512); e.setSettings(s);
    auto t0 = std::chrono::steady_clock::now();
    render(e, 0.4, 0.4, 5.0, 440.0);
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("   stereo 8x: %.1f s audio in %.2f s (%.1f%% of one core)\n", 5.0, sec, sec / 5.0 * 100);
    check(sec / 5.0 < 0.5, "8x stereo runs well under real time");
  }

  std::printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
