#include "geolib/GeoLocation.h"
#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/data_sources/BavariaDgm1HeightDataSource.h"
#include "geolib/data_sources/BavariaDgm1TileDownloader.h"
#include "geolib/data_sources/WorldCopernicusDem30HeightDataSource.h"
#include "geolib/data_sources/WorldCopernicusDem30TileDownloader.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace {

constexpr const char* kCopernicusUserName = "<your-copernicus-account-user-name>";
constexpr const char* kCopernicusPassword = "<your-copernicus-account-password>";

struct BasicAuthCredentials {
    std::string userName;
    std::string password;

    bool configured() const
    {
        return !userName.empty() && !password.empty() && userName.front() != '<' &&
               password.front() != '<';
    }
};

std::wstring widenAscii(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

std::string base64Encode(const std::string& value)
{
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((value.size() + 2) / 3) * 4);

    unsigned int bits = 0;
    int bitCount = 0;
    for (unsigned char ch : value) {
        bits = (bits << 8) | ch;
        bitCount += 8;
        while (bitCount >= 6) {
            bitCount -= 6;
            encoded.push_back(kAlphabet[(bits >> bitCount) & 0x3F]);
        }
    }

    if (bitCount > 0) {
        encoded.push_back(kAlphabet[(bits << (6 - bitCount)) & 0x3F]);
    }
    while ((encoded.size() % 4) != 0) {
        encoded.push_back('=');
    }
    return encoded;
}

class WinHttpHandle {
public:
    explicit WinHttpHandle(HINTERNET handle = nullptr) : m_handle(handle) {}

    ~WinHttpHandle()
    {
        if (m_handle != nullptr) {
            WinHttpCloseHandle(m_handle);
        }
    }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    WinHttpHandle(WinHttpHandle&& other) noexcept : m_handle(other.m_handle)
    {
        other.m_handle = nullptr;
    }

    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept
    {
        if (this != &other) {
            if (m_handle != nullptr) {
                WinHttpCloseHandle(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HINTERNET get() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr; }

private:
    HINTERNET m_handle;
};

bool downloadFileWinHttp(const std::string& url, const std::string& targetPath,
                         const std::string& authorizationHeader, std::string& error)
{
    const std::wstring wideUrl = widenAscii(url);

    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    wchar_t extraInfo[512] = {};
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = hostName;
    components.dwHostNameLength = static_cast<DWORD>(std::size(hostName));
    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(urlPath));
    components.lpszExtraInfo = extraInfo;
    components.dwExtraInfoLength = static_cast<DWORD>(std::size(extraInfo));

    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
        error = "WinHttpCrackUrl failed for " + url + " (error " +
                std::to_string(GetLastError()) + ")";
        return false;
    }

    WinHttpHandle session(WinHttpOpen(L"GeoSolar HeightDataSourcesSample/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS,
                                      0));
    if (!session) {
        error = "WinHttpOpen failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    WinHttpHandle connection(
        WinHttpConnect(session.get(), host.c_str(), components.nPort, 0));
    if (!connection) {
        error = "WinHttpConnect failed for URL " + url +
                " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    std::wstring objectName(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) {
        objectName.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", objectName.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) {
        error = "WinHttpOpenRequest failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    if (!authorizationHeader.empty()) {
        const std::wstring header = widenAscii("Authorization: " + authorizationHeader);
        if (!WinHttpAddRequestHeaders(request.get(), header.c_str(),
                                      static_cast<DWORD>(header.size()),
                                      WINHTTP_ADDREQ_FLAG_ADD |
                                          WINHTTP_ADDREQ_FLAG_REPLACE)) {
            error = "WinHttpAddRequestHeaders failed (error " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }
    }

    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = "WinHttpSendRequest failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        error = "WinHttpReceiveResponse failed (error " + std::to_string(GetLastError()) +
                ")";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request.get(),
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        error = "WinHttpQueryHeaders failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    if (statusCode < 200 || statusCode >= 300) {
        error = "HTTP GET failed for " + url + " with status " + std::to_string(statusCode);
        return false;
    }

    const std::filesystem::path outputPath(targetPath);
    if (!outputPath.parent_path().empty()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream output(targetPath, std::ios::binary);
    if (!output) {
        error = "Cannot open output file: " + targetPath;
        return false;
    }

    for (;;) {
        DWORD bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &bytesAvailable)) {
            error = "WinHttpQueryDataAvailable failed (error " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }
        if (bytesAvailable == 0) {
            break;
        }

        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), bytesAvailable, &bytesRead)) {
            error = "WinHttpReadData failed (error " + std::to_string(GetLastError()) + ")";
            return false;
        }
        output.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
        if (!output) {
            error = "Writing download target failed: " + targetPath;
            return false;
        }
    }

    return true;
}

