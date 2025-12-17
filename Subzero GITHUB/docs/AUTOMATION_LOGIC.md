## Automation logic (Demo explanation)

### Modes
- **OFF**: actuators off
- **MANUAL**: direct operator control (demo)
- **AUTO**: state machine decides pump/fan outputs based on temperatures and timers

### AUTO state machine (conceptual)
- **IDLE**
  - Goal: maintain circulation with minimal energy and avoid stagnation.
  - Behavior: short “circulation bursts” (e.g., 40s on / 20s off) until either:
    - liquid temp rises above target → start PUMP_CYCLE
    - an idle timer expires → start PUMP_CYCLE
- **PUMP_CYCLE**
  - Goal: actively drive the temperature toward a computed cooling target.
  - Behavior: pump PWM is computed from temperature error + external conditions + “AI factor”.
  - Stops when:
    - cooling target is reached, or
    - max cycle time is reached (prevents long continuous runs)
- **FAN_CYCLE**
  - Goal: handle high-temp events (quick response).
  - Behavior: fan runs without pump; stops on target reached or timeout.
- **COOLDOWN**
  - Goal: prevent rapid cycling (hardware stress + unstable control).
  - Behavior: wait for a short cooldown, then transition back to IDLE.

### “AI / pattern learning” (demo intent)
This demo uses a simple *multiplier* to adjust:
- the computed cooling target (more/less aggressive offset)
- the pump PWM base level

The important point is the *interface*: deterministic control stays explicit, while the “AI” knob is:
- bounded
- explainable
- optional


