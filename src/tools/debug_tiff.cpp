#include "geolib/data_sources/GeoTiffReader.h"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: debug_tiff <path>\n"; return 1; }
    std::ifstream stream(argv[1], std::ios::binary);
    if (!stream) { std::cerr << "cannot open\n"; return 1; }
    geo::GeoTiffReader reader;
    geo::GeoTiffReader::Raster raster;
    std::string error;
    geo::GeoBounds fallback{0,1,0,1};
    bool ok = reader.read(stream, fallback, raster, &error);
    std::cout << "ok=" << ok << " error=" << error << "\n";
    if (ok) {
        std::cout << "cols=" << raster.columns << " rows=" << raster.rows << "\n";
        std::cout << "bounds: " << raster.bounds.minLatitudeDeg << " " << raster.bounds.maxLatitudeDeg
                   << " " << raster.bounds.minLongitudeDeg << " " << raster.bounds.maxLongitudeDeg << "\n";
        // print a few values
        for (int i = 0; i < 5; ++i) {
            std::cout << "v[" << i << "]=" << raster.values[i] << "\n";
        }
        // sample middle
        std::size_t mid = (static_cast<std::size_t>(raster.rows)/2) * raster.columns + raster.columns/2;
        std::cout << "v[mid]=" << raster.values[mid] << "\n";
    }
    auto it324 = reader.tags().find(324);
    auto it325 = reader.tags().find(325);
    if (it324 != reader.tags().end()) {
        std::cout << "TileOffsets: ";
        for (auto v : it324->second.ints) std::cout << v << " ";
        std::cout << "\n";
    }
    if (it325 != reader.tags().end()) {
        std::cout << "TileByteCounts: ";
        for (auto v : it325->second.ints) std::cout << v << " ";
        std::cout << "\n";
    }
    for (const auto& tag : reader.tags()) {
        std::cout << "tag " << tag.first << " type=" << tag.second.type << " ints=" << tag.second.ints.size() << " dbls=" << tag.second.doubles.size();
        if (!tag.second.ints.empty()) std::cout << " first=" << tag.second.ints[0];
        std::cout << "\n";
    }
    return 0;
}
