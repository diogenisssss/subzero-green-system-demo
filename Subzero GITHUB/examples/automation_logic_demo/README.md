## Automation Logic Demo (Non-production)

This folder contains **simplified**, **hardware-free** example code that mirrors the firmware’s
high-level automation decisions.

### Why this exists
- The real firmware has ESP32/Arduino dependencies, web UI, WiFi, storage, etc.
- For GitHub showcasing, it’s easier to review the *decision logic* in isolation.

### Files
- `automation_logic_demo.cpp`: a tiny simulator + state machine controller

### Run locally (desktop)
From the repo root:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/automation_logic_demo/automation_logic_demo.cpp -o automation_demo
./automation_demo
```


