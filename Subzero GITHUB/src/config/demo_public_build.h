/**
 * Demo / public showcase build defaults
 * ------------------------------------
 * This repository is intentionally sanitized to be SAFE for public GitHub.
 *
 * - No real credentials, SSIDs, passwords, tokens, or IPs should exist in the repo.
 * - Anything below is a placeholder and MUST be changed for real deployments.
 * - This project is NOT intended to be a secure, production-ready firmware release.
 */
#pragma once

// Device network identity (avoid leaking real hostnames used on private networks).
#define DEMO_MDNS_HOSTNAME "subzero-demo"

// Demo AP configuration (used when switching to AP mode for setup/recovery).
// Keep password empty to make it obvious this is NOT production-ready security.
#define DEMO_AP_SSID "Subzero-Demo-AP"
#define DEMO_AP_PASSWORD ""

// Web UI / Serial auth placeholders.
// These are intentionally non-secret placeholders. Do NOT ship real credentials in code.
#define DEMO_DEFAULT_ADMIN_PASSWORD "CHANGE_ME_ADMIN_PASSWORD"
#define DEMO_DEFAULT_OTA_PASSWORD "CHANGE_ME_OTA_PASSWORD"

// Optional: hard-disable OTA in public builds to reduce the "deployable product" feel.
// Set to 0 if you want to enable OTA locally for experimentation.
#ifndef DEMO_DISABLE_OTA
#define DEMO_DISABLE_OTA 1
#endif


