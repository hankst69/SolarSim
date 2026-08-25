# Map projections

Elevation data sets are published in projected national grids, so `geolib`
carries the projections needed to address their tiles. They live in the core of
the library rather than next to a single data set, because the same projection
serves several data sources.

## UTM projection

`UtmProjection` implements the transverse Mercator projection used by UTM on the
WGS84/ETRS89 ellipsoid. The zone enters only through its central meridian
(`6 * zone - 183`), so one implementation serves all 60 zones;
`zoneForLongitude()` picks the right one. `forward()` maps latitude/longitude to
easting/northing, `inverse()` maps back, with a round trip accurate to roughly
1e-9 degrees.

Many national elevation models are published in UTM: zone 32N (EPSG:25832) for
Germany, Austria and Denmark, zone 31N for France, zones 32-35 for the Nordics
and zones 10-19 for the USA. `Utm32Projection` is a static convenience wrapper
for zone 32N.

```cpp
double easting = 0.0, northing = 0.0;
Utm32Projection::forward(48.1372, 11.5756, easting, northing);   // Munich

UtmProjection zone31(31);
zone31.forward(48.8566, 2.3522, easting, northing);              // Paris
```

## British National Grid projection

Not every national elevation model uses UTM: the British data products are
published on the British National Grid (OSGB36, EPSG:27700), which uses the Airy
1830 ellipsoid instead of WGS84. `BritishNationalGridProjection` therefore
combines the transverse Mercator projection of the grid with the Helmert datum
shift WGS84 -> OSGB36, so its interface takes and returns WGS84 coordinates just
like `UtmProjection`. The Helmert parameters are the standard OS values, giving
an accuracy of a few metres, well below the 1 m raster spacing of the LIDAR
data. In addition it converts between coordinates and the two letter codes of
the 100 km squares (`squareFor()` / `squareOrigin()`), which the tile names of
the British data sets are built from.

```cpp
double easting = 0.0, northing = 0.0;
BritishNationalGridProjection::forward(51.5074, -0.1278, easting, northing);  // London
const std::string square = BritishNationalGridProjection::squareFor(easting, northing); // "TQ"
```

## Related

- [HeightDataSources.md](HeightDataSources.md) - the tiles addressed through
  these projections.
