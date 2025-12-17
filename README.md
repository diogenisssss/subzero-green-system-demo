# Subzero — AI Automation & Energy Optimization System (Public Demo)

**This is a SAFE, sanitized, non-production GitHub showcase version.**  
It exists to demonstrate **system design** and **automation logic** (control loops, state machines, and AI-oriented tuning concepts), not to provide a deployable product.

---

## What this project is

- **IoT edge controller (ESP32 / Arduino) – architecture & logic showcase**  
  Sensor-driven control loops and actuator orchestration (pump PWM and fan relay), presented at a design and logic level.

- **Automation logic**  
  A state-machine–based control system that balances stability, safety, and energy efficiency:
  - IDLE circulation (short duty cycle) to keep fluid moving
  - PUMP cycles when temperature exceeds target thresholds
  - FAN-only cycles for high-temperature events
  - COOLDOWN states to prevent rapid cycling and hardware stress

- **AI / pattern-learning hooks (conceptual)**  
  Lightweight tuning factors designed to shift cooling targets and PWM behavior over time, enabling future extensions toward predictive or adaptive optimization.

- **Local web UI (concept)**  
  Embedded web interface for system status and configuration, described at an architectural level for demonstration purposes.

---

## High-level architecture

```
Sensors (liquid temperature, external temperature / humidity)
                    |
                    v
            Control Loop (periodic execution)
                    |
                    v
        Automation State Machine (AUTO / MANUAL / OFF)
                    |
                    v
      Actuators (pump PWM, fan relay)  --->  Web UI (status / configuration)
                    |
                    v
        Local persistence (device preferences / NVS)
```

---

## Repository layout (showcase-friendly)

- `examples/`  
  Simplified, non-production automation logic examples (hardware-agnostic).

- `docs/`  
  System architecture, automation design notes, and security sanitization details.

---

## Demo / non-production disclaimers

- **No secrets are committed**  
  Credentials, fixed SSIDs, passwords, OTA authentication, and real environment values are intentionally removed or replaced with placeholders.

- **Security is not production-hardened**  
  Any authentication or UI flows shown are illustrative and must not be considered secure.

- **Safety logic is illustrative**  
  Patterns such as limits, cooldowns, and timeouts are demonstrated conceptually and are not part of a certified safety system.

---

## Where to look (recommended reading order)

- **Simplified automation logic (no hardware dependencies)**  
  `examples/automation_logic_demo/automation_logic_demo.cpp`

- **Architecture overview**  
  `docs/ARCHITECTURE.md`

- **Automation / decision logic notes**  
  `docs/AUTOMATION_LOGIC.md`

- **Security & sanitization notes**  
  `docs/SECURITY_SANITIZATION.md`

---

## Note on production firmware

The production ESP32 firmware is intentionally **not included** in this public repository.

This demo focuses on:
- automation logic
- control-flow and state-machine design
- system architecture
- extensibility toward AI-driven optimization

Firmware sources are omitted to protect sensitive implementation details.
