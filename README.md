# Stream Notify

**Анонсы трансляций и донатов в Telegram и Discord прямо из OBS.** Панель показывает состояние подключений, отправки сообщений и потоков.

| Ожидание эфира | Во время трансляции |
| --- | --- |
| ![Ожидание](docs/screenshots/home.png) | ![Трансляция](docs/screenshots/monitoring.png) |

[Узкая панель](docs/screenshots/home-narrow.png) · [English](docs/README_EN.md)

## Что удобно

- Плагин проверяет эфир YouTube и Twitch и добавляет ссылки только на подтверждённые трансляции.
- Если вторая платформа запустилась позже, можно отправить отдельное короткое сообщение.
- Текст анонсов, задержку и получателей можно менять в самой панели OBS.
- Уведомления о донатах DonationAlerts и DonatePay настраиваются отдельно.
- Stream Stats может показать трафик Notify за время работы OBS.

## Как установить

1. Полностью закройте OBS. Скачайте **StreamNotify-Test-DLL-OBS33-Windows-x64** на странице [последней успешной сборки](https://github.com/DIMI0007/stream-notify/actions/workflows/build-test-windows.yml) и распакуйте ZIP.
2. Откройте `C:\ProgramData\obs-studio\plugins\`. Удалите старую папку `stream-notify`, если она есть. Скопируйте сюда папку `stream-notify` из ZIP целиком.
3. Запустите OBS. Откройте **Док-панели → Stream Notify**, подключите Telegram или Discord и нажмите **«Отправить тест»**. Для тестового сообщения эфир не нужен.

Файл `stream-notify.dll` должен лежать прямо в папке `stream-notify`, рядом с `Qt6WebSockets.dll` и папкой `data`. Настройки хранятся отдельно. [Подробная инструкция](docs/TEST_WINDOWS_RU.md).

Для **OBS Studio 33 на Windows x64**. Сборка пока не подписана. Автоматические ссылки и защита от повторных анонсов требуют проверки во время реального мультистрима.

## Ещё

[Как работают анонсы](docs/AUTO_ANNOUNCEMENTS_RU.md) · [Донаты](README_DONATIONS_RU.md) · [Общий стиль](docs/VISUAL_STYLE_RU.md) · [Конфиденциальность](PRIVACY.md) · [Лицензия](LICENSE)

Разработчик: [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
## Поддержать проект

Если плагины полезны вам, поддержите проект донатом. Ваша помощь поддерживает развитие плагинов, исправления и будущую разработку.

[Поддержать через DonationAlerts](https://www.donationalerts.com/r/otmopozok) · [Поддержать через DonatePay](https://donatepay.ru/don/OTMOPOZOK)
