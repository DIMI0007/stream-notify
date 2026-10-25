<div align="center">
<h1>Stream Notify</h1>
<p><strong>Start your stream. Let your viewers know.</strong></p>
<p><a href="../README.md">Русский</a> · <strong>English</strong></p>
<p><img alt="OBS 33.0.0" src="https://img.shields.io/badge/OBS-33.0.0-302E31"> <img alt="Windows x64" src="https://img.shields.io/badge/Windows-x64-0078D4"> <img alt="Native Qt" src="https://img.shields.io/badge/UI-native_Qt-00B88C"></p>
</div>

Stream Notify is a native OBS Studio dock for stream announcements and donation notifications sent directly to your own Telegram and Discord destinations. It tracks OBS and Aitum network outputs and runs locally without a project-operated relay server.

[Windows test builds](https://github.com/DIMI0007/stream-notify/actions/workflows/build-test-windows.yml) · [Russian installation guide](TEST_WINDOWS_RU.md) · [Report an issue](https://github.com/DIMI0007/stream-notify/issues)

## Current status

| Area | Status |
| --- | --- |
| Stream-start announcements | The user confirmed delivery to both Telegram and Discord during a real stream using the restored previous build. |
| Manual delivery tests | Confirmed by the user; they verify delivery, not donation ingestion. |
| New output-history fixes and version 1.0.1 | Version 1.0.1 passed CI; OBS runtime validation of the new behavior is pending. |
| DonationAlerts browser sign-in | Implemented using the documented Implicit Flow; real-account validation is pending. |
| DonatePay HTTP API | Current official method and response schema verified. The incorrect local key was replaced by the user. A new real donation has not yet been validated end to end. |
| DonatePay WebSocket | Account authentication and subscription verified in a separate read-only probe. Incoming real donation mapping and recovery remain unverified; the plugin keeps its existing HTTP adapter. |
| Code signing | SignPath application received; approval pending. Current CI artifacts are unsigned. |

Unsigned builds can be blocked by Windows Smart App Control. A successful build is not proof that Windows will trust the package or that a new feature has been validated against a real account.

## What it does

- Announces a stream once per session, even with concurrent Twitch and YouTube outputs.
- Supports a configurable countdown and cancellation before dispatch.
- Sends Telegram and Discord requests independently and reports each delivery result.
- Offers manual messages and optional Discord @everyone mentions.
- Displays per-output bitrate, transferred data and lightweight local resource counters.
- Routes eligible donations through the existing notification destinations and templates.
- Keeps credentials locally with Windows DPAPI protection.

## Interface

<p align="center"><img src="screenshots/home.png" width="320" alt="Stream Notify native OBS dock"></p>

The image shows the existing interface. New builds identify their version in the footer and About section. CI UI previews use an isolated test host and are not evidence of real service connectivity.

## Install or update

Supported test configuration: **Windows x64, OBS 33.0.0, Qt 6.11.1**.

1. Close OBS completely.
2. Download the artifact from a successful workflow run and extract it.
3. Copy the complete `stream-notify` directory to `C:\ProgramData\obs-studio\plugins\`.
4. Replace the old plugin directory completely. Keep the Qt WebSockets and TLS files in their packaged locations.
5. Start OBS and enable the Stream Notify dock.

Settings are stored separately under the user's OBS configuration. Do not send settings files or credentials to support. To uninstall, close OBS and remove only the installed `stream-notify` plugin directory; local settings remain until you choose to remove them.

## Telegram and Discord

Configure your Telegram bot token and destination locally. Give the bot permission to post to the channel. For Discord, create a webhook in the target channel and paste its URL locally. Use each channel's test button before testing automatic announcements.

Discord @everyone is optional and off by default. Announcements, donations and manual messages follow the same opt-in setting.

## Donation services

DonationAlerts uses its official OAuth API and Centrifugo WebSocket. The app requests `oauth-user-show` and `oauth-donation-subscribe`. The browser sign-in returns to a temporary loopback listener on `127.0.0.1:38561`; no Client Secret is embedded. A widget token is not an API OAuth access token.

DonatePay uses the API key from the official account API page. The current plugin polls transactions no faster than every 30 seconds and backs off on errors. It never assumes RUB when a transaction lacks a currency. An HTTP 200 response containing `status: error` is an API failure, not a successful connection.

The default RUB levels are 100 / 500 / 2000. User-defined thresholds remain in force. Explicit saved thresholds for another currency take priority; otherwise the existing RUB conversion applies.

Persistent donation IDs prevent replay. Indeterminate deliveries are not silently resent. Actual donation receipt, destination delivery, network recovery and restart behavior still require end-to-end account tests.

## Diagnostics and documentation

Record the version/build ID, the safe error text and whether the failure occurred during sign-in, subscription, receipt or delivery. Never post real tokens, webhook URLs or unredacted configuration files.

- [Donation integration details (Russian)](../README_DONATIONS_RU.md)
- [Monitoring and resource counters (Russian)](MONITORING_RU.md)
- [Currency rules (Russian)](CURRENCY_RU.md)
- [Validation record](../tests/VALIDATION.txt)
- [Privacy policy](../PRIVACY.md)
- [Code signing policy](CODE_SIGNING.md)

## Support development

Optional support: [DonationAlerts](https://www.donationalerts.com/r/otmopozok) · [DonatePay](https://donatepay.ru/don/OTMOPOZOK).

## License

The project's own code is licensed under [GNU GPL version 2](../LICENSE). Original template and dependency notices are retained.

Developed by [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).

