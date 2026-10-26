# Stream Notify

**OBS stream and donation announcements for Telegram and Discord.** It runs locally without a project-operated server. [Русский](../README.md).

| Home | During a broadcast |
| --- | --- |
| ![Stream Notify home](screenshots/home.png) | ![Stream Notify LIVE status and bitrate graphs](screenshots/monitoring.png) |

*Version 1.0.5 screenshots come from an isolated UI test host. Connections and streams shown are demonstration data.*

## Verified status

- Automatic announcements reached both Telegram and Discord during a real broadcast.
- DonationAlerts browser sign-in was confirmed. Replaying an earlier donation delivered notifications to both destinations; a new payment after connection has not yet been checked.
- DonatePay accepted the account API key and connected. Delivery from a new real donation has not yet been checked.
- Version 1.0.5 built and passed automated UI checks. LIVE status and bitrate graphs still need a real OBS broadcast check.

## Features

- One announcement per stream session, including multiple OBS/Aitum outputs.
- Separate Telegram and Discord destinations, manual messages, and delivery tests.
- DonationAlerts and DonatePay notifications with configurable thresholds and text.
- A card per output with LIVE status, bitrate, a **30-minute** graph and transferred GB. Stopped-output totals remain visible until the next start.
- Local settings with Windows-protected secrets.

## Install

1. Close OBS.
2. [Download the artifact from the latest successful build](https://github.com/DIMI0007/stream-notify/actions/workflows/build-test-windows.yml).
3. Copy the entire `stream-notify` directory to `C:\ProgramData\obs-studio\plugins\`. Replace the complete directory when updating.
4. Start OBS, open **Stream Notify**, configure destinations and use the test buttons.

Test target: **OBS 33.0.0 on Windows x64**. The archive is currently unsigned. `BUILD_INFO.json` contains the version and source revision.

[GPL v2 license](../LICENSE) · [Privacy](../PRIVACY.md) · [Donation guide in Russian](../README_DONATIONS_RU.md)

Developed by [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
