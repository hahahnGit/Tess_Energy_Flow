# Energy flow

An animated energy flow tile for Tessera screens, in the style of Home Assistant's
[power-flow-card-plus](https://github.com/flixlix/power-flow-card-plus): circles with symbols for solar (PV), grid, home,
battery, wallbox and a second storage (Speicher2), joined by lines. Small dots run along every line that carries power, from
the source to the consumer; the more power, the faster they run. A line without flow stays grey and still.

```
   PV ------- Wallbox
    |
 Grid --- Home --- Battery
                     |
                  Speicher2
```

## Set up

1. Add the plugin to your screens in Tessera (or copy the folder to `/config/tessera-plugins/energy_flow/` and press "Build again").
2. Pick your sensors. All power sensors must report **W**, the battery charge level %. Only solar, grid and battery are needed
   for a useful picture; leave the others on a sensor that does not exist and the circle shows `--`.

   | Input | Meaning |
   |---|---|
   | Solar power sensor | total PV power, shown below the sun |
   | Inverter input sensor | shown above the sun symbol |
   | Grid power sensor | grid power |
   | Battery power sensor | charge/discharge power |
   | Battery state of charge sensor | charge level in % |
   | Home consumption sensor | used when "Calculate home from the balance" is off |
   | Wallbox power sensor | wallbox |
   | Speicher2 power sensor | a second storage or load, bottom right |

3. In Layout, place the tile "Energy flow" from the Plugins group. 3x2 cells or more looks best; smaller tiles show fewer rows
   per circle, a 1x1 tile shows four lines of text.

## Options of the tile

| Option | What it does |
|---|---|
| Power for the fastest dots | from this power on the dots run at full speed |
| Grid sensor: positive = feed-in | on if your grid sensor is positive while you feed in (like `invert_state` in power-flow-card-plus) |
| Battery sensor: positive = charging | on if your battery sensor is positive while charging |
| Speicher2: positive = discharging | off: a positive value means Speicher2 takes power from the house |
| Label: PV, wallbox, grid, battery, Speicher2 | own name for each circle (empty = standard name) |
| Calculate home from the balance | home = solar + grid draw + battery discharge - Speicher2 (wallbox included) |
| Dots per line | 1 to 6 |
| Animation steps per second | 1 to 4 |

The battery label can hold the placeholder `{soc}`, for example `Battery {soc}` shows `Battery 8 %` and follows the charge level.
In that case the top row of the battery circle stays empty.

## How it moves

The screen cannot run Lovelace cards. The plugin moves the dots itself, at most four times a second, so the animation is a few
frames per second, not a smooth card animation. Fewer dots and fewer steps per second are easier on the screen.

## Troubleshooting

Every 10 seconds the raw values of all sensors go to the screen log (tag `energy_flow`). Use it to find a swapped sensor or a
wrong sign.

## Privacy

Nothing leaves your home: the screen reads the chosen sensors from Home Assistant, the plugin uses no network.

## License

MIT
