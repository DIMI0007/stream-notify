"""Zero-dependency structural checks; NOT a substitute for building/running in OBS."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
text = {name: (root/name).read_text(encoding='utf-8') for name in [
    'src/plugin.cpp', 'src/settings.cpp', 'src/net.cpp', 'src/dock.cpp',
    'src/dock.hpp', 'CMakeLists.txt', 'installer/StreamNotify.nsi']}
checks = {
    'native OBS dock': ('src/plugin.cpp', 'obs_frontend_add_dock_by_id'),
    'confirmed OBS stream start': ('src/plugin.cpp', 'OBS_FRONTEND_EVENT_STREAMING_STARTED'),
    'stream-stop cancellation': ('src/dock.cpp', 'cancelCurrent("Stream stopped")'),
    'mandatory 60-second delay': ('src/settings.cpp', 'seconds == 60 || seconds == 90 || seconds == 120'),
    'elapsed-based countdown': ('src/dock.cpp', 'elapsed_.elapsed()'),
    'cancel button': ('src/dock.cpp', 'cancelCurrent("User cancelled")'),
    'separate Telegram enable': ('src/dock.cpp', 'store_.current.telegramEnabled=val'),
    'separate Discord enable': ('src/dock.cpp', 'store_.current.discordEnabled=val'),
    'independent test sender': ('src/dock.cpp', 'testSender_.send'),
    'per-channel confirmation': ('src/dock.cpp', 'recordOutcome(c,d)'),
    'Telegram confirmation ID': ('src/net.cpp', '"message_id"'),
    'Discord wait confirmation': ('src/net.cpp', '"wait", "true"'),
    'no mass mentions': ('src/net.cpp', '"allowed_mentions"'),
    'bounded rate retries': ('src/net.cpp', 'task->attempts < 3'),
    'request timeout': ('src/net.cpp', 'setTransferTimeout(10000)'),
    'no secret error strings': ('src/net.cpp', 'Never surface raw Qt errorString'),
    'DPAPI encryption': ('src/settings.cpp', 'CryptProtectData'),
    'atomic settings': ('src/settings.cpp', 'QSaveFile'),
    'Russian and English UI': ('src/dock.cpp', 'store_.current.locale == "ru"'),
    'official OBS locale auto': ('src/dock.cpp', 'obs_get_locale()'),
    'Windows installer': ('installer/StreamNotify.nsi', 'stream-notify.dll'),
}
for title, (name, needle) in checks.items():
    assert needle in text[name], f'MISSING: {title}'
print(f'OK: {len(checks)} structural checks passed.')
print('Integration compilation/runtime tests: NOT RUN (Windows Qt/OBS SDK unavailable).')
