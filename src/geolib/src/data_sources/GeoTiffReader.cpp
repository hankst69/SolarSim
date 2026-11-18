#include "geolib/data_sources/GeoTiffReader.h"

#include <algorithm>
#include <cstring>
#include <iterator>

extern "C" {
#include "miniz.h"
}

namespace geo {
namespace {

constexpr double kMetresPerDegree = 111320.0;

void setError(std::string* error, const std::string& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

std::size_t tiffTypeSize(std::uint16_t type)
{
    switch (type) {
    case 1: // BYTE
    case 2: // ASCII
    case 6: // SBYTE
    case 7: // UNDEFINED
        return 1;
    case 3: // SHORT
    case 8: // SSHORT
        return 2;
    case 4: // LONG
    case 9: // SLONG
    case 11: // FLOAT
        return 4;
    case 5: // RATIONAL
    case 10: // SRATIONAL
    case 12: // DOUBLE
    case 16: // LONG8
    case 17: // SLONG8
        return 8;
    default:
        return 0;
    }
}

bool inflateDeflate(const std::vector<std::uint8_t>& compressed, std::vector<std::uint8_t>& out)
{
    size_t outLen = 0;
    void* decompressed = tinfl_decompress_mem_to_heap(compressed.data(), compressed.size(), &outLen,
                                                      TINFL_FLAG_PARSE_ZLIB_HEADER);
    if (decompressed == nullptr) {
        return false;
    }
    out.assign(static_cast<std::uint8_t*>(decompressed),
              static_cast<std::uint8_t*>(decompressed) + outLen);
    mz_free(decompressed);
    return true;
}

bool inflateLzw(const std::vector<std::uint8_t>& compressed, std::size_t expectedSize,
                std::vector<std::uint8_t>& out)
{
    out.clear();
    out.reserve(expectedSize);

    constexpr int kClearCode = 256;
    constexpr int kEoiCode = 257;
    constexpr int kFirstCode = 258;

    std::vector<std::vector<std::uint8_t>> table;
    auto resetTable = [&]() {
        table.clear();
        table.resize(4096);
        for (int i = 0; i < 256; ++i) {
            table[static_cast<std::size_t>(i)] = {static_cast<std::uint8_t>(i)};
        }
    };
    resetTable();

    int codeSize = 9;
    int nextCode = kFirstCode;
    std::size_t bitPos = 0;
    const std::size_t totalBits = compressed.size() * 8;
    std::vector<std::uint8_t> previous;

    auto readCode = [&]() -> int {
        if (bitPos + static_cast<std::size_t>(codeSize) > totalBits) {
            return -1;
        }
        int code = 0;
        for (int i = 0; i < codeSize; ++i) {
            const std::size_t bytePos = (bitPos + static_cast<std::size_t>(i)) / 8;
            const int bitIndex = 7 - static_cast<int>((bitPos + static_cast<std::size_t>(i)) % 8);
            const int bit = (compressed[bytePos] >> bitIndex) & 1;
            code = (code << 1) | bit;
        }
        bitPos += static_cast<std::size_t>(codeSize);
        return code;
    };

    while (true) {
        const int code = readCode();
        if (code == -1 || code == kEoiCode) {
            break;
        }
        if (code == kClearCode) {
            resetTable();
            codeSize = 9;
            nextCode = kFirstCode;
            previous.clear();
            continue;
        }

        std::vector<std::uint8_t> entry;
        if (code < nextCode && !table[static_cast<std::size_t>(code)].empty()) {
            entry = table[static_cast<std::size_t>(code)];
        } else if (code == nextCode && !previous.empty()) {
            entry = previous;
            entry.push_back(previous.front());
        } else {
            return false;
        }

        out.insert(out.end(), entry.begin(), entry.end());

        if (!previous.empty() && nextCode < 4096) {
            std::vector<std::uint8_t> newEntry = previous;
            newEntry.push_back(entry.front());
            table[static_cast<std::size_t>(nextCode)] = std::move(newEntry);
            ++nextCode;
            if (nextCode == 511 || nextCode == 1023 || nextCode == 2047) {
                ++codeSize;
            }
        }
        previous = std::move(entry);
    }
    return true;
}

/// Undoes TIFF Predictor=3 (floating point horizontal differencing).
///
/// For this predictor the encoder first rearranges each scanline so that all
/// most-significant bytes of every sample come first, followed by all the
/// next bytes, and so on (a "byte plane" layout), and only then applies byte
/// wise horizontal differencing across that rearranged row. Decoding has to
/// reverse both steps: undo the cumulative byte sum, then de-interleave the
/// byte planes back into normal per-sample byte order. The byte planes are
/// always ordered most-significant-byte first regardless of the TIFF file's
/// declared byte order, so the result is big-endian and has to be swapped
/// back to the file's declared byte order if that is little endian.
void applyFloatingPointPredictor(std::vector<std::uint8_t>& data, int width, int height,
                                 int bytesPerSample, bool bigEndian)
{
    const std::size_t rowBytes = static_cast<std::size_t>(width) * bytesPerSample;
    std::vector<std::uint8_t> shuffled(rowBytes);
    for (int row = 0; row < height; ++row) {
        std::uint8_t* rowPtr = data.data() + static_cast<std::size_t>(row) * rowBytes;

        // Undo the horizontal byte-wise differencing across the whole row.
        for (std::size_t i = 1; i < rowBytes; ++i) {
            rowPtr[i] = static_cast<std::uint8_t>(rowPtr[i] + rowPtr[i - 1]);
        }

        // Undo the byte-plane shuffle: byte plane `b` (0 = most significant)
        // holds byte `b` of every sample in the row, in order. The result is
        // always big-endian per sample.
        for (int col = 0; col < width; ++col) {
            for (int b = 0; b < bytesPerSample; ++b) {
                shuffled[static_cast<std::size_t>(col) * bytesPerSample + b] =
                    rowPtr[static_cast<std::size_t>(b) * width + col];
            }
        }
        if (!bigEndian) {
            for (int col = 0; col < width; ++col) {
                std::uint8_t* sample = shuffled.data() + static_cast<std::size_t>(col) * bytesPerSample;
                std::reverse(sample, sample + bytesPerSample);
            }
        }
        std::memcpy(rowPtr, shuffled.data(), rowBytes);
    }
}

void applyHorizontalPredictor(std::vector<std::uint8_t>& data, int width, int height,
                              int bitsPerSample, bool bigEndian)
{
    const int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample != 2 && bytesPerSample != 4) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        std::uint8_t* rowPtr = data.data() + static_cast<std::size_t>(row) * width * bytesPerSample;
        for (int col = 1; col < width; ++col) {
            std::uint8_t* cur = rowPtr + static_cast<std::size_t>(col) * bytesPerSample;
            std::uint8_t* prev = cur - bytesPerSample;
            if (bytesPerSample == 2) {
                auto readV = [&](const std::uint8_t* p) -> std::uint16_t {
                    return bigEndian ? static_cast<std::uint16_t>((p[0] << 8) | p[1])
                                     : static_cast<std::uint16_t>((p[1] << 8) | p[0]);
                };
                auto writeV = [&](std::uint8_t* p, std::uint16_t v) {
                    if (bigEndian) {
                        p[0] = static_cast<std::uint8_t>(v >> 8);
                        p[1] = static_cast<std::uint8_t>(v & 0xFF);
                    } else {
                        p[1] = static_cast<std::uint8_t>(v >> 8);
                        p[0] = static_cast<std::uint8_t>(v & 0xFF);
                    }
                };
                writeV(cur, static_cast<std::uint16_t>(readV(cur) + readV(prev)));
            } else {
                auto readV = [&](const std::uint8_t* p) -> std::uint32_t {
                    std::uint32_t v = 0;
                    for (int b = 0; b < 4; ++b) {
                        v = (v << 8) | p[bigEndian ? b : (3 - b)];
                    }
                    return v;
                };
                auto writeV = [&](std::uint8_t* p, std::uint32_t v) {
                    for (int b = 0; b < 4; ++b) {
                        p[bigEndian ? b : (3 - b)] = static_cast<std::uint8_t>(v >> (8 * (3 - b)));
                    }
                };
                writeV(cur, readV(cur) + readV(prev));
            }
        }
    }
}

} // namespace

