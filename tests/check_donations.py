"""Static invariants, not a replacement for Qt6/OBS Windows integration tests."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
files={n:(p/n).read_text('utf-8') for n in ['src/donations.cpp','src/donations.hpp','src/dock.cpp','src/settings.cpp','CMakeLists.txt']}
checks={
 'DonationAlerts socket': ('src/donations.cpp','centrifugo.donationalerts.com/connection/websocket'),
 'official DA OAuth': ('src/donations.cpp','/api/v1/user/oauth'),
 'DA subscription': ('src/donations.cpp','/api/v1/centrifuge/subscribe'),
 'DP legacy guarded': ('src/donations.cpp','s.legacyApiOptIn'),
 'DP 30-second poll': ('src/donations.cpp','dpTimer_.start(30000)'),
 'dedupe by event key': ('src/donations.cpp','ledger_.contains(key)'),
 'atomic journal': ('src/donations.cpp','QSaveFile'),
 'claim before delivery': ('src/donations.cpp','ledger_.claim(key,d,level,tg,dc)'),
 'inflight before POST': ('src/donations.cpp','ledger_.setStatus(key,name,"inflight")'),
 'independent destinations': ('src/donations.cpp','s.donations.toDiscord&&s.discordEnabled'),
 'only live': ('src/donations.cpp','cfg.onlyLive && !live_'),
 'templates': ('src/dock.cpp','Шаблоны трёх уровней'),
 'currency tiers': ('src/dock.cpp','Пороговые суммы'),
 'donations tab': ('src/dock.cpp','Page::Donations'),
 'secret DPAPI': ('src/settings.cpp','protect(current.donations.daToken)'),
 'websocket Qt': ('CMakeLists.txt','Qt6::WebSockets')
}
for name,(file,token) in checks.items():
 assert token in files[file],name
print('Donation structural checks passed:',len(checks))
