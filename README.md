# Spin To Win

A simple reflex game built with Unreal Engine 5.8 and C++.

A clock-style hand sweeps around a dial. Press the right button while the hand is inside the target zone and it reverses direction, speeds up, and a new, slightly smaller zone appears. Every mistake - missing a zone, pressing the wrong button, running into an obstacle, or failing to fix a jam in time - costs a life; most mistakes then give you a few seconds of invincibility to settle back in. Run out of lives and a game-over menu lets you retry, head back to the main menu, or quit. How far can you get?

## Controls

| Input | Action |
|---|---|
| W / Gamepad A (Xbox) or X (PlayStation) | Hit the first zone, confirm menus/prompts (Tutorial on mode select) |
| D / Gamepad B (Xbox) or O (PlayStation) | Hit the second zone (unlocks at level 4), confirm menus/prompts |
| A / Gamepad X (Xbox) or Square (PlayStation) | Hit the third zone (unlocks at level 6), Daily on mode select |
| Hold Left Click / Right Trigger | Raise the hand over an obstacle (costs points while up, drops after one full turn) |
| Space / Gamepad Y (Xbox) or Triangle (PlayStation) | Mash to repair a jammed hand (does nothing the rest of the time) |
| Mouse | Click buttons on the main menu, mode select, settings, and pause screens |

On-screen button prompts follow whichever device you used last (keyboard or gamepad).

## Modes

- **Tutorial** - 7 short, unfailable levels that introduce the zone, the second button, obstacles/the lift, and jams one at a time. Clearing the last one carries straight on into an Endless run at level 5 (after a quick screen explaining lives and misses), with jams already in play.
- **Endless** - the real run. Before it starts you can switch on optional modifiers (Fast Hand, Tiny Zones, Wild Spin, Early Jams, One Life); each makes the run harder and multiplies every point scored, and boosted runs are tagged on the leaderboard. Every 10 hits is a level; speed ramps up, the zone shrinks, a second button unlocks at level 4 and a third at level 6, obstacles start appearing, and eventually the hand can jam and needs mashing to fix. Mistakes cost a life rather than ending the run outright - it only ends when you're out of lives. Hits decide your level; your score comes from how close to the centre of each zone you hit (PERFECT / GOOD / OK), multiplied by a combo that builds with PERFECT streaks and resets on sloppy hits or mistakes.
- **Daily** - one run a day that is the same for everyone: a seed and a "twist" modifier picked from the date. Retry as often as you like; your best counts on a separate daily board that starts over each day.

## Status

Early prototype.

- [x] Core loop: spinning hand, target zone, increasing speed, game over
- [x] Extra buttons / prompts that unlock as you progress
- [x] Obstacles, the lift, and a regenerating life pool
- [x] Jams that need mashing to fix before the hand breaks
- [x] Separate "miss" lives for missed/mistimed zones, with one coming back every 5 levels
- [x] Mistake recovery: a few seconds of invincibility instead of an instant game over
- [x] Tutorial mode
- [x] Main menu (Play / Settings), mode select, and a settings screen (quality, resolution, volume, Xbox or PlayStation controller icons)
- [x] Precision scoring (PERFECT / GOOD / OK) with a combo multiplier for PERFECT streaks
- [x] Saved local leaderboard, ranked two ways: highest level and highest score
- [x] Player profiles: pick or add a player on the main menu; runs go on the shared leaderboard under their name, and personal bests are tracked per player
- [x] Fast start: once you have reached level 5, the opening levels of each run go by quicker
- [x] Run modifiers with score multipliers
- [x] Daily challenge with its own board
- [x] End-of-run breakdown (longest combo, points lost to lifting, obstacles hit)
- [ ] Online leaderboard

## About this repo

This repository contains the **C++ source and project config only**. The `Content/` folder (levels, Blueprints, art) is intentionally not tracked, so the project can't be opened and run directly from a fresh clone. The game's UI is drawn entirely in code, so the code is meant to be read and learned from.

## Code overview

| File | Purpose |
|---|---|
| `SpinGameMode` | Game rules: hand movement, scoring, speed ramp, zone placement, lives/jams, game states, menus and settings |
| `SpinPlayerController` | Reads input (keyboard/gamepad/mouse) and forwards it to the game mode |
| `SpinHUD` | The in-run HUD (score, level, health, controls, callouts) and every menu and pause screen, all drawn in code |
| `SpinClock` | The in-world dial actor: hand, zone, and obstacle meshes |
| `SpinCityFX` | Purely cosmetic ambient effects (floor grid, circuit traces, flickering nodes) driven by level-placed dressing |
| `SpinSaveGame` | Saved data: player profiles (with personal bests) and the shared leaderboard (top runs by level and by score) |

Difficulty values (start speed, max speed, zone size, lives, jam timing, and so on) are `UPROPERTY` fields on `SpinGameMode`.

## Building

Requires Unreal Engine 5.8 and a C++ toolchain (Visual Studio with the "Game development with C++" workload on Windows).

1. Create or open a C++ project named `SpinToWin` in UE 5.8.
2. Copy `Source/` and `Config/` from this repo into it.
3. Right-click the `.uproject` and choose **Generate Visual Studio project files**, then build.