std::uint16_t GeoTiffReader::readU16(std::size_t offset) const
{
    const auto& b = *m_bytes;
    const auto b0 = static_cast<std::uint8_t>(b[offset]);
    const auto b1 = static_cast<std::uint8_t>(b[offset + 1]);
    return m_bigEndian ? static_cast<std::uint16_t>((b0 << 8) | b1)
                       : static_cast<std::uint16_t>((b1 << 8) | b0);
}

std::uint32_t GeoTiffReader::readU32(std::size_t offset) const
{
    const auto& b = *m_bytes;
    const auto b0 = static_cast<std::uint8_t>(b[offset]);
    const auto b1 = static_cast<std::uint8_t>(b[offset + 1]);
    const auto b2 = static_cast<std::uint8_t>(b[offset + 2]);
    const auto b3 = static_cast<std::uint8_t>(b[offset + 3]);
    return m_bigEndian
               ? (static_cast<std::uint32_t>(b0) << 24) | (static_cast<std::uint32_t>(b1) << 16) |
                     (static_cast<std::uint32_t>(b2) << 8) | b3
               : (static_cast<std::uint32_t>(b3) << 24) | (static_cast<std::uint32_t>(b2) << 16) |
                     (static_cast<std::uint32_t>(b1) << 8) | b0;
}

