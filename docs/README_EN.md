# Stream Notify

**Stream and donation announcements from OBS to Telegram and Discord.** The plugin runs on your computer without a project-operated server. [Русский](../README.md).

| Waiting for a stream | During a broadcast |
| --- | --- |
| ![Stream Notify home dock](screenshots/home.png) | ![Outputs, bitrate, and graphs](screenshots/monitoring.png) |

*Version 1.0.10 UI previews were made in an isolated test environment. Shown accounts and outputs are sample data.*
[Narrow 275 px dock](screenshots/home-narrow.png).

## Features

- Send an automatic announcement after the YouTube or Twitch API confirms a live broadcast. A second-platform announcement and manual fallback links are configurable. [Verification details](AUTO_ANNOUNCEMENTS_RU.md).
- Set recipients, text, and announcement delay in the OBS dock. See the latest delivery result at a glance.
- Receive DonationAlerts and DonatePay donation notifications with sender, amount, message, and configurable thresholds. [Donation setup](../README_DONATIONS_RU.md).
- Monitor each output: **LIVE**, current bitrate, a graph of the last 30 minutes, and transferred data. Finished output totals stay visible until that output starts again.
- View tracked plugin usage and session traffic. Settings and donation delivery history stay local; Windows protects stored secrets.

## Verified status

- An automatic announcement during a real stream reached both Telegram and Discord.
- DonationAlerts browser sign-in and delivery of a replayed earlier donation to both destinations were confirmed. A new payment after connection still needs testing.
- DonatePay accepted the account API key and connected. Delivery from a new real donation is not yet confirmed; this integration is experimental.
- Version 1.0.11 is built and UI-tested in CI. Automatic links and deduplication still need validation during a real multistream.

## Install

1. Close OBS and [download the latest successful build](https://github.com/DIMI0007/stream-notify/actions/workflows/build-test-windows.yml).
2. Copy the entire `stream-notify` folder into `C:\ProgramData\obs-studio\plugins\`. Replace the whole folder when updating.
3. Start OBS, open the **Stream Notify** dock, and configure recipients. Test buttons verify sending without a live stream.

Target: **OBS 33, Windows x64**. `BUILD_INFO.json` in the archive identifies the version and source revision. The build is currently unsigned.

[Donation guide](../README_DONATIONS_RU.md) · [GPL v2 license](../LICENSE) · [Privacy](../PRIVACY.md)

## Support development

[DonationAlerts](https://www.donationalerts.com/r/otmopozok) · [DonatePay](https://donatepay.ru/don/OTMOPOZOK)

Developed by [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
