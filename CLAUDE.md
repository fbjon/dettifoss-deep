# Dettifoss Deep

A songwriting firmware for the M-VAVE FM-1 (GPL-3.0-only), forked from Niagara Drift
(github.com/evanyerburgh/niagara-drift) at `887c3d7`. The fork is permanent: its own UI and save format on Niagara
Drift's engine and platform. Until the new UI lands the README is still Niagara Drift's manual; BUILDING.md says how to
build. This file is for anyone working on the code with Claude. Personal notes go in `CLAUDE.local.md` (ignored).

## Commits and pull requests

The repository is public and should read on its own.

- **Messages:** a short subject in plain English (about 70 characters, no `feat:` prefixes); the body says why, and
  what was measured. A message doesn't restate the code, as a comment doesn't.
- **Before pushing:** prune and shorten the messages, and squash commits that belong together. Each commit builds
  and passes the tests.
- **Readable without the conversation.** A commit message or PR description makes sense to someone who has only
  the repository: no links to conversations or sessions, and no names for plans that exist only in a chat ("the
  new save format"). Work that is still to come is a GitHub issue, referenced by number; if it isn't worth an
  issue, leave the sentence out.
- **Trailers:** `Co-Authored-By: Claude <noreply@anthropic.com>` on commits Claude wrote; nothing else.
- **Branches:** readable names (`live-page`), one topic each, merged with a merge commit and never force-pushed
  once shared. Larger work is a stack of PRs that merge together.
- **Upstream:** a fix taken from Niagara Drift is cherry-picked with `-x`, so its origin stays in the message.

## Words

Used the same way in the code, the screen and the docs.

| Word | Meaning |
| --- | --- |
| Drone | The whole sustained sound. The engine's two voice groups ("drone I", "drone II" in older code) are layers |
| Sound | Every timbre setting as it is now |
| Preset | A factory sound to start from |
| Scene | A stored sound, recalled with a glide |
| Macro | A live knob that moves several settings |
| Chord | A bass note and up to three tones over it; the bass is the root unless set apart |
| Progression | A section's chords in order, each a step |
| Section | A progression, with or without a scene |
| Song | The order of sections, with repeats |
| Motion | Recorded knob movement |
| Take | One recording pass |
| Project | Everything saved together |

"Sequence" is kept for a future timed mode: don't use it for a progression.

## Commands

```
sh tests/run_tests.sh        # every host test, the scenarios, and the emulator if emcc is there
sh host/build_host.sh        # the host simulator: build/host/omni_host SCRIPT.omni OUTDIR (WAVs, PNGs)
./build.sh                   # the firmware (Docker, the JieLi toolchain and SDK): build/omni.fwsc
sh web/emu/build.sh          # the browser emulator into build/emu
tools/budget.py              # after ./build.sh: the PR's budget line against main
tools/same_samples.py        # this tree's sounds and screens against main's: the same, or listen
```

On macOS, export `SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"` first. On Linux the tests need
`CC="cc -D_DEFAULT_SOURCE -Wl,--no-as-needed -lm"`, and drone-cost can't count instructions there (CI
counts them with valgrind: `tools/ci_cost.py`).

## The FM-1's budget

The chip is small, and the sound comes first. A change can spend these only when it's worth hearing:

**The same sound for less** (Evan, 2026-10-28): when code can give the same sound and behaviour with less RAM, less CPU
or less flash, take that version, proven by `tools/same_samples.py`.

- **RAM:** .data + .bss under 96 KB, the pool with at least 8 KB spare; the build fails past either.
  The pool is close (11 KB spare in October 2026); the main RAM is 82 KB of 96 (November 2026, 0.3): a new buffer or table
  in RAM still needs a reason.
- **CPU:** the engine's worst case (`tests/host/drone_cost.c`) under its limit; on the device the
  longest audio block (`tools/fm1_cpu.py`) about 69 % in the worst realistic case at 0.2: keep the
  headroom. A block past 100 % is late, and a late block is a click.
- **Flash:** the image fits its slot; tables that aren't read every sample stay `const` in flash.

Every firmware PR body carries `tools/budget.py`'s line, so a change's cost is seen beside what it does.

## What the build enforces (don't work around it)

- No double: the FPU is single precision, and a soft-double routine in the image fails the build.
- `.ram_text` (code that runs with the flash off) makes no calls.
- Registers are touched only in `firmware/hal/`.
- `-ffp-contract=off` everywhere, so the host, the emulator and the device give the same samples.

## Conventions

- New files start with `/* SPDX-License-Identifier: GPL-3.0-only */`; keep the headers of reused files;
  anything borrowed gets a row in LICENSING.md, its licence checked first. No JieLi SDK files in git.
- New parameters go at the end of the enums and bump `PROJ_FORMAT`; renumber the `.omni` scenarios when
  parameters or pages move (the top of `tests/scenarios/drone.omni` lists them).
- `drone.c` is one compilation unit (it includes `drone_fx.c`): declare statics before first use.
- A sound change comes with a host render (WAV) and numbers measured on it; a screen change with a
  screenshot. Measure on and off as two renders of the same passage (the host is deterministic).
- **A change that shouldn't change the sound or the screen passes `tools/same_samples.py`** (every fixed render
  in `tests/golden`, every scenario, the engine's scenes, sample by sample; CI runs it on every PR while the repository
  variable `SAME_SAMPLES` is `on`, off during the rework, so run it by hand). One that means to
  carries the PR label `changes-sound`, with its WAVs; one that changes only the screen `changes-screen` (the sound still
  checked). A new feature adds its own render to `tests/golden`; renumbered
  parameters or pages renumber `tests/golden` too (a script that differs from main's isn't compared, only listed).
- Audio code: `.claude/rules/dsp.md`.

## Gotchas

- `tools/make_screens.py` (after `sh host/build_host.sh`) remakes the README's pictures from
  `tests/scenarios/pages.omni`, which starts with `notips` so no one-time tip shows; afterwards only the screens a
  change touches should differ (`git status docs/img`).
- A host build next to stale copies of `drone.h` picks those up: build from the tree.
- `.omni` files are the host simulator's scenarios (the name is FoMni's: "OMNI"); `omni_host`, `build/omni.fwsc` too.