void GeoTiffReader::readTypedValue(std::uint16_t type, std::size_t offset, TagValue& value)
{
    switch (type) {
    case 1: // BYTE
    case 2: // ASCII
        value.ints.push_back(static_cast<std::uint8_t>((*m_bytes)[offset]));
        break;
    case 3: // SHORT
        value.ints.push_back(readU16(offset));
        break;
    case 4: // LONG
        value.ints.push_back(readU32(offset));
        break;
    case 11: { // FLOAT
        std::uint32_t bits = readU32(offset);
        float f = 0.0f;
        std::memcpy(&f, &bits, sizeof(f));
        value.doubles.push_back(static_cast<double>(f));
        break;
    }
    case 12: { // DOUBLE
        const auto hi = readU32(offset + (m_bigEndian ? 0 : 4));
        const auto lo = readU32(offset + (m_bigEndian ? 4 : 0));
        std::uint64_t bits = (static_cast<std::uint64_t>(hi) << 32) | lo;
        double d = 0.0;
        std::memcpy(&d, &bits, sizeof(d));
        value.doubles.push_back(d);
        break;
    }
    case 5: { // RATIONAL
        const auto num = readU32(offset);
        const auto den = readU32(offset + 4);
        value.doubles.push_back(den != 0 ? static_cast<double>(num) / den : 0.0);
        break;
    }
    default:
        break;
    }
}

