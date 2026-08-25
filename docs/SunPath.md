# SunPath

`SunPath` samples a whole UTC day (by default every 10 minutes, i.e. 144
intervals) and stores the time, the `SunPosition` and the dome point of every
sample.

## What it offers

- `samples()` - all samples of the day, ordered by time.
- `visibleSamples()` / `arcPoints()` - the part of the path above the horizon,
  the latter as a plain point list in the local ENU frame, ready for rendering
  the sun arc on the `HorizonDome`.
- `at(hour, minute, second)` - the `SunPosition` at an arbitrary time of that
  day, computed directly instead of interpolated.
- `sunrise()` / `sunset()` - the horizon crossings, refined by bisection between
  the two bracketing samples. Both return `false` for polar day and polar night,
  which `hasSunrise()` reports separately.
- `highestSample()` - the sample with the largest elevation, i.e. solar noon.
- `relativeDailyEnergy()` - the sum of `SunPosition::relativeIrradiance()` over
  all samples times the sample interval in hours. A relative measure that is
  useful to compare days and locations; for a value in W/m^2 use
  [SunEnergy.md](SunEnergy.md).

```cpp
HorizonDome dome(home);
SunPath path(dome, 2026, 8, 23);

std::vector<Vector3> arc = path.arcPoints();

DateTimeUtc rise;
if (path.sunrise(rise)) {
    // rise holds the UTC sunrise time
}

SunPosition noon = path.highestSample().position;
```

## Related

- [SunPosition.md](SunPosition.md) - the underlying position calculation.
- [SunEnergy.md](SunEnergy.md) - absolute irradiance in W/m^2.
- [HorizonDome.md](HorizonDome.md) - the dome the arc is projected onto.
