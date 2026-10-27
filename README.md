# Spin To Win

A simple reflex game built with Unreal Engine 5.8 and C++.

A clock-style hand sweeps around a dial. Press the right button while the hand is inside the target zone and it reverses direction, speeds up, and a new, slightly smaller zone appears. Every mistake - missing a zone, pressing the wrong button, running into an obstacle, or failing to fix a jam in time - costs a life and pauses the run so you can confirm and get a few seconds of invincibility to settle back in. Run out of lives and it's back to the title screen. How far can you get?

## Controls

| Input | Action |
|---|---|
| W / Gamepad A (Xbox) or X (PlayStation) | Hit the first zone, confirm menus/prompts |
| D / Gamepad B (Xbox) or O (PlayStation) | Hit the second zone (unlocks later), confirm menus/prompts |
| Hold Left Click / Right Trigger | Raise the hand over an obstacle |
| Mouse | Click buttons on the main menu, mode select, settings, and pause screens |

## Modes

- **Tutorial** - 7 short, unfailable levels that introduce the zone, the second button, obstacles/the lift, and jams one at a time.
- **Endless** - the real run. Every 10 hits is a level; speed ramps up, the zone shrinks, a second button unlocks, obstacles start appearing, and eventually the hand can jam and needs mashing to fix. Mistakes cost a life and pause the run rather than ending it outright - the run only ends when you're out of lives.

## Status

Early prototype.

- [x] Core loop: spinning hand, target zone, increasing speed, game over
- [x] Extra buttons / prompts that unlock as you progress
- [x] Obstacles, the lift, and a regenerating life pool
- [x] Jams that need mashing to fix before the hand breaks
- [x] Non-regenerating "miss" lives for missed/mistimed zones, separate from obstacle lives
- [x] Mistake recovery: pause, confirm, then a few seconds of invincibility instead of an instant game over
- [x] Tutorial mode
- [x] Main menu (Play / Settings), mode select, and a settings screen (quality, resolution, volume)
- [ ] Saved local leaderboard
- [ ] Online leaderboard

## About this repo

This repository contains the **C++ source and project config only**. The `Content/` folder (levels, Blueprints, art) is intentionally not tracked, so the project can't be opened and run directly from a fresh clone. The game's UI is drawn entirely in code, so the code is meant to be read and learned from.

## Code overview

| File | Purpose |
|---|---|
| `SpinGameMode` | Game rules: hand movement, scoring, speed ramp, zone placement, lives/jams, game states, menus and settings |
| `SpinPlayerController` | Reads input (keyboard/gamepad/mouse) and forwards it to the game mode |
| `SpinHUD` | Draws the dial overlay, menus, pause screens and text |
| `SpinClock` | The in-world dial actor: hand, zone, and obstacle meshes |
| `SpinCityFX` | Purely cosmetic ambient effects (floor grid, circuit traces, flickering nodes) driven by level-placed dressing |

Difficulty values (start speed, max speed, zone size, lives, jam timing, and so on) are `UPROPERTY` fields on `SpinGameMode`.

## Building

Requires Unreal Engine 5.8 and a C++ toolchain (Visual Studio with the "Game development with C++" workload on Windows).

1. Create or open a C++ project named `SpinToWin` in UE 5.8.
2. Copy `Source/` and `Config/` from this repo into it.
3. Right-click the `.uproject` and choose **Generate Visual Studio project files**, then build.
