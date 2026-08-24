#include "geolib/data_sources/WorldCopernicusDem30TileReader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace geo {
namespace {

constexpr double kNoData = -32768.0;
/// Length of one degree of latitude in metres, used to express the raster
/// spacing of a geographic grid as a ground sample distance.
constexpr double kMetresPerDegree = 111320.0;

void setError(std::string* error, const std::string& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

} // namespace

std::shared_ptr<GridHeightDataSource> WorldCopernicusDem30TileReader::readHgt(std::istream& stream,
                                                                        const GeoBounds& bounds,
                                                                        std::string* error)
{
    std::vector<char> bytes((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
    if (bytes.size() < 4 || (bytes.size() % 2) != 0) {
        setError(error, "HGT tile is empty or has an odd byte count");
        return nullptr;
    }

    const std::size_t samples = bytes.size() / 2;
    const auto side = static_cast<std::size_t>(std::llround(std::sqrt(static_cast<double>(samples))));
    if (side < 2 || side * side != samples) {
        setError(error, "HGT tile is not square");
        return nullptr;
    }

    // Big endian signed 16 bit samples, northernmost row first - which is
    // exactly the row order expected by GridHeightDataSource.
    std::vector<double> grid(samples, kNoData);
    for (std::size_t i = 0; i < samples; ++i) {
        const auto high = static_cast<std::int16_t>(static_cast<unsigned char>(bytes[2 * i]));
        const auto low = static_cast<std::int16_t>(static_cast<unsigned char>(bytes[2 * i + 1]));
        const auto value = static_cast<std::int16_t>((high << 8) | low);
        grid[i] = static_cast<double>(value);
    }

    const int side32 = static_cast<int>(side);
    const double latSpan = bounds.maxLatitudeDeg - bounds.minLatitudeDeg;
    const double resolutionM = latSpan / (side32 - 1) * kMetresPerDegree;
    return std::make_shared<GridHeightDataSource>("Copernicus DEM GLO-30 tile", bounds, side32,
                                                 side32, std::move(grid), resolutionM, kNoData);
}

std::shared_ptr<GridHeightDataSource> WorldCopernicusDem30TileReader::readAsciiGrid(std::istream& stream,
                                                                              std::string* error)
{
    int columns = 0;
    int rows = 0;
    double xll = 0.0;
    double yll = 0.0;
    double cellSize = 0.0;
    double noData = kNoData;
    bool centerReference = false;

    // Header: key/value pairs in any order, terminated by the first token that
    // is not a known header key (i.e. the first data value).
    while (true) {
        const std::streampos mark = stream.tellg();
        std::string key;
        if (!(stream >> key)) {
            break;
        }
        const std::string lowered = toLower(key);
        if (lowered != "ncols" && lowered != "nrows" && lowered != "xllcorner" &&
            lowered != "yllcorner" && lowered != "xllcenter" && lowered != "yllcenter" &&
            lowered != "cellsize" && lowered != "nodata_value") {
            stream.clear();
            stream.seekg(mark);
            break;
        }

        double value = 0.0;
        if (!(stream >> value)) {
            setError(error, "malformed ASCII grid header at key '" + key + "'");
            return nullptr;
        }
        if (lowered == "ncols") {
            columns = static_cast<int>(value);
        } else if (lowered == "nrows") {
            rows = static_cast<int>(value);
        } else if (lowered == "xllcorner") {
            xll = value;
        } else if (lowered == "yllcorner") {
            yll = value;
        } else if (lowered == "xllcenter") {
            xll = value;
            centerReference = true;
        } else if (lowered == "yllcenter") {
            yll = value;
            centerReference = true;
        } else if (lowered == "cellsize") {
            cellSize = value;
        } else {
            noData = value;
        }
    }

    if (columns < 2 || rows < 2 || cellSize <= 0.0) {
        setError(error, "incomplete ASCII grid header");
        return nullptr;
    }

    // ASCII grids store the northernmost row first, matching the row order of
    // GridHeightDataSource, so the cells can be read straight into the raster.
    std::vector<double> grid(static_cast<std::size_t>(columns) * rows, noData);
    for (std::size_t i = 0; i < grid.size(); ++i) {
        double value = 0.0;
        if (!(stream >> value)) {
            setError(error, "ASCII grid ended before all cells were read");
            return nullptr;
        }
        grid[i] = value;
    }

    // Bounds refer to the outermost sample centres.
    const double minLon = centerReference ? xll : xll + 0.5 * cellSize;
    const double minLat = centerReference ? yll : yll + 0.5 * cellSize;
    const GeoBounds bounds{minLat, minLat + (rows - 1) * cellSize, minLon,
                           minLon + (columns - 1) * cellSize};
    return std::make_shared<GridHeightDataSource>("Copernicus DEM GLO-30 tile", bounds, columns,
                                                 rows, std::move(grid),
                                                 cellSize * kMetresPerDegree, noData);
}

std::shared_ptr<GridHeightDataSource> WorldCopernicusDem30TileReader::readFile(const std::string& path,
                                                                         const GeoBounds& bounds,
                                                                         std::string* error)
{
    const std::size_t dot = path.find_last_of('.');
    const std::string extension =
        (dot == std::string::npos) ? std::string() : toLower(path.substr(dot));

    if (extension == ".asc") {
        std::ifstream stream(path);
        if (!stream) {
            setError(error, "cannot open file: " + path);
            return nullptr;
        }
        return readAsciiGrid(stream, error);
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        setError(error, "cannot open file: " + path);
        return nullptr;
    }
    return readHgt(stream, bounds, error);
}

} // namespace geo
