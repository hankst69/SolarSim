# SunPosition — coordinate systems and calculation

`geo::SunPosition` implements the NOAA solar position algorithm (accuracy of
about one arc minute). The calculation silently walks through **four** different
reference frames before it arrives at the polar coordinates
(azimuth / elevation) that are relative to the `GroundPlane` of the given
`GeoLocation`.

This document describes that chain as implemented in
`geolib/src/SunPosition.cpp`.

## 1. Time: no frame yet, just an epoch

`DateTimeUtc::julianCentury()` converts UTC to `t`, the number of Julian
centuries since **J2000.0** (2000-01-01 12:00 TT). Every polynomial below is a
series expansion in `t`.

Caveat: UTC is fed in directly where the theory expects TT (? UTC + 69 s
today). That is an error of about 0.0008° in the sun's longitude — well below
the algorithm's own accuracy, so it is ignored.

## 2. Heliocentric to geocentric ecliptic

```
geomMeanLong, geomMeanAnom, eccent, sunEqOfCentre, sunTrueLong, sunAppLong
```

These describe the **earth's Keplerian orbit around the sun**, so the underlying
model here is sun-centred. The classical trick is applied immediately: the
earth's heliocentric longitude + 180° is the sun's geocentric longitude, and
that is exactly what `geomMeanLong` (280.46646 + ...) already encodes.

The working frame is therefore the **geocentric ecliptic frame**:

- origin: earth centre
- fundamental plane: the ecliptic (the earth's orbital plane)
- `sunTrueLong` is the ecliptic longitude of the sun, measured from the vernal
  equinox

`sunEqOfCentre` is the equation of centre, the correction from the mean
(circular) anomaly to the true anomaly of the elliptical orbit. `sunAppLong`
then subtracts aberration (-0.00569°, light travel time) and nutation in
longitude (the `omega` term), which gives the *apparent* longitude. The ecliptic
latitude of the sun is taken as zero.

## 3. Geocentric ecliptic to geocentric equatorial

```
meanObliq, obliqCorr  ->  declination = asin(sin(obliq) * sin(appLong))
```

A rotation by the obliquity about the vernal equinox axis tilts the ecliptic
onto the celestial equator. The origin stays the **earth centre**, the
fundamental plane becomes the equator. The result is the sun's **declination**.

The right ascension is not computed explicitly — the hour angle is obtained via
solar time instead, which is the cheaper NOAA route.

## 4. Hour angle: the earth's rotation, referred to the local meridian

```
equationOfTime  -> difference between true solar time and mean solar time
trueSolarTime   =  UTC minutes + equationOfTime + 4 * longitude
hourAngle       =  trueSolarTime / 4 - 180
```

The term `4 * longitude` (four minutes per degree) rotates from the Greenwich
meridian to the standpoint's meridian, so the frame is now **earth centred but
observer oriented**: the origin is still the earth centre, but the reference
direction is the local meridian. The hour angle is zero at true local solar
noon, negative in the morning and positive in the afternoon.

Only the **longitude** of the `GeoLocation` enters here.

## 5. Geocentric equatorial to topocentric horizontal (the ground plane)

```
cos(zenith) = sin(lat) * sin(decl) + cos(lat) * cos(decl) * cos(hourAngle)
elevation   = 90 - zenith
azimuth     from the same spherical triangle, flipped when hourAngle > 0
```

This is the spherical law of cosines applied to the astronomical triangle
(pole - zenith - sun). It rotates by the **latitude** so that the local zenith
becomes the pole of the frame. The result is the **horizontal (topocentric)
frame**:

- fundamental plane: exactly the `GroundPlane` of the location
- azimuth measured clockwise from north, elevation above that plane

This is the polar coordinate pair that `direction()` turns into an ENU unit
vector and that `projectOnDome()` feeds into `HorizonDome::pointOnDome`.

## The origin question, precisely

The **direction angles are computed geocentrically** and are then simply
declared to be topocentric. Strictly the origin should be shifted from the earth
centre to the standpoint (parallax). For the sun, whose distance is about 23500
earth radii, the horizontal parallax is at most **8.8 arc seconds** — again
below the algorithm's accuracy, so the shift is deliberately omitted.

That is why `GeoLocation::altitude()` and the `EarthModel` play no role in
`SunPosition` at all; only latitude and longitude do.

The only place where the observer's position genuinely becomes the origin is
**downstream**, in `GroundPlane` and `HorizonDome`, which are true east/north/up
frames centred on the standpoint. And `refractedElevation()` is the one step
that is physically local: it bends the ray in the observer's atmosphere.

## Summary

| Step | Origin | Fundamental plane | Output |
| --- | --- | --- | --- |
| Orbit series | sun (recast to earth) | ecliptic | longitude, mean anomaly, eccentricity |
| Apparent longitude | earth centre | ecliptic | apparent longitude |
| Obliquity rotation | earth centre | equator | declination |
| Equation of time + longitude | earth centre | local meridian | hour angle |
| Astronomical triangle | earth centre (approx. standpoint) | ground plane | azimuth, elevation |
