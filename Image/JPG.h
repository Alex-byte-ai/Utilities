#pragma once

#include <cstdint>
#include <variant>

#include "Image/Format.h"
#include "Image/ANYF.h"

#include "BitIO.h"

// https://en.wikipedia.org/wiki/JPEG
// https://en.wikipedia.org/wiki/YCbCr
// https://docs.fileformat.com/image/jpeg/

namespace ImageConvert
{
// JPG pipeline:
// (File) sectors <-> Huffman-code <-> quantization <-> DCT <-> scaling <-> color space conversion (Pixels)

// JPEG segment container
class JPEG
{
public:
    struct Segment
    {
        // Read exactly `length` bytes from reader (body)
        virtual bool read( ReaderBase &r, uint16_t length ) = 0;

        // Write entire segment (marker + length + body)
        virtual bool write( WriterBase &w ) const = 0;

        virtual ~Segment() = default;
    };

    void read( ReaderBase &r );
    void write( WriterBase &w ) const;

    template<typename S>
    const S *findSingle() const
    {
        const S *segment = nullptr;

        for( auto& s : segments )
        {
            auto next = dynamic_cast<S*>( s.get() );

            if( segment && next )
                return nullptr;

            if( next )
                segment = next;
        }

        return segment;
    }

    template<typename S>
    std::vector<const S*> find() const
    {
        std::vector<const S*> result;
        const S *segment;

        for( auto& s : segments )
        {
            segment = dynamic_cast<S*>( s.get() );
            if( segment )
                result.push_back( segment );
        }

        return result;
    }

private:
    std::vector<std::shared_ptr<Segment>> segments;
};

// Generic segment (Stores raw body)
struct SegmentGeneric : public JPEG::Segment
{
    uint8_t marker;
    bool hasLength;
    std::vector<uint8_t> data;

    SegmentGeneric( uint8_t m, bool l );
    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// SOI (Start of image)
// Count: 1
struct SegmentSOI : public JPEG::Segment
{
    bool read( ReaderBase &, uint16_t ) override;
    bool write( WriterBase &w ) const override;
};

// EOI (End of image)
// Count: 1
struct SegmentEOI : public JPEG::Segment
{
    bool read( ReaderBase &, uint16_t ) override;
    bool write( WriterBase &w ) const override;
};

// TEM (Temporary/forbidden)
// Count: 0 or 1
struct SegmentTEM : public JPEG::Segment
{
    bool read( ReaderBase &, uint16_t ) override;
    bool write( WriterBase &w ) const override;
};

// JFIF (APP0)
// Count: any
#pragma pack(push,1)
struct DataJFIF
{
    char identifier[5]; // "JFIF\0"
    uint8_t versionMajor;
    uint8_t versionMinor;
    uint8_t units;
    uint16_t xDensity; // Big-endian in file
    uint16_t yDensity; // Big-endian in file
    uint8_t xThumbnail;
    uint8_t yThumbnail;
};
#pragma pack(pop)

struct SegmentJFIF : public JPEG::Segment
{
    DataJFIF info;
    std::vector<uint8_t> thumbnail;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// EXIF (APP1)
// Count: any
struct SegmentEXIF : public JPEG::Segment
{
    std::vector<uint8_t> tiffData;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// ICC (APP2)
// Count: any
#pragma pack(push,1)
struct DataICC
{
    char identifier[12];
    uint8_t seqNumber;
    uint8_t totalChunks;
};
#pragma pack(pop)

struct SegmentICC : public JPEG::Segment
{
    DataICC hdr;
    std::vector<uint8_t> chunkData;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// Adobe (APP14)
// Count: any
#pragma pack(push,1)
struct DataAdobe
{
    char identifier[5]; // "Adobe"
    uint16_t version; // Big-endian in file
    uint16_t flags0; // Big-endian in file
    uint16_t flags1; // Big-endian in file
    uint8_t colorTransform;
};
#pragma pack(pop)

struct SegmentAdobe : public JPEG::Segment
{
    DataAdobe hdr;
    std::vector<uint8_t> extraData;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// COM (Comment)
// Count: any
struct SegmentCOM : public JPEG::Segment
{
    std::string commentary;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// SOF (Start of frame)
// Count: 1
#pragma pack(push,1)
struct DataSOF
{
    uint8_t samplePrecision;
    uint16_t imageHeight; // Big-endian in file
    uint16_t imageWidth; // Big-endian in file
    uint8_t numComponents;

    struct Component
    {
        uint8_t componentId;
        uint8_t samplingFactors;
        uint8_t quantTableId;
    };
};
#pragma pack(pop)

struct SegmentSOF : public JPEG::Segment
{
    DataSOF header;
    std::vector<DataSOF::Component> components;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;

    virtual bool writeMarker( WriterBase &w ) const = 0;
};

// Baseline DCT
struct SegmentSOF0 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Extended sequential DCT
struct SegmentSOF1 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Progressive DCT
struct SegmentSOF2 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Lossless, predictive
struct SegmentSOF3 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Differential sequential
struct SegmentSOF5 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Differential progressive
struct SegmentSOF6 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Differential lossless
struct SegmentSOF7 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Arithmetic-coded sequential
struct SegmentSOF9 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Arithmetic-coded progressive
struct SegmentSOF10 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Arithmetic-coded lossless
struct SegmentSOF11 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Arithmetic-coded differential sequential
struct SegmentSOF13 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Arithmetic-coded differential progressive
struct SegmentSOF14 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// Arithmetic-coded differential lossless
struct SegmentSOF15 : public SegmentSOF
{
    virtual bool writeMarker( WriterBase &w ) const override;
};

// DNL (Define Number of Lines)
// Count: 0 or 1
struct SegmentDNL : public JPEG::Segment
{
    uint16_t numberOfLines = 0;  // Big-endian in file

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// DAC (Define arithmetic coding)
// Count: any
#pragma pack(push,1)
struct DataDAC
{
    struct Table
    {
        uint8_t tb;
        uint8_t cs;
        uint8_t tc;
    };
};
#pragma pack(pop)

struct SegmentDAC : public JPEG::Segment
{
    std::vector<DataDAC::Table> tables;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// DQT (Define quantization table)
// Count: any
#pragma pack(push,1)
struct DataDQT
{
    struct Table8
    {
        uint8_t pq_tq;
        uint8_t values[64];
    };