bool GeoTiffReader::parseIfd(std::uint32_t ifdOffset, std::string* error)
{
    if (ifdOffset == 0 || static_cast<std::size_t>(ifdOffset) + 2 > m_bytes->size()) {
        setError(error, "TIFF has no image file directory");
        return false;
    }
    const std::uint16_t entryCount = readU16(ifdOffset);
    std::size_t pos = static_cast<std::size_t>(ifdOffset) + 2;
    for (std::uint16_t i = 0; i < entryCount; ++i) {
        if (pos + 12 > m_bytes->size()) {
            setError(error, "TIFF IFD entry out of bounds");
            return false;
        }
        const std::uint16_t tag = readU16(pos);
        const std::uint16_t type = readU16(pos + 2);
        const std::uint32_t count = readU32(pos + 4);
        const std::size_t elemSize = tiffTypeSize(type);
        TagValue value;
        value.type = type;
        if (elemSize > 0 && count > 0) {
            const std::size_t totalSize = elemSize * count;
            std::size_t dataOffset = pos + 8;
            if (totalSize > 4) {
                dataOffset = readU32(pos + 8);
            }
            if (dataOffset + totalSize <= m_bytes->size()) {
                for (std::uint32_t k = 0; k < count; ++k) {
                    const std::size_t off = dataOffset + k * elemSize;
                    readTypedValue(type, off, value);
                }
            }
        }
        m_tags[tag] = std::move(value);
        pos += 12;
    }
    return true;
}

bool GeoTiffReader::parse(const std::vector<char>& bytes, std::string* error)
{
    m_bytes = &bytes;
    if (bytes.size() < 8) {
        setError(error, "TIFF file is too small");
        return false;
    }
    if (bytes[0] == 'I' && bytes[1] == 'I') {
        m_bigEndian = false;
    } else if (bytes[0] == 'M' && bytes[1] == 'M') {
        m_bigEndian = true;
    } else {
        setError(error, "not a TIFF file (bad byte order mark)");
        return false;
    }
    const std::uint32_t ifdOffset = readU32(4);
    return parseIfd(ifdOffset, error);
}

