#include "geolib/data_sources/BavariaDgm1TileReader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace geo {
namespace {

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

std::string escapeForSingleQuotedShell(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char c : value) {
        if (c == '\'') {
            escaped += "'\"'\"'";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

std::string escapeForPowerShellSingleQuotedString(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char c : value) {
        if (c == '\'') {
            escaped += "''";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

bool ensureExtractedTxtFile(const std::string& zipPath, std::string& txtPath, std::string* error)
{
    namespace fs = std::filesystem;

    fs::path archivePath(zipPath);
    fs::path extractedPath = archivePath;
    extractedPath.replace_extension(".txt");
    txtPath = extractedPath.string();

    if (fs::exists(extractedPath)) {
        return true;
    }

    std::error_code ec;
    if (!extractedPath.parent_path().empty()) {
        fs::create_directories(extractedPath.parent_path(), ec);
        if (ec) {
            setError(error, "cannot create cache directory for extracted tile: " +
                                extractedPath.parent_path().string());
            return false;
        }
    }

    const std::string archive = escapeForPowerShellSingleQuotedString(archivePath.string());
    const std::string output = escapeForPowerShellSingleQuotedString(extractedPath.string());
    const std::string entryName = escapeForPowerShellSingleQuotedString(extractedPath.filename().string());

#if defined(_WIN32)
    const std::string command =
        "powershell -NoProfile -NonInteractive -Command \"Add-Type -AssemblyName "
        "System.IO.Compression.FileSystem; $archive='" +
        archive + "'; $output='" + output + "'; $entryName='" + entryName +
        "'; $zip=[System.IO.Compression.ZipFile]::OpenRead($archive); try { $entry=$zip.Entries | "
        "Where-Object { $_.Name -ieq $entryName } | Select-Object -First 1; if ($null -eq $entry) "
        "{ exit 2 }; [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $output, $false) } "
        "finally { $zip.Dispose() }\"";
#else
    const std::string command = "unzip -p '" + escapeForSingleQuotedShell(archivePath.string()) +
                                "' '" + escapeForSingleQuotedShell(extractedPath.filename().string()) +
                                "' > '" + escapeForSingleQuotedShell(extractedPath.string()) + "'";
#endif

    if (std::system(command.c_str()) != 0 || !fs::exists(extractedPath)) {
        setError(error, "cannot extract TXT file from ZIP archive: " + zipPath);
        return false;
    }

    return true;
}

/// Smallest positive difference between consecutive sorted unique coordinates.
double deriveSpacing(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    double spacing = std::numeric_limits<double>::max();
    for (std::size_t i = 1; i < values.size(); ++i) {
        const double d = values[i] - values[i - 1];
        if (d > 1e-6 && d < spacing) {
            spacing = d;
        }
    }
    return (spacing == std::numeric_limits<double>::max()) ? 0.0 : spacing;
}

} // namespace

bool BavariaDgm1TileReader::readXyz(std::istream& stream, Utm32GridTile& tile, std::string* error)
{
    std::vector<double> eastings;
    std::vector<double> northings;
    std::vector<double> heights;

    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        // Tolerate comma or semicolon separated variants.
        std::replace(line.begin(), line.end(), ',', ' ');
        std::replace(line.begin(), line.end(), ';', ' ');

        std::istringstream ls(line);
        double east = 0.0;
        double north = 0.0;
        double height = 0.0;
        if (!(ls >> east >> north >> height)) {
            continue;
        }
        eastings.push_back(east);
        northings.push_back(north);
        heights.push_back(height);
    }

    if (heights.empty()) {
        setError(error, "no XYZ samples found");
        return false;
    }

    const double minEast = *std::min_element(eastings.begin(), eastings.end());
    const double maxEast = *std::max_element(eastings.begin(), eastings.end());
    const double minNorth = *std::min_element(northings.begin(), northings.end());
    const double maxNorth = *std::max_element(northings.begin(), northings.end());

    double spacing = deriveSpacing(eastings);
    if (spacing <= 0.0) {
        spacing = deriveSpacing(northings);
    }
    if (spacing <= 0.0) {
        setError(error, "cannot derive grid spacing from XYZ samples");
        return false;
    }

    const int columns = static_cast<int>(std::llround((maxEast - minEast) / spacing)) + 1;
    const int rows = static_cast<int>(std::llround((maxNorth - minNorth) / spacing)) + 1;
    if (columns < 2 || rows < 2) {
        setError(error, "XYZ tile is too small");
        return false;
    }

    constexpr double kNoData = -9999.0;
    std::vector<double> grid(static_cast<std::size_t>(columns) * rows, kNoData);
    for (std::size_t i = 0; i < heights.size(); ++i) {
        const long long column = std::llround((eastings[i] - minEast) / spacing);
        const long long row = std::llround((northings[i] - minNorth) / spacing);
        if (column < 0 || row < 0 || column >= columns || row >= rows) {
            continue;
        }
        grid[static_cast<std::size_t>(row) * columns + static_cast<std::size_t>(column)] =
            heights[i];
    }

    tile = Utm32GridTile(minEast, minNorth, spacing, columns, rows, std::move(grid), kNoData);
    return true;
}

bool BavariaDgm1TileReader::readAsciiGrid(std::istream& stream, Utm32GridTile& tile,
                                          std::string* error)
{
    int columns = 0;
    int rows = 0;
    double xll = 0.0;
    double yll = 0.0;
    double cellSize = 0.0;
    double noData = -9999.0;
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
            return false;
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
        return false;
    }

    // ASCII grids store the northernmost row first; flip to south-up ordering.
    std::vector<double> grid(static_cast<std::size_t>(columns) * rows, noData);
    for (int row = 0; row < rows; ++row) {
        const std::size_t target = static_cast<std::size_t>(rows - 1 - row) * columns;
        for (int column = 0; column < columns; ++column) {
            double value = 0.0;
            if (!(stream >> value)) {
                setError(error, "ASCII grid ended before all cells were read");
                return false;
            }
            grid[target + static_cast<std::size_t>(column)] = value;
        }
    }

    const double originEast = centerReference ? xll : xll + 0.5 * cellSize;
    const double originNorth = centerReference ? yll : yll + 0.5 * cellSize;
    tile = Utm32GridTile(originEast, originNorth, cellSize, columns, rows, std::move(grid), noData);
    return true;
}

bool BavariaDgm1TileReader::readFile(const std::string& path, Utm32GridTile& tile,
                                     std::string* error)
{
    const std::size_t dot = path.find_last_of('.');
    const std::string extension =
        (dot == std::string::npos) ? std::string() : toLower(path.substr(dot));

    std::string resolvedPath = path;
    if (extension == ".zip") {
        if (!ensureExtractedTxtFile(path, resolvedPath, error)) {
            return false;
        }
    }

    std::ifstream stream(resolvedPath);
    if (!stream) {
        setError(error, "cannot open file: " + resolvedPath);
        return false;
    }

    if (extension == ".asc" || extension == ".txt.asc") {
        return readAsciiGrid(stream, tile, error);
    }
    return readXyz(stream, tile, error);
}

} // namespace geo
