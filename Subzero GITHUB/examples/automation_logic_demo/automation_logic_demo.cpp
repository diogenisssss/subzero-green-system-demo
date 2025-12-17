// Demo-only automation controller (hardware-free)
// -----------------------------------------------
// This file is intentionally standalone so you can read/review the automation logic without:
// - Arduino/ESP32 SDK
// - WiFi/web server
// - device persistence
//
// It mirrors the *shape* of the firmware decision logic:
// - a time-based state machine (IDLE, PUMP_CYCLE, FAN_CYCLE, COOLDOWN)
// - an "AI factor" that gently shifts targets/PWM (bounded, explainable)
//
// NOT PRODUCTION CODE.

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace demo {

static int clampInt(int v, int lo, int hi) {
  return std::max(lo, std::min(hi, v));
}

struct Config {
  // Primary targets
  float targetTempC = 5.0f;
  float safetyTempC = 10.0f;

  // Idle circulation pattern: keep fluid moving without constant pumping
  std::uint64_t idlePumpRunMs = 40'000;
  std::uint64_t idlePumpRestMs = 20'000;
  int idlePumpPWM = 204;  // 80% (204/255)

  // Cycle timing guards
  std::uint64_t idleMaxMs = 600'000;        // after this, do a pump cycle anyway
  std::uint64_t pumpCycleMaxMs = 300'000;   // pump max continuous cycle
  std::uint64_t fanCycleMaxMs = 300'000;    // fan max continuous cycle
  std::uint64_t cooldownMs = 120'000;       // anti-thrashing

  // Fan trigger (high-temp response)
  float fanTriggerOffsetC = 2.5f;           // target + 2.5C
  std::uint64_t fanMinDelayMs = 30'000;     // don't trigger instantly on boot/transition

  // "AI" tuning knobs (bounded, explainable)
  float aiOptimizationFactor = 1.0f;        // e.g. 0.8 .. 1.3
  float learnedEfficiencyBoost = 0.0f;      // e.g. 0.0 .. 0.5
};

enum class Mode : std::uint8_t { Off, Manual, Auto };
enum class AutoState : std::uint8_t { Idle, PumpCycle, FanCycle, Cooldown };

struct Inputs {
  Mode mode = Mode::Auto;
  float liquidTempC = 25.0f;
  float externalTempC = 20.0f;
};

struct Outputs {
  int pumpPWM = 0;     // 0..255
  bool fanOn = false;
  std::string state;
  std::string note;
};

class AutomationController {
 public:
  explicit AutomationController(Config cfg) : cfg_(cfg) {}

  Outputs update(std::uint64_t nowMs, const Inputs& in) {
    Outputs out{};

    // Safety / validity checks (illustrative)
    if (!isValidTemp(in.liquidTempC)) {
      out.state = "SAFE";
      out.note = "Invalid liquid temp reading -> actuators OFF";
      return out;
    }

    if (in.liquidTempC > cfg_.safetyTempC) {
      // Demonstration of "safety overrides automation".
      // In real systems you would also add alarms, latching behavior, etc.
      out.state = "SAFETY_OVERRIDE";
      out.fanOn = true;
      out.pumpPWM = 0;
      out.note = "Liquid temp above safety limit -> fan ON, pump OFF";
      return out;
    }

    if (in.mode != Mode::Auto) {
      // For demo purposes we treat non-AUTO as "no automation decisions".
      // Manual control would be done elsewhere.
      current_ = AutoState::Idle;
      stateStartMs_ = nowMs;
      idleCycleStartMs_ = 0;
      idlePumpRunning_ = false;
      out.state = (in.mode == Mode::Off) ? "OFF" : "MANUAL";
      out.note = "Automation disabled";
      return out;
    }

    // AUTO mode
    if (stateStartMs_ == 0) stateStartMs_ = nowMs;

    // Highest-priority fan trigger: handle high-temp events quickly.
    if (shouldStartFanCycle(nowMs, in.liquidTempC) && current_ != AutoState::FanCycle) {
      current_ = AutoState::FanCycle;
      stateStartMs_ = nowMs;
      fanActive_ = true;
    }

    switch (current_) {
      case AutoState::Idle:
        return handleIdle(nowMs, in);
      case AutoState::PumpCycle:
        return handlePumpCycle(nowMs, in);
      case AutoState::FanCycle:
        return handleFanCycle(nowMs, in);
      case AutoState::Cooldown:
        return handleCooldown(nowMs, in);
    }

    // Unreachable, but keep compiler happy.
    out.state = "UNKNOWN";
    return out;
  }

 private:
  static bool isValidTemp(float t) {
    // Demo heuristic: common DS18B20 error sentinel is around -127C.
    return t > -100.0f && t < 125.0f;
  }

  float computeCoolingTargetC(float liquidTempC, float externalTempC) const {
    (void)liquidTempC;
    (void)externalTempC;

    // Mirror firmware behavior: baseOffset is negative => cool below targetTemp.
    const float baseOffset = -1.0f;
    const float totalBoost = cfg_.aiOptimizationFactor + cfg_.learnedEfficiencyBoost;
    float target = cfg_.targetTempC + (baseOffset * totalBoost);

    // Safety clamp: never chase crazy low targets in demo logic.
    return std::max(target, -2.0f);
  }

  int computePumpPWM(float liquidTempC, float coolingTargetC, float externalTempC) const {
    const int basePWM = 77; // ~30%

    const float tempDiff = liquidTempC - coolingTargetC;
    int tempAdj = 0;
    if (tempDiff > 3.0f) tempAdj = 50;
    else if (tempDiff > 1.5f) tempAdj = 25;
    else if (tempDiff < 0.5f) tempAdj = -25;

    int extAdj = 0;
    if (externalTempC < (cfg_.targetTempC - 5.0f)) extAdj = -20;
    else if (externalTempC > cfg_.targetTempC) extAdj = 30;

    const float totalBoost = cfg_.aiOptimizationFactor + cfg_.learnedEfficiencyBoost;
    const int aiBase = static_cast<int>(basePWM * totalBoost);
    const int pwm = aiBase + tempAdj + extAdj;

    // Bound outputs (avoid stressing hardware / reduce risk in demo).
    return clampInt(pwm, 50, 200);
  }

  bool shouldStartFanCycle(std::uint64_t nowMs, float liquidTempC) const {
    const float threshold = cfg_.targetTempC + cfg_.fanTriggerOffsetC;
    const bool highTemp = liquidTempC > threshold;
    const bool minDelay = (nowMs - stateStartMs_) > cfg_.fanMinDelayMs;
    return highTemp && minDelay && !fanActive_;
  }

  Outputs handleIdle(std::uint64_t nowMs, const Inputs& in) {
    Outputs out{};

    const std::uint64_t idleTime = nowMs - stateStartMs_;
    const bool aboveTarget = (in.liquidTempC > cfg_.targetTempC);

    // If above target or we've been idle for too long, start an active pump cycle.
    if (aboveTarget || idleTime >= cfg_.idleMaxMs) {
      current_ = AutoState::PumpCycle;
      stateStartMs_ = nowMs;
      idleCycleStartMs_ = 0;
      idlePumpRunning_ = false;
      return handlePumpCycle(nowMs, in);
    }

    // Otherwise: duty-cycle circulation
    if (idleCycleStartMs_ == 0) {
      idleCycleStartMs_ = nowMs;
      idlePumpRunning_ = true;
    }

    const std::uint64_t cycleElapsed = nowMs - idleCycleStartMs_;
    if (idlePumpRunning_) {
      if (cycleElapsed >= cfg_.idlePumpRunMs) {
        idlePumpRunning_ = false;
        idleCycleStartMs_ = nowMs;
      }
    } else {
      if (cycleElapsed >= cfg_.idlePumpRestMs) {
        idlePumpRunning_ = true;
        idleCycleStartMs_ = nowMs;
      }
    }

    out.state = "IDLE";
    out.fanOn = false;
    out.pumpPWM = idlePumpRunning_ ? cfg_.idlePumpPWM : 0;
    out.note = idlePumpRunning_ ? "Idle circulation burst" : "Idle rest";
    return out;
  }

  Outputs handlePumpCycle(std::uint64_t nowMs, const Inputs& in) {
    Outputs out{};

    const float coolingTargetC = computeCoolingTargetC(in.liquidTempC, in.externalTempC);
    const bool targetReached = (in.liquidTempC <= coolingTargetC);
    const bool timeout = ((nowMs - stateStartMs_) >= cfg_.pumpCycleMaxMs);

    if (targetReached || timeout) {
      current_ = AutoState::Cooldown;
      stateStartMs_ = nowMs;
      idleCycleStartMs_ = 0;
      idlePumpRunning_ = false;
      out.state = "COOLDOWN";
      out.note = targetReached ? "Cooling target reached" : "Pump cycle timeout";
      return out;
    }

    out.state = "PUMP_CYCLE";
    out.fanOn = true; // mirrors firmware’s “pump + fan” behavior during cycle
    out.pumpPWM = computePumpPWM(in.liquidTempC, coolingTargetC, in.externalTempC);
    out.note = "Active cooling";
    return out;
  }

  Outputs handleFanCycle(std::uint64_t nowMs, const Inputs& in) {
    Outputs out{};

    const float coolingTargetC = computeCoolingTargetC(in.liquidTempC, in.externalTempC);
    const bool targetReached = (in.liquidTempC <= coolingTargetC);
    const bool timeout = ((nowMs - stateStartMs_) >= cfg_.fanCycleMaxMs);

    out.state = "FAN_CYCLE";
    out.fanOn = true;
    out.pumpPWM = 0;
    out.note = "High-temp response (fan only)";

    if (targetReached || timeout) {
      current_ = AutoState::Cooldown;
      stateStartMs_ = nowMs;
      fanActive_ = false;
      out.note = targetReached ? "Cooling target reached" : "Fan cycle timeout";
    }
    return out;
  }

  Outputs handleCooldown(std::uint64_t nowMs, const Inputs& /*in*/) {
    Outputs out{};
    out.state = "COOLDOWN";
    out.fanOn = false;
    out.pumpPWM = 0;
    out.note = "Anti-thrashing delay";

    if ((nowMs - stateStartMs_) >= cfg_.cooldownMs) {
      current_ = AutoState::Idle;
      stateStartMs_ = nowMs;
      idleCycleStartMs_ = 0;
      idlePumpRunning_ = false;
    }
    return out;
  }

  Config cfg_;
  AutoState current_ = AutoState::Idle;
  std::uint64_t stateStartMs_ = 0;

  // Idle duty-cycle tracking
  std::uint64_t idleCycleStartMs_ = 0;
  bool idlePumpRunning_ = false;

  // Fan trigger tracking (avoid repeated triggers)
  bool fanActive_ = false;
};

} // namespace demo