//https://download1.bayernwolke.de/a/dgm/dgm1/dgm1_32_636_5529_1_by.xyz
//https://download1.bayernwolke.de/a/dgm/dgm1xyz/636_5529.zip

bool fetchUrl(const std::string& url, const std::string& targetPath,
              const BasicAuthCredentials& credentials)
{
    std::string authorizationHeader;
    if (credentials.configured()) {
        authorizationHeader = "Basic " + base64Encode(credentials.userName + ':' + credentials.password);
    }

    std::string error;
    const bool ok = downloadFileWinHttp(url, targetPath, authorizationHeader, error);
    if (!ok) {
        std::cerr << "Download failed: " << error << '\n';
    }

    return ok;
}

void printRegisteredSources(const geo::HeightDataSourceRegistry& registry)
{
    std::cout << "Registered height data sources:\n";
    for (const auto& source : registry.sources()) {
        std::cout << "  - " << source->name() << " (resolution " << source->resolutionM()
                  << " m)\n";
    }
}

void sampleLocation(const std::string& label, const geo::GeoLocation& location,
                    const geo::HeightDataSourceRegistry& registry)
{
    std::cout << '\n'
              << label << " at " << std::fixed << std::setprecision(5) << location.latitude()
              << ", " << location.longitude() << '\n';

    const auto candidates = registry.sourcesFor(location.latitude(), location.longitude());
    if (candidates.empty()) {
        std::cout << "  No source covers this location.\n";
        return;
    }

    for (const auto& source : candidates) {
        double heightM = 0.0;
        std::cout << "  Trying " << source->name() << "... ";
        if (source->sampleHeight(location, heightM)) {
            std::cout << "height = " << std::setprecision(2) << heightM << " m\n";
            return;
        }
        std::cout << "no tile cached/downloaded for that point\n";
    }

    std::cout << "  No registered source returned a height.\n";
}

} // namespace

int main()
{
    const std::filesystem::path cacheRoot = std::filesystem::current_path() / "height_data_cache";

    geo::BavariaDgm1TileDownloader::Config bavariaConfig;
    bavariaConfig.cacheDirectory = (cacheRoot / "bavaria_dgm1").string();
    bavariaConfig.baseUrl = "https://download1.bayernwolke.de/a/dgm/dgm1xyz";
    bavariaConfig.fileExtension = ".zip";
    bavariaConfig.allowDownload = true;

    geo::BavariaDgm1TileDownloader bavariaDownloader(
        bavariaConfig,
        [](const std::string& url, const std::string& targetPath) {
            return fetchUrl(url, targetPath, {});
        });

    const BasicAuthCredentials copernicusCredentials{kCopernicusUserName, kCopernicusPassword};

    geo::WorldCopernicusDem30TileDownloader::Config worldConfig;
    worldConfig.cacheDirectory = (cacheRoot / "world_copernicus_dem30").string();
    worldConfig.baseUrl = "https://copernicus-dem-30m.s3.amazonaws.com";
    worldConfig.fileExtension = ".hgt";
    worldConfig.allowDownload = true;

    geo::WorldCopernicusDem30TileDownloader worldDownloader(
        worldConfig,
        [copernicusCredentials](const std::string& url, const std::string& targetPath) {
            return fetchUrl(url, targetPath, copernicusCredentials);
        });

    const auto bavariaSource =
        std::make_shared<geo::BavariaDgm1HeightDataSource>(bavariaDownloader.tileLoader());
    const auto worldSource = std::make_shared<geo::WorldCopernicusDem30HeightDataSource>(
        worldDownloader.tileLoader());

    geo::HeightDataSourceRegistry& registry = geo::HeightDataSourceRegistry::instance();
    registry.clear();
    registry.addSource(std::make_shared<geo::FlatHeightDataSource>());
    registry.addSource(worldSource);
    registry.addSource(bavariaSource);

    std::cout << "HeightDataSources sample\n";
    std::cout << "  Bavaria DGM1 cache: " << bavariaConfig.cacheDirectory << '\n';
    std::cout << "  World Copernicus DEM30 cache: " << worldConfig.cacheDirectory << '\n';
    if (!copernicusCredentials.configured()) {
        std::cout << "  Copernicus credentials are placeholders. Replace them before running against"
                     " an authenticated endpoint.\n";
    }

    printRegisteredSources(registry);
    sampleLocation("Bamberg, Germany (should prefer Bavaria DGM1)",
                   geo::GeoLocation(49.89873, 10.90067), registry);
    sampleLocation("Paris, France (should fall back to Copernicus DEM30)",
                   geo::GeoLocation(48.85837, 2.29448), registry);

    return 0;
}
