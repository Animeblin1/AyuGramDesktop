# Changelog

## 7.2.9-3 - 2026-10-05

- Strip local symbols from the macOS executable. The release step removed only
  debugging entries and left about two million local symbols, so the
  executable weighed 1.75 GB; it is now about 0.6 GB, and the disk image
  shrinks with it.

## 7.2.9-2 - 2026-10-05

- Based the fork on AyuGram 7.2.9 instead of its own merge of Telegram
  Desktop 7.2.9. This brings AyuGram's rework of view-once media, its crash
  fixes and stricter link handling, and window screenshot protection disabled
  on macOS and Windows. `lib_ui`, `lib_tl` and `codegen` come from AyuGram's
  repositories again.
- Kept the language synchronization and macOS icon fixes, which AyuGram does
  not have yet, and two parts of Telegram Desktop 7.2.9 that AyuGram's merge
  dropped: the guard against rebuilding a chat badge on selection, and the
  reply handling of a send that ghost mode schedules.
- Publish a new build of an already released version under a revision tag,
  such as this `v7.2.9-2`, while the application keeps the Telegram version.

## 7.2.9 - 2026-09-17

- Updated the application base to Telegram Desktop 7.2.9.
- Build and publish a release from a single workflow, dispatched on the default
  branch so it writes the caches every release branch can read. A version is
  now compiled once instead of once to warm the caches and again to publish.
- Key the Windows dependency caches by the build root. `prepare.py` embeds it
  in every per-stage key, so a cache keyed without it hit while every stage
  inside it missed, rebuilding Qt from source on each release.
- Prepare Qt Release-only on both platforms; the application is built
  Release-only and nothing consumed the Debug half.
- Cache the Windows compiler for the first time, refresh every cache twice a
  week against the seven-day eviction, and drop each superseded entry after a
  successful save.
- Report per-target build times, compiler cache statistics and the processor
  count in the build log as well as the run summary, and attribute Windows
  objects to their real targets instead of to the drive letter.

## 7.2.8 - 2026-09-15

- Updated the application base to Telegram Desktop 7.2.8, which fixes crashes
  on invalid Lottie files.
- Picked up the 7.2.6 feature release: image editor text tool, folder and file
  set sending, GIF editing before send, and call rating in the call panel.
- Replaced the rlottie animation library with tlottie, following upstream;
  preparation now builds a Rust static library under ThirdParty.
- Built the two macOS architectures in separate release jobs, because one cold
  universal build exceeds the six-hour job limit.

## 7.2.5 - 2026-09-06

- Updated the application base to Telegram Desktop 7.2.5.
- Preserved AyuGram features and upstream attribution without fork-specific branding.
- Fixed language synchronization, macOS bundle identity and application icons.
- Added reproducible technical packages for universal macOS and Windows x64.
- Disabled automatic updates until a signed update channel is available.