bool GeoTiffReader::read(std::istream& stream, const GeoBounds& fallbackBounds, Raster& raster,
                        std::string* error)
{
    std::vector<char> bytes((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());

    GeoTiffReader tiff;
    if (!tiff.parse(bytes, error)) {
        return false;
    }
    const auto& tags = tiff.tags();

    auto tagValue = [&](std::uint16_t tag) -> const TagValue* {
        const auto it = tags.find(tag);
        return it == tags.end() ? nullptr : &it->second;
    };

    const TagValue* widthTag = tagValue(256);   // ImageWidth
    const TagValue* heightTag = tagValue(257);  // ImageLength
    const TagValue* bitsTag = tagValue(258);    // BitsPerSample
    const TagValue* compressionTag = tagValue(259); // Compression
    const TagValue* sampleFormatTag = tagValue(339); // SampleFormat
    const TagValue* predictorTag = tagValue(317);    // Predictor

    if (widthTag == nullptr || heightTag == nullptr || bitsTag == nullptr) {
        setError(error, "GeoTIFF is missing required image tags");
        return false;
    }

    const int width = static_cast<int>(widthTag->asInt());
    const int height = static_cast<int>(heightTag->asInt());
    const int bitsPerSample = static_cast<int>(bitsTag->asInt());
    const int bytesPerSample = bitsPerSample / 8;
    const int sampleFormat = sampleFormatTag != nullptr ? static_cast<int>(sampleFormatTag->asInt()) : 1;
    const int predictor = predictorTag != nullptr ? static_cast<int>(predictorTag->asInt()) : 1;
    const int compression = compressionTag != nullptr ? static_cast<int>(compressionTag->asInt()) : 1;

    if (width < 2 || height < 2) {
        setError(error, "GeoTIFF raster is too small");
        return false;
    }
    if (bytesPerSample != 2 && bytesPerSample != 4 && bytesPerSample != 8) {
        setError(error, "unsupported GeoTIFF sample size");
        return false;
    }

    std::vector<std::uint8_t> rasterBytes(static_cast<std::size_t>(width) * height *
                                          static_cast<std::size_t>(bytesPerSample));
    // Cloud Optimized GeoTIFFs commonly omit data for "sparse" tiles/strips
    // that are entirely empty (e.g. no-data areas); such blocks are encoded
    // with a byte count of 0 (and typically an offset of 0 as well). Track
    // which samples came from such blocks so they can be set to the no-data
    // sentinel afterwards instead of being misread as decoded zero bytes.
    std::vector<bool> sparseSample(static_cast<std::size_t>(width) * height, false);

    auto decodeBlock = [&](std::size_t fileOffset, std::size_t fileLength,
                          std::uint8_t* dest, std::size_t destLen) -> bool {
        if (fileLength == 0) {
            std::memset(dest, 0, destLen);
            return true;
        }
        if (fileOffset + fileLength > bytes.size()) {
            setError(error, "GeoTIFF block is out of bounds");
            return false;
        }
        const std::uint8_t* src = reinterpret_cast<const std::uint8_t*>(bytes.data()) + fileOffset;
        if (compression == 1) { // none
            if (fileLength != destLen) {
                setError(error, "GeoTIFF uncompressed block size mismatch");
                return false;
            }
            std::memcpy(dest, src, destLen);
            return true;
        }
        if (compression == 5) { // LZW
            std::vector<std::uint8_t> compressed(src, src + fileLength);
            std::vector<std::uint8_t> decoded;
            if (!inflateLzw(compressed, destLen, decoded) || decoded.size() != destLen) {
                setError(error, "LZW decompression of GeoTIFF block failed");
                return false;
            }
            std::memcpy(dest, decoded.data(), destLen);
            return true;
        }
        if (compression == 8 || compression == 32946) { // Deflate / Adobe Deflate
            std::vector<std::uint8_t> compressed(src, src + fileLength);
            std::vector<std::uint8_t> decoded;
            if (!inflateDeflate(compressed, decoded) || decoded.size() != destLen) {
                setError(error, "Deflate decompression of GeoTIFF block failed");
                return false;
            }
            std::memcpy(dest, decoded.data(), destLen);
            return true;
        }
        setError(error, "unsupported GeoTIFF compression scheme");
        return false;
    };

    const TagValue* tileWidthTag = tagValue(322);
    const TagValue* tileLengthTag = tagValue(323);
    const TagValue* tileOffsetsTag = tagValue(324);
    const TagValue* tileByteCountsTag = tagValue(325);

    if (tileWidthTag != nullptr && tileLengthTag != nullptr && tileOffsetsTag != nullptr &&
        tileByteCountsTag != nullptr) {
        const int tileWidth = static_cast<int>(tileWidthTag->asInt());
        const int tileLength = static_cast<int>(tileLengthTag->asInt());
        if (tileWidth <= 0 || tileLength <= 0) {
            setError(error, "invalid GeoTIFF tile dimensions");
            return false;
        }
        const int tilesAcross = (width + tileWidth - 1) / tileWidth;
        const int tilesDown = (height + tileLength - 1) / tileLength;
        const std::size_t expectedTiles =
            static_cast<std::size_t>(tilesAcross) * static_cast<std::size_t>(tilesDown);
        if (tileOffsetsTag->ints.size() < expectedTiles || tileByteCountsTag->ints.size() < expectedTiles) {
            setError(error, "GeoTIFF tile index is incomplete");
            return false;
        }

        std::vector<std::uint8_t> tileBuffer(static_cast<std::size_t>(tileWidth) * tileLength *
                                             static_cast<std::size_t>(bytesPerSample));
        for (int ty = 0; ty < tilesDown; ++ty) {
            for (int tx = 0; tx < tilesAcross; ++tx) {
                const std::size_t tileIndex =
                    static_cast<std::size_t>(ty) * tilesAcross + static_cast<std::size_t>(tx);
                const std::size_t fileOffset = tileOffsetsTag->asInt(tileIndex);
                const std::size_t fileLength = tileByteCountsTag->asInt(tileIndex);
                if (!decodeBlock(fileOffset, fileLength, tileBuffer.data(), tileBuffer.size())) {
                    return false;
                }
                if (predictor == 2 && fileLength != 0) {
                    applyHorizontalPredictor(tileBuffer, tileWidth, tileLength, bitsPerSample,
                                            tiff.m_bigEndian);
                } else if (predictor == 3 && fileLength != 0) {
                    applyFloatingPointPredictor(tileBuffer, tileWidth, tileLength, bytesPerSample,
                                               tiff.m_bigEndian);
                }

                const int xBase = tx * tileWidth;
                const int yBase = ty * tileLength;
                const int copyWidth = std::min(tileWidth, width - xBase);
                const int copyHeight = std::min(tileLength, height - yBase);
                for (int row = 0; row < copyHeight; ++row) {
                    const std::uint8_t* srcRow =
                        tileBuffer.data() + static_cast<std::size_t>(row) * tileWidth * bytesPerSample;
                    std::uint8_t* destRow = rasterBytes.data() +
                        (static_cast<std::size_t>(yBase + row) * width + xBase) *
                            static_cast<std::size_t>(bytesPerSample);
                    std::memcpy(destRow, srcRow, static_cast<std::size_t>(copyWidth) * bytesPerSample);
                    if (fileLength == 0) {
                        std::fill_n(sparseSample.begin() +
                                        static_cast<std::ptrdiff_t>(
                                            static_cast<std::size_t>(yBase + row) * width + xBase),
                                    copyWidth, true);
                    }
                }
            }
        }
    } else {
        const TagValue* stripOffsetsTag = tagValue(273);
        const TagValue* rowsPerStripTag = tagValue(278);
        const TagValue* stripByteCountsTag = tagValue(279);
        if (stripOffsetsTag == nullptr || stripByteCountsTag == nullptr) {
            setError(error, "GeoTIFF has neither tile nor strip layout");
            return false;
        }
        const int rowsPerStrip =
            rowsPerStripTag != nullptr ? static_cast<int>(rowsPerStripTag->asInt()) : height;
        const int stripCount = (height + rowsPerStrip - 1) / rowsPerStrip;
        if (static_cast<int>(stripOffsetsTag->ints.size()) < stripCount ||
            static_cast<int>(stripByteCountsTag->ints.size()) < stripCount) {
            setError(error, "GeoTIFF strip index is incomplete");
            return false;
        }

        for (int strip = 0; strip < stripCount; ++strip) {
            const int rowStart = strip * rowsPerStrip;
            const int rowsHere = std::min(rowsPerStrip, height - rowStart);
            const std::size_t destLen =
                static_cast<std::size_t>(width) * rowsHere * static_cast<std::size_t>(bytesPerSample);
            std::uint8_t* dest = rasterBytes.data() +
                static_cast<std::size_t>(rowStart) * width * static_cast<std::size_t>(bytesPerSample);
            const std::size_t fileOffset = stripOffsetsTag->asInt(static_cast<std::size_t>(strip));
            const std::size_t fileLength = stripByteCountsTag->asInt(static_cast<std::size_t>(strip));
            if (!decodeBlock(fileOffset, fileLength, dest, destLen)) {
                return false;
            }
            if (fileLength == 0) {
                std::fill_n(sparseSample.begin() +
                                static_cast<std::ptrdiff_t>(static_cast<std::size_t>(rowStart) * width),
                            static_cast<std::size_t>(width) * rowsHere, true);
            } else if (predictor == 2) {
                std::vector<std::uint8_t> rowsView(dest, dest + destLen);
                applyHorizontalPredictor(rowsView, width, rowsHere, bitsPerSample, tiff.m_bigEndian);
                std::memcpy(dest, rowsView.data(), destLen);
            } else if (predictor == 3) {
                std::vector<std::uint8_t> rowsView(dest, dest + destLen);
                applyFloatingPointPredictor(rowsView, width, rowsHere, bytesPerSample, tiff.m_bigEndian);
                std::memcpy(dest, rowsView.data(), destLen);
            }
        }
    }

    // Decode samples to double, row 0 = northernmost (TIFF row 0 is the top row,
    // which matches GridHeightDataSource's expected row order).
    constexpr double tiffNoData = -32768.0;
    std::vector<double> grid(static_cast<std::size_t>(width) * height, tiffNoData);
    for (std::size_t i = 0; i < grid.size(); ++i) {
        if (sparseSample[i]) {
            grid[i] = tiffNoData;
            continue;
        }
        const std::uint8_t* p = rasterBytes.data() + i * static_cast<std::size_t>(bytesPerSample);
        double value = tiffNoData;
        if (bytesPerSample == 2) {
            std::uint16_t raw = 0;
            if (tiff.m_bigEndian) {
                raw = static_cast<std::uint16_t>((p[0] << 8) | p[1]);
            } else {
                raw = static_cast<std::uint16_t>((p[1] << 8) | p[0]);
            }
            if (sampleFormat == 2) { // signed int
                value = static_cast<double>(static_cast<std::int16_t>(raw));
            } else {
                value = static_cast<double>(raw);
            }
        } else if (bytesPerSample == 4) {
            std::uint32_t raw = 0;
            for (int b = 0; b < 4; ++b) {
                raw = (raw << 8) | p[tiff.m_bigEndian ? b : (3 - b)];
            }
            if (sampleFormat == 3) { // IEEE float
                float f = 0.0f;
                std::memcpy(&f, &raw, sizeof(f));
                value = static_cast<double>(f);
            } else if (sampleFormat == 2) {
                value = static_cast<double>(static_cast<std::int32_t>(raw));
            } else {
                value = static_cast<double>(raw);
            }
        } else { // 8 bytes: double
            std::uint64_t raw = 0;
            for (int b = 0; b < 8; ++b) {
                raw = (raw << 8) | static_cast<std::uint64_t>(p[tiff.m_bigEndian ? b : (7 - b)]);
            }
            double d = 0.0;
            std::memcpy(&d, &raw, sizeof(d));
            value = d;
        }
        grid[i] = value;
    }

    // Prefer the GeoTIFF georeference if present (ModelPixelScale + ModelTiepoint).
    GeoBounds effectiveBounds = fallbackBounds;
    const TagValue* pixelScaleTag = tagValue(33550);
    const TagValue* tiePointTag = tagValue(33922);
    if (pixelScaleTag != nullptr && tiePointTag != nullptr && pixelScaleTag->doubles.size() >= 2 &&
        tiePointTag->doubles.size() >= 6) {
        const double scaleX = pixelScaleTag->doubles[0];
        const double scaleY = pixelScaleTag->doubles[1];
        const double tiePixel = tiePointTag->doubles[0];
        const double tieLine = tiePointTag->doubles[1];
        const double tieX = tiePointTag->doubles[3];
        const double tieY = tiePointTag->doubles[4];
        const double originLon = tieX - tiePixel * scaleX;
        const double originLat = tieY + tieLine * scaleY;
        const double minLon = originLon;
        const double maxLon = originLon + width * scaleX;
        const double maxLat = originLat;
        const double minLat = originLat - height * scaleY;
        effectiveBounds = GeoBounds{minLat, maxLat, minLon, maxLon};
    }

    raster.columns = width;
    raster.rows = height;
    raster.values = std::move(grid);
    raster.bounds = effectiveBounds;
    raster.noDataValue = tiffNoData;

    m_tags = tags;
    m_bigEndian = tiff.m_bigEndian;

    return true;
}

std::shared_ptr<GridHeightDataSource> GeoTiffReader::toGridHeightDataSource(const Raster& raster,
                                                                            const std::string& name)
{
    const double latSpan = raster.bounds.maxLatitudeDeg - raster.bounds.minLatitudeDeg;
    const double resolutionM = latSpan / (raster.rows - 1) * kMetresPerDegree;
    return std::make_shared<GridHeightDataSource>(name, raster.bounds, raster.columns, raster.rows,
                                                  raster.values, resolutionM, raster.noDataValue);
}

} // namespace geo