int main() {
  demo::Config cfg;
  cfg.targetTempC = 5.0f;
  cfg.safetyTempC = 10.0f;
  cfg.aiOptimizationFactor = 1.05f;
  cfg.learnedEfficiencyBoost = 0.10f;

  demo::AutomationController controller(cfg);

  demo::Inputs in;
  in.mode = demo::Mode::Auto;
  in.externalTempC = 18.0f;

  // Simple temperature scenario: start hot, then cool down gradually.
  float temp = 9.5f;
  std::uint64_t now = 0;

  std::cout << "t(s)  tempC  state        pumpPWM  fan  note\n";
  std::cout << "----  -----  -----------  ------  ---  ------------------------------\n";

  for (int step = 0; step < 120; step++) { // 120 * 5s = 10 minutes
    in.liquidTempC = temp;
    const auto out = controller.update(now, in);

    std::cout << std::setw(4) << (now / 1000)
              << "  " << std::fixed << std::setprecision(1) << std::setw(5) << temp
              << "  " << std::setw(11) << out.state
              << "  " << std::setw(6) << out.pumpPWM
              << "  " << std::setw(3) << (out.fanOn ? "ON" : "OFF")
              << "  " << out.note
              << "\n";

    // Extremely simplified "plant" model:
    // - Pump+fan cool faster than fan-only
    // - Idle circulation changes very little
    if (out.state == "PUMP_CYCLE") temp -= 0.20f;
    else if (out.state == "FAN_CYCLE") temp -= 0.05f;
    else temp -= 0.01f;

    now += 5'000;
  }
}


