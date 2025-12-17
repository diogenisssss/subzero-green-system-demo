## Architecture (Demo / Showcase)

### Goals
- Make the automation logic understandable and reviewable.
- Show how an IoT edge controller can combine:
  - deterministic safety/controls
  - lightweight “AI” tuning (heuristics + learned factors)
  - a small local UI for visibility and configuration

### System components
- **Sensors**
  - Liquid temperature (control variable)
  - External temperature/humidity (context variable; smoothing filters)
- **Actuators**
  - Pump via PWM (variable power)
  - Fan via relay (on/off)
- **Control loop**
  - Runs on a fixed cadence (e.g., every 5 seconds for sensor reads)
  - Updates the automation state machine and writes actuator outputs
- **Automation state machine**
  - AUTO / MANUAL / OFF modes
  - Internal states such as IDLE, PUMP_CYCLE, FAN_CYCLE, COOLDOWN
- **Local UI**
  - Embedded web server renders HTML templates for dashboard/settings/health
  - Exposes read-only status endpoints and configuration actions
- **Persistence**
  - Device preferences store target temp, safety temp, and other user settings

### Data flow
1. Read sensors → validate + smooth (reject spikes)
2. Compute targets (target temp, safety temp) + apply “AI factor”
3. Run state machine to decide:
   - Pump PWM value
   - Fan relay state
4. Publish status to UI and store updated settings (if changed)

### Design principles (what this demo tries to illustrate)
- **Deterministic first**: safety thresholds and cooldowns are explicit and easy to audit.
- **Time-based state transitions**: reduce jitter and rapid toggling.
- **Bounded outputs**: clamp PWM to safe ranges; avoid runaway commands.
- **Graceful degradation**: if sensor readings are invalid, avoid unsafe actions and surface alerts.


