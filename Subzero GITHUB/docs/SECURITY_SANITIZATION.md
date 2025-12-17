## Security / Sanitization Notes (Public Demo)

### What was removed/replaced
- **Hardcoded credentials**
  - Removed real/default passwords from firmware sources.
  - Replaced with explicit placeholders (e.g. `CHANGE_ME_*`).
- **OTA upload secrets and hostnames**
  - `platformio.ini` no longer contains OTA hostnames or upload auth flags.
- **Hardcoded WiFi SSIDs/passwords**
  - The demo AP SSID/password are placeholders and intentionally insecure (open AP by default).

### Where demo defaults live
All public-demo placeholders are centralized in:
- `firmware/src/config/demo_public_build.h`

This is the single place to review when preparing a public release.

### Guidance for real deployments (not included here)
If you ever evolve this into a private, production repo:
- Load secrets from secure provisioning (not source code).
- Use unique, per-device credentials.
- Add proper session management, rate limiting, CSRF protection, and secure storage.


