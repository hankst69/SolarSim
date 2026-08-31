#pragma once

/// Registers the concrete height data sources (Bavaria DGM1, World Copernicus
/// DEM GLO-30) with geo::HeightDataSourceRegistry, using Qt Network for the
/// tile downloads. Safe to call more than once; sources are only added the
/// first time.
void registerHeightDataSources();
