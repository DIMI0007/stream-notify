# Stream Notify

**Stream and donation announcements from OBS to Telegram and Discord.** The plugin runs on your computer without a project-operated server. [Русский](../README.md).

| Waiting for a stream | During a broadcast |
| --- | --- |
| ![Stream Notify home dock](screenshots/home.png) | ![Outputs, bitrate, and graphs](screenshots/monitoring.png) |

*Version 1.0.13 UI previews were made in an isolated test environment. Shown accounts and outputs are sample data.*
[Narrow 275 px dock](screenshots/home-narrow.png).

## Features

- Send an automatic announcement after the YouTube or Twitch API confirms a live broadcast. A second-platform announcement and manual fallback links are configurable. [Verification details](AUTO_ANNOUNCEMENTS_RU.md).
- Set recipients, text, and announcement delay in the OBS dock. See the latest delivery result at a glance.
- Receive DonationAlerts and DonatePay donation notifications with sender, amount, message, and configurable thresholds. [Donation setup](../README_DONATIONS_RU.md).
- Monitor active outputs: **LIVE**, current bitrate, a graph of the last 30 minutes, and transferred data.
- Stream Stats can show Notify's measured CPU, tracked data and session traffic. Settings and donation delivery history stay local; Windows protects stored secrets.

## How the plugins work together

Stream Notify works on its own when you sign in here and configure recipients. If Stream Chat is installed, Notify may use its protected sign-ins only to check live status. Stream Stats can read Notify's local counters; it does not send announcements.

## Install

1. Close OBS and [download the latest successful build](https://github.com/DIMI0007/stream-notify/actions/workflows/build-test-windows.yml).
2. Copy the entire `stream-notify` folder into `C:\ProgramData\obs-studio\plugins\`. Replace the whole folder when updating.
3. Start OBS, open the **Stream Notify** dock, and configure recipients. Test buttons verify sending without a live stream.

Target: **OBS 33, Windows x64**. `BUILD_INFO.json` in the archive identifies the version and source revision. The build is currently unsigned.

Automatic links and duplicate protection still need verification during a real multistream.

[Donation guide](../README_DONATIONS_RU.md) · [GPL v2 license](../LICENSE) · [Privacy](../PRIVACY.md)

## Support the project

If these plugins help your streams, please consider a donation. Your support helps improve the plugins, fix issues and fund future development.

[Support via DonationAlerts](https://www.donationalerts.com/r/otmopozok) · [Support via DonatePay](https://donatepay.ru/don/OTMOPOZOK)

Developed by [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
