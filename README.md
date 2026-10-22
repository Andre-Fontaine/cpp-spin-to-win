# Spin To Win

A simple reflex game built with Unreal Engine 5.8 and C++.

A clock-style hand sweeps around a dial. Press the button while the hand is inside the green zone and it reverses direction, speeds up, and a new, slightly smaller zone appears. Miss the zone, or press too early, and the run is over. How long can you last?

## Controls

| Input | Action |
|---|---|
| Space / Left Click / Gamepad A | Start, hit the zone, restart after game over |

## Status

Early prototype.

- [x] Core loop: spinning hand, target zone, increasing speed, game over
- [ ] Saved local leaderboard
- [ ] Extra buttons / prompts that unlock as you progress
- [ ] Online leaderboard

## About this repo

This repository contains the **C++ source and project config only**. The `Content/` folder (levels, Blueprints, art) is intentionally not tracked, so the project can't be opened and run directly from a fresh clone. The game's UI is drawn entirely in code, so the code is meant to be read and learned from.

## Code overview

| File | Purpose |
|---|---|
| `SpinGameMode` | Game rules: hand movement, scoring, speed ramp, zone placement, game states |
| `SpinPlayerController` | Reads input and forwards it to the game mode |
| `SpinHUD` | Draws the dial, hand, target zone and text |

Difficulty values (start speed, max speed, zone size, and so on) are `UPROPERTY` fields on `SpinGameMode`.

## Building

Requires Unreal Engine 5.8 and a C++ toolchain (Visual Studio with the "Game development with C++" workload on Windows).

1. Create or open a C++ project named `SpinToWin` in UE 5.8.
2. Copy `Source/` and `Config/` from this repo into it.
3. Right-click the `.uproject` and choose **Generate Visual Studio project files**, then build.
