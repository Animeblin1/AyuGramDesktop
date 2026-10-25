# Correctness audit and fixes

## Scope

First-pass audit of the first-party project: build/CI, settings and local persistence, filter import and regex execution, plugin metadata preview, account/window lifetimes, history painting, input/calculator integration, live activity, scrolling and profile gradients. Existing crypto/update/media/wallet subsystems were mapped and selected entry points inspected, not exhaustively verified. Vendored dependencies were not audited. This is not a claim that the whole Telegram codebase is bug-free.

## Confirmed defects fixed

- Saved Messages crash: online-badge painting dereferenced nullable `HistoryItem::displayFrom()` after correctly handling hidden forward authors. Guard the sender; preserve painter state.
- Mini online badge: ignored the toggle and restored guessed text colors rather than original painter state. Respect the setting and use save/restore.
- Calculator absent from ordinary chats: only ComposeControls had the hook; classic HistoryWidget did not. Share setup between both actual input paths; parent the result panel outside the clipped compose wrapper, track geometry/visibility/settings, remove diagnostic toasts.
- Calculator: incorrect precedence for `-2^2`, unbounded recursive expression nesting, quadratic/manual text elision and over-wide panel. Correct precedence, cap nesting, use font elision and parent width.
- Live activity: process-global PeerId cache leaked state between accounts; stale reads emitted reentrant events; cancellation and replay suppression absent; expiry updates unreliable. Scope state to session lifetime, expire via independent timer, make getter side-effect-free, scope cancellation to chat/topic, ignore replay events.
- Live activity UI: animation used wrong baseline, could continue after switching to a group/topic, and restarted its timer on every update. Fix baseline/gating/timer lifecycle; restore ordinary profile status timeout.
- Wheel scrolling: every input reset frame time; elastic scroll interpreted successful scrolling as a stall. Preserve animation frame time and correct clamping/stall conditions.
- Selection scrolling: negative deltas became positive boosted targets; shared dragging timer restarted on every mouse movement. Preserve direction and do not restart an active timer.
- Profile gradient: body tint survived the topbar-only change; global peer-keyed scroll events crossed windows/accounts; clipping broke fading band; avatar cache used a lossy key and cached transient letter fallbacks. Delete body tint/global bus, subscribe to the actual profile container, restore painter clipping, use real userpic keys and invalidate on photo changes.
- Settings persistence: truncating the live JSON file could destroy all saved settings on failed writes. Serialize first, use QSaveFile, check complete write and commit.
- Plugin metadata preview: asynchronous sticker lookup/download callbacks could use a destroyed window controller. Guard callbacks and bind the subscription to the controller lifetime.
- Filter import: background peer resolution retained raw session pointers after account deletion. Resolve weak session on main before starting each request and stop the worker when it is gone.
- Regex filters: pathological imported/local patterns could monopolize UI matching. Set ICU time/stack limits, check matcher errors and do not apply inverse matching after failure.
- Windows build-report script: Unicode multiplication sign crashed cp1251 stdout. Use an ASCII separator.

## Checks

Executed successfully before CI:

- Python unittest discovery in Telegram/build: 35 tests passed (including new settings-save/regex source contracts).
- `pwsh -NoProfile -File Telegram/build/tests/check_avatar_paint.ps1`.
- `pwsh -NoProfile -File Telegram/build/tests/check_activity_calculator.ps1`.
- `node Telegram/build/tests/check_scroll_gradient.cjs`.
- `node Telegram/build/tests/check_lifetime_selection.cjs`.
- Root and lib_ui `git diff --check`.

The new source-contract checks and scrolling model do not prove Qt runtime behavior. `Telegram/build/tests/inline_calculator_test.cpp` directly calls the production parser but has not been compiled/linked/executed here. No local C++/Qt build environment is configured. Windows release compilation is checked separately in GitHub Actions; GUI reproduction and pixel/interaction checks still require the resulting client.

## Remaining limitations / follow-up

- No crash dump was supplied: the hidden-author null dereference is confirmed in source and fits Saved Messages, but an actual client reproduction is still required to rule out additional crashes.
- Ayu's retained edited/deleted-message database remains ordinary plaintext SQLite. Redesigning at-rest protection requires a migration/key-management plan; no destructive encryption migration was attempted.
- One freshest received action per user is shown; cancelling it does not restore an older concurrent action. Telegram only exposes activity updates it actually delivers to this account; no polling of arbitrary chats or activity inference was added.
- Activity context wording inherited Russian strings; complete localization and action-specific animation coverage remain follow-up work.
- Photo-gradient cache remains unbounded. Cache policy is not a confirmed immediate crash and was not redesigned.
- Regex caps are per matcher, not a total per-frame budget for arbitrarily many configured filters.
- Wallet/MTProto cryptography, all platform installers, every third-party dependency and every UI screen were not exhaustively tested.
