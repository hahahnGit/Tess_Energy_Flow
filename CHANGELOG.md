# Changelog

## 0.8.1 - 2026-11-09
- Ready for publishing: neutral example sensors as defaults, new README, maintainer set.

## 0.8.0 - 2026-11-09
- The second storage is called "Speicher2" everywhere (no product name). Its input is now ef_storage2 and its option invert_storage2: pick the sensor again after the update.

## 0.7.1 - 2026-11-09
- Fix: Tessera allows at most 12 options per tile. The toggle "Animate the dots" is removed (dots always run; 1 dot and 1 step per second is the gentlest setting).

## 0.7.0 - 2026-11-09
- New: the battery label can hold {soc}, e.g. "Batterie {soc}". It shows the charge level and changes with it. The top row of the circle stays empty then.

## 0.6.1 - 2026-11-09
- Fix: the battery charge level was missing on tiles too small for all rows. It is kept now; the power rows go first.

## 0.6.0 - 2026-11-09
- New text options: a label for the PV, wallbox, grid, battery and Speicher2 circle (empty = standard name).

## 0.5.0 - 2026-11-09
- New picture like power-flow-card-plus: circles with symbols for PV, grid, home, battery, wallbox and Speicher2, rectangular lines for every flow (sun to home, battery, grid; grid to home, battery; battery to home; home to wallbox, Speicher2).
- New input: inverter input sensor (shown in the PV circle).
- Small tiles show fewer rows per circle (symbol and one value, then the value alone).

## 0.4.0 - 2026-11-09
- New option **Calculate home from the balance**: home = solar + grid draw + battery discharge - Speicher2.

## 0.3.2 - 2026-11-09
- The raw values of all sensors go to the screen log every 10 seconds (tag energy_flow), to find a wrong or swapped sensor.

## 0.3.1 - 2026-11-09
- Fix: "Home" showed the grid value. The inputs have unique names now (ef_home, ef_grid, ...), so no placeholder can collide with another.

## 0.3.0 - 2026-11-08
- New in the inspector: **Dots per line** (1 to 6). Animation steps per second (1 to 4) stays.

## 0.2.0 - 2026-11-08
- The screen is spared: two dots per line instead of three, a dot moves only when its place changed, and the dots step at most twice a second by default.
- New in the inspector: **Animation steps per second** (1 to 4).
- A tile that is too small for the picture (1x1) now shows four lines of text: solar, home, grid and battery.
- The tile can be placed from 1x1 up to 4x3 cells.

## 0.1.0 - 2026-11-08
- First version: an energy flow tile with solar, grid, battery, home, wallbox and a second storage, and dots that run along the lines.