    struct Table16
    {
        uint8_t pq_tq;
        uint16_t values[64]; // Big-endian in file
    };
};
#pragma pack(pop)

struct SegmentDQT : public JPEG::Segment
{
    using Table = std::variant<DataDQT::Table8, DataDQT::Table16>;
    std::vector<Table> tables;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// DHT (Define Huffman table)
// Count: any
struct SegmentDHT : public JPEG::Segment
{
    struct Table
    {
        uint8_t tc_th;
        uint8_t counts[16];
        std::vector<uint8_t> symbols;
    };

    std::vector<Table> tables;

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// DRI (Restart interval)
// Count: 0 or 1
struct SegmentDRI : public JPEG::Segment
{
    uint16_t restartInterval = 0; // Big-endian in file

    bool read( ReaderBase &r, uint16_t length ) override;
    bool write( WriterBase &w ) const override;
};

// SOS (Start of scan)
// Includes entropy-data
// Count: more than 1
#pragma pack(push,1)
struct DataSOS
{
    struct Component
    {
        uint8_t componentId;
        uint8_t huffmanSelectors;
    };
};
#pragma pack(pop)

struct SegmentSOS : public JPEG::Segment
{
    uint8_t numScanComponents = 0;
    std::vector<DataSOS::Component> components;

    uint8_t spectralStart = 0;
    uint8_t spectralEnd = 0;
    uint8_t successiveApproximation = 0;

    struct Entropy
    {
        std::optional<uint8_t> restartMarker;
        std::vector<uint8_t> data;
    };

    // Entropy-coded bytes between SOS header and next non-restart marker
    std::vector<uint8_t> rawEntropy;

    // Processed byte-stuffing and restart markers
    std::vector<Entropy> entropy;

    // If `read` consumes the terminating marker after entropy, it is stored here
    std::optional<uint8_t> nextMarker;

    bool read( ReaderBase &r, uint16_t length ) override;

    // Write SOS marker, header and entropy bytes. Does not write consumed nextMarker
    bool write( WriterBase &w ) const override;
};

struct Huffman : public Compression
{
    std::shared_ptr<JPEG> image;

    const SegmentSOF* sof;
    const SegmentDRI* dri;
    std::vector<const SegmentDHT*> dht;
    std::vector<const SegmentSOS*> sos;

    Huffman( std::shared_ptr<JPEG> image, unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;

    // Decompress a Huffman-coded scan
    // Input: entropy data extracted from the SOS segments
    // Output: serialized current coefficient arrays { uint32_t count, {uint8_t compId, int32_t coeffs[64]} blocks[count] }
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct Arithmetic : public Compression
{
    std::shared_ptr<JPEG> image;

    const SegmentSOF* sof;
    const SegmentDRI* dri;
    std::vector<const SegmentDAC*> dac;
    std::vector<const SegmentSOS*> sos;

    Arithmetic( std::shared_ptr<JPEG> image, unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct Quantization : public Compression
{
    std::shared_ptr<JPEG> image;

    const SegmentSOF* sof;
    std::vector<const SegmentDQT*> dqt;

    Quantization( std::shared_ptr<JPEG> image, unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;

    // Input: output of Huffman::decompress
    // Output: same layout, but each coefficient replaced by dequantized value
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct DCT : public Compression
{
    std::shared_ptr<JPEG> image;

    const SegmentSOF* sof;

    DCT( std::shared_ptr<JPEG> image, unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;

    // IDCT
    // Input: dequantized coefficients
    // Output: spatial samples (before level shift) { uint32_t count, { uint8_t compId, int32_t[64] } blocks[count] }
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct BlockGrouping : public Compression
{
    std::shared_ptr<JPEG> image;

    const SegmentSOF* sof;

    BlockGrouping( std::shared_ptr<JPEG> image, unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;

    // Input: DCT output
    // Output: uint16_t widthBlocks, uint16_t heightBlocks, uint8_t count, { uint8_t compId, uint8_t samplingFactors, uint8_t quantTableId, uint32_t numBlocks, {int16_t[64]} blocks[numBlocks] }
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct Scale : public Compression
{
    std::shared_ptr<JPEG> image;

    const SegmentSOF* sof;

    Scale( std::shared_ptr<JPEG> image, unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;

    // Bilinear chroma up-sampling
    // Input: BlockGrouping output
    // Output: uint16_t width, uint16_t height, uint8_t componentCount, {uint8_t compId, uint8_t elemSize, int16_t values[width*height]} component[componentCount]
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct YCbCrK : public Compression
{
    std::shared_ptr<JPEG> image;

    YCbCrK( std::shared_ptr<JPEG> image, unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;

    // Color conversion
    // Input: Scale output
    // Output: uint16_t width, uint16_t height, uint8_t channels, uint8_t bitDepth, interleaved pixel data (RGB)
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct CMYK : public Compression
{
    std::shared_ptr<JPEG> image;

    CMYK( std::shared_ptr<JPEG> image, unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;

    // Color conversion
    // Same as YCbCrK
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

void makeJpg( const Reference &ref, Format &format, HeaderWriter *write );
}
