# Static regression check: hidden forwarded authors may have no PeerData.
# Run: pwsh -NoProfile -File Telegram/build/tests/check_avatar_paint.ps1
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '../../..')
$history = Get-Content -Raw (Join-Path $root 'Telegram/SourceFiles/history/history_inner_widget.cpp')
$dialogs = Get-Content -Raw (Join-Path $root 'Telegram/SourceFiles/dialogs/ui/dialogs_message_view.cpp')
if ($history -match 'displayFrom\(\)->') {
    throw 'Nullable hidden-forward sender is dereferenced without a guard.'
}
if ($history -notmatch 'from\s*\?\s*from->asUser\(\)\s*:\s*nullptr') {
    throw 'History online badge must handle a missing sender.'
}
$mini = [regex]::Match($dialogs, '(?s)if \(const auto user = _miniSender->asUser\(\)\).*?rect\.setLeft').Value
if ($mini -notmatch 'onlineIndicator\(\)' -or $mini -notmatch 'p\.save\(\)' -or $mini -notmatch 'p\.restore\(\)') {
    throw 'Mini badge must obey the toggle and preserve painter state.'
}
Write-Output 'PASS: nullable sender and online-badge painter source invariants (not a GUI runtime test).'
