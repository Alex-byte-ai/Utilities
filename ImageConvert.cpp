#include "ImageConvert.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <zlib.h>
#pragma GCC diagnostic pop

#include <algorithm>
#include <vector>
#include <memory>
#include <deque>
#include <set>

#include <Bits.h>

#include "BasicMemory.h"
#include "BasicMath.h"
#include "Exception.h"

namespace ImageConvert
{
// ---------------------------------------------------------------------------
// Headers
// ---------------------------------------------------------------------------

// Ensure structures are packed without padding.
#pragma pack(push, 1)

// https://en.wikipedia.org/wiki/BMP_file_format
// https://www.w3.org/TR/PNG-Filters.html

#define BI_RGB 0
#define BI_RLE4 2
#define BI_RLE8 1
#define BI_BITFIELDS 3

#define PNG_GRAYSCALE 0
#define PNG_TRUECOLOR 2
#define PNG_INDEXED 3
#define PNG_GRAYSCALE_ALPHA 4
#define PNG_TRUECOLOR_ALPHA 6

#define PNG_NONE 0
#define PNG_SUB 1
#define PNG_UP 2
#define PNG_AVERAGE 3
#define PNG_PAETH 4

// 14 bytes at the very start of the file
struct BITMAPFILEHEADER
{
    uint16_t bfType;       // Signature: must be 0x4D42 ("BM")
    uint32_t bfSize;       // Total file size in bytes
    uint16_t bfReserved1;  // Reserved (must be 0)
    uint16_t bfReserved2;  // Reserved (must be 0)
    uint32_t bfOffBits;    // Offset to the beginning of the pixel data
};

// 12 bytes (older OS/2 variant)
struct BITMAPCOREHEADER
{
    uint32_t bcSize;      // Size of this header (must be 12)
    uint16_t bcWidth;     // Image width in pixels
    uint16_t bcHeight;    // Image height in pixels
    uint16_t bcPlanes;    // Number of color planes (must be 1)
    uint16_t bcBitCount;  // Bits per pixel (e.g. 1, 4, 8, 24)
};

// 64 bytes (OS/2 variant)
struct BMPCOREHEADER2
{
    uint32_t bcSize;          // Size of this header (must be 64)
    uint32_t bcWidth;         // Image width in pixels
    uint32_t bcHeight;        // Image height in pixels
    uint16_t bcPlanes;        // Number of color planes (must be 1)
    uint16_t bcBitCount;      // Bits per pixel (e.g., 1, 4, 8, 16, 24)
    uint32_t bcCompression;   // Compression method (BI_RGB, etc.)
    uint32_t bcImageSize;     // Size of image data in bytes (may be 0 if uncompressed)
    int32_t  bcXPelsPerMeter; // Horizontal resolution (pixels per meter)
    int32_t  bcYPelsPerMeter; // Vertical resolution (pixels per meter)
    uint32_t bcClrUsed;       // Number of colors in the palette (0 means default)
    uint32_t bcClrImportant;  // Number of important colors (0 means all)

    // Additional fields for OS/2 2.x:
    uint16_t bcUnits;         // Units for resolution (e.g., pixels per meter)
    uint16_t bcReserved;      // Reserved (must be 0)
    uint16_t bcRecording;     // Recording mode (halftoning method)
    uint16_t bcRendering;     // Rendering mode (halftoning algorithm).
    uint32_t bcSize1;         // Reserved for halftoning algorithm (size parameter 1)
    uint32_t bcSize2;         // Reserved for halftoning algorithm (size parameter 2)
    uint32_t bcColorEncoding; // Color encoding (typically RGB)
    uint32_t bcIdentifier;    // Application‐specific identifier
};

// 40 bytes (most common Windows header)
struct BITMAPINFOHEADER
{
    uint32_t biSize;           // Size of this header (must 40 bytes)
    int32_t  biWidth;          // Image width in pixels
    int32_t  biHeight;         // Image height in pixels (positive means bottom-up)
    uint16_t biPlanes;         // Number of color planes (must be 1)
    uint16_t biBitCount;       // Bits per pixel
    uint32_t biCompression;    // Compression method (0 = BI_RGB, 3 = BI_BITFIELDS, etc.)
    uint32_t biSizeImage;      // Size of the raw image data (can be 0 for uncompressed images)
    int32_t  biXPelsPerMeter;  // Horizontal resolution (pixels per meter)
    int32_t  biYPelsPerMeter;  // Vertical resolution (pixels per meter)
    uint32_t biClrUsed;        // Number of colors in the color palette (0 means default)
    uint32_t biClrImportant;   // Number of important colors (0 means all)
};

// 52 bytes (extends BITMAPINFOHEADER with RGB masks)
struct BITMAPV2INFOHEADER
{
    BITMAPINFOHEADER info;
    uint32_t redMask;       // Mask for red channel
    uint32_t greenMask;     // Mask for green channel
    uint32_t blueMask;      // Mask for blue channel
};

// 56 bytes (extends V2 with an alpha mask)
struct BITMAPV3INFOHEADER
{
    BITMAPV2INFOHEADER v2;
    uint32_t alphaMask;     // Mask for alpha channel
};

// Structures for color endpoints used in V4/V5 headers
struct CIEXYZ
{
    int32_t ciexyzX;
    int32_t ciexyzY;
    int32_t ciexyzZ;
};

struct CIEXYZTRIPLE
{
    CIEXYZ ciexyzRed;
    CIEXYZ ciexyzGreen;
    CIEXYZ ciexyzBlue;
};

// 108 bytes (adds masks, color space info, endpoints, and gamma)
struct BITMAPV4HEADER
{
    BITMAPINFOHEADER info;
    uint32_t bV4RedMask;        // Red channel mask
    uint32_t bV4GreenMask;      // Green channel mask
    uint32_t bV4BlueMask;       // Blue channel mask
    uint32_t bV4AlphaMask;      // Alpha channel mask
    uint32_t bV4CSType;         // Color space type (e.g., LCS_sRGB)
    CIEXYZTRIPLE bV4Endpoints;  // Color space endpoints (red, green, blue)
    uint32_t bV4GammaRed;       // Gamma for red channel
    uint32_t bV4GammaGreen;     // Gamma for green channel
    uint32_t bV4GammaBlue;      // Gamma for blue channel
};

// 124 bytes (extends V4 with ICC profile info)
struct BITMAPV5HEADER
{
    BITMAPV4HEADER v4;
    uint32_t bV5Intent;       // Rendering intent
    uint32_t bV5ProfileData;  // Offset to ICC profile data
    uint32_t bV5ProfileSize;  // Size of ICC profile data
    uint32_t bV5Reserved;     // Reserved, must be 0
};

class ReaderBase;
class WriterBase;

// 8 bytes, the PNG signature
struct PNGSignature
{
    uint8_t signature[8] {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    bool read( ReaderBase &r );
    bool write( WriterBase &w ) const;
};

// A generic PNG chunk header
// Every chunk starts with a 4-byte length and a 4-byte type
struct PNGChunkHeader
{
    uint32_t length; // Length of the chunk's data in bytes (big-endian)
    char type[4];    // Chunk type (e.g., "IHDR")

    bool is( const char *string ) const;
    void set( const char *string );
};

// 13 bytes the IHDR chunk data
struct PNGIHDRData
{
    uint32_t width;            // Image width in pixels (big-endian)
    uint32_t height;           // Image height in pixels (big-endian)
    uint8_t bitDepth;          // Bit depth per channel
    uint8_t colorType;         // Color type (e.g., PNG_GRAYSCALE, PNG_TRUECOLOR, etc.)
    uint8_t compressionMethod; // Compression method (PNG always uses 0)
    uint8_t filterMethod;      // Filter method (PNG always uses 0)
    uint8_t interlaceMethod;   // Interlace method (0 for no interlace, 1 for Adam7 interlacing)
};

#pragma pack(pop)

struct PNGChunk
{
    PNGChunkHeader meta;
    std::vector<uint8_t> data;
    uint32_t crc;

    bool read( ReaderBase &r, const std::function<bool( const PNGChunkHeader & )> &include = nullptr );
    bool write( WriterBase &w ) const;

    void updateCrc();

    uint32_t calculateCrc() const;
    unsigned size() const;
};

// ---------------------------------------------------------------------------
// Reference
// ---------------------------------------------------------------------------

Reference::Reference()
{
    link = nullptr;
    bytes = 0;
    w = h = 0;
}

Reference::Reference( Reference &&other ) noexcept
{
    if( clear )
        clear( *this );

    reset = std::move( other.reset );
    clear = std::move( other.clear );
    format = std::move( other.format );
    bytes = other.bytes;
    link = other.link;
    w = other.w;
    h = other.h;

    other.reset = nullptr;
    other.clear = nullptr;
    other.format.reset();
    other.link = nullptr;
    other.bytes = 0;
    other.w = other.h = 0;
}

Reference::~Reference()
{
    if( clear )
        clear( *this );
}

Reference &Reference::operator=( Reference &&other ) noexcept
{
    if( this == &other )
        return *this;

    if( clear )
        clear( *this );

    reset = std::move( other.reset );
    clear = std::move( other.clear );
    format = std::move( other.format );
    bytes = other.bytes;
    link = other.link;
    w = other.w;
    h = other.h;

    other.reset = nullptr;
    other.clear = nullptr;
    other.format.reset();
    other.link = nullptr;
    other.bytes = 0;
    other.w = other.h = 0;

    return *this;
}

bool Reference::operator==( const Reference &other ) const
{
    return
        w == other.w && h == other.h &&
        format == other.format &&
        bytes == other.bytes &&
        ( ( !link && !other.link ) || compare( link, other.link, bytes ) );
}

// Creates reference to purely internal data, which starts empty
void Reference::fill()
{
    if( clear )
        clear( *this );

    format.reset();
    link = nullptr;
    bytes = 0;
    w = h = 0;

    reset = []( Reference & ref )
    {
        delete[]( uint8_t * )ref.link;
        if( ref.bytes > 0 )
        {
            ref.link = new uint8_t[ref.bytes];
        }
        else
        {
            ref.link = nullptr;
        }
        return true;
    };

    clear = []( Reference & ref )
    {
        delete[]( uint8_t * )ref.link;
    };
}

// ---------------------------------------------------------------------------
// Bit–level helpers
// ---------------------------------------------------------------------------

template<typename T>
struct BitPointer
{
    T *pointer = nullptr;
    unsigned bitOffset = 0;

    BitPointer &operator=( T *p )
    {
        pointer = p;
        bitOffset = 0;
        return *this;
    }

    operator const BitPointer<const T>()
    {
        BitPointer<const T> result;
        result.pointer = pointer;
        result.bitOffset = bitOffset;
        return result;
    }

    void addBits( unsigned delta )
    {
        bitOffset += delta;
        auto blockBits = 8 * sizeof( T );
        pointer += bitOffset / blockBits;
        bitOffset = bitOffset % blockBits;
    }
};

static inline unsigned countSetBits( uint32_t mask )
{
    unsigned count = 0;
    while( mask )
    {
        count += mask & 1;
        mask >>= 1;
    }
    return count;
}

static inline unsigned countTrailingZeros( uint32_t mask )
{
    if( mask == 0 )
        return 8 * sizeof( mask );

    unsigned count = 0;
    while( ( mask & 1 ) == 0 )
    {
        ++count;
        mask >>= 1;
    }
    return count;
}

static inline uint32_t decodeBe32( uint32_t v )
{
    auto p = ( uint8_t * )&v;
    return ( uint32_t( p[0] ) << 24 ) | ( uint32_t( p[1] ) << 16 ) | ( uint32_t( p[2] ) << 8 ) | uint32_t( p[3] );
}

static inline uint32_t encodeBe32( uint32_t v )
{
    uint32_t r = 0;
    auto p = ( uint8_t * )&r;
    p[0] = ( v >> 24 ) & 0xFF;
    p[1] = ( v >> 16 ) & 0xFF;
    p[2] = ( v >> 8 ) & 0xFF;
    p[3] = v & 0xFF;
    return r;
}

class ReaderBase
{
public:
    virtual bool read( unsigned bits, BitList &value ) = 0;
    virtual bool read( unsigned bytes, void *value ) = 0;
    virtual ~ReaderBase()
    {}
};

class WriterBase
{
public:
    virtual bool write( unsigned bits, BitList value ) = 0;
    virtual bool write( unsigned bytes, const void *value ) = 0;
    virtual ~WriterBase()
    {}
};

// ---------------------------------------------------------------------------
// Format
// ---------------------------------------------------------------------------

struct Format;
using Pixel = std::vector<BitList>;
using Color = std::vector<double>;
using HeaderWriter = std::function<void( const Format &, Reference & )>;

// Holds one channel ('A' – 'Z', '_' for unused) and its bit–width.
struct Channel
{
    char channel;
    unsigned bits;

    // Maximum value, that can be stored in this channel
    BitList max() const
    {
        return bits > 0 ? ( ( ( BitList )1 ) << bits ) - 1 : 0;
    }

    bool operator==( const Channel &other ) const
    {
        return channel == other.channel && bits == other.bits;
    }
};

// Channel with its position in channel mask
struct OffsettedChannel : public Channel
{
    // Bit offset in mask
    unsigned offset;
};

struct Replacement
{
    // Number of a channel to be replaced
    unsigned id;

    // Value will be extracted from channel of source format
    std::optional<char> channel;

    // Constant will be used
    std::optional<BitList> constant;
};

struct PixelFormat
{
    std::vector<Channel> channels;

    // Bits per pixel of this format, total bit count for channels
    unsigned bits = 0;

    std::vector<Replacement> replacements;

    // Calculates bits
    void calculateBits()
    {
        bits = 0;
        for( const auto &channel : channels )
            bits += channel.bits;
    }

    void copy( const PixelFormat &other )
    {
        replacements = other.replacements;
        channels = other.channels;
        bits = other.bits;
    }

    void clear()
    {
        replacements.clear();
        channels.clear();
        bits = 0;
    }

    std::optional<unsigned> id( char channel ) const
    {
        unsigned result = 0;
        for( auto c : channels )
        {
            if( c.channel == channel )
                break;
            ++result;
        }
        if( result < channels.size() )
            return result;
        return {};
    }

    const Replacement *replace( unsigned id, const PixelFormat &source, std::optional<unsigned> &srcId ) const
    {
        srcId.reset();
        for( auto &replacement : replacements )
        {
            if( replacement.id == id )
            {
                if( replacement.channel )
                {
                    srcId = source.id( *replacement.channel );
                    if( srcId )
                        return &replacement;
                }
                if( replacement.constant )
                    return &replacement;
            }
        }
        return nullptr;
    }

    bool operator==( const PixelFormat &other ) const
    {
        return channels == other.channels;
    }
};

struct Compression : public PixelFormat
{
    // Size of the compressed data
    unsigned size;

    Compression( unsigned s, const PixelFormat &pfmt )
    {
        copy( pfmt );
        size = s;
    }

    virtual void compress( Format &fmt, const Reference &source, Reference &destination ) = 0;
    virtual void decompress( Format &fmt, const Reference &source, Reference &destination ) const = 0;

    virtual bool equals( const Compression &other ) const
    {
        return this->PixelFormat::operator==( other );
    }

    virtual std::shared_ptr<Compression> clone() const = 0;

    virtual ~Compression()
    {}
};

struct Palette : public Compression
{
    std::vector<Pixel> samples;

    virtual void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    virtual void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    Palette( unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt )
    {}

    bool equals( const Compression &other ) const override
    {
        if( auto otherPtr = dynamic_cast<const Palette *>( &other ) )
            return samples == otherPtr->samples && this->Compression::operator==( other );
        return false;
    };

    std::shared_ptr<Compression> clone() const override
    {
        auto result = std::make_shared<Palette>( 0, *this );
        result->samples = samples;
        return result;
    };
};

struct Rle : public Compression
{
    unsigned granule = 0;

    Rle( unsigned s, const PixelFormat &pfmt, unsigned g ): Compression( s, pfmt ), granule( g )
    {}

    virtual void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    virtual void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override
    {
        if( auto otherPtr = dynamic_cast<const Rle *>( &other ) )
            return granule == otherPtr->granule && this->Compression::operator==( other );
        return false;
    };

    std::shared_ptr<Compression> clone() const override
    {
        return std::make_shared<Rle>( 0, *this, granule );
    };
};

struct Misc : public Compression
{
    // Those pixels will be considered transparent
    std::optional<Pixel> transparent;

    bool fixX, fixY;

    Misc( unsigned s, bool x, bool y, std::optional<Pixel> t, const PixelFormat &pfmt ) : Compression( s, pfmt ),
        transparent( std::move( t ) ), fixX( x ), fixY( y )
    {}

    virtual void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    virtual void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override
    {
        if( auto misc = dynamic_cast<const Misc *>( &other ) )
            return this->Compression::operator==( other ) &&
                   fixX == misc->fixX &&
                   fixY == misc->fixY &&
                   transparent == misc->transparent;
        return false;
    };

    std::shared_ptr<Compression> clone() const override
    {
        return std::make_shared<Misc>( 0, fixX, fixY, transparent, *this );
    }
};

struct FracturePng : public Compression
{
    FracturePng( unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt )
    {}

    virtual void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    virtual void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override
    {
        if( dynamic_cast<const FracturePng *>( &other ) )
            return this->Compression::operator==( other );
        return false;
    };

    std::shared_ptr<Compression> clone() const override
    {
        return std::make_shared<FracturePng>( 0, *this );
    }
};

struct ZlibPng : public Compression
{
    ZlibPng( unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt )
    {}

    virtual void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    virtual void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override
    {
        if( dynamic_cast<const ZlibPng *>( &other ) )
            return this->Compression::operator==( other );
        return false;
    };

    std::shared_ptr<Compression> clone() const override
    {
        return std::make_shared<ZlibPng>( 0, *this );
    }
};

struct FilterAndInterlacePng : public Compression
{
    // Adam7 parameters: starting offsets and increments
    static constexpr unsigned passStart[7][2] =
    {
        {0, 0}, {4, 0}, {0, 4}, {2, 0}, {0, 2}, {1, 0}, {0, 1}
    };

    static constexpr unsigned passInc[7][2] =
    {
        {8, 8}, {8, 8}, {4, 8}, {4, 4}, {2, 4}, {2, 2}, {1, 2}
    };

    struct Step
    {
        unsigned startX, startY, incX, incY;

        Step( unsigned pass )
        {
            startX = passStart[pass][0];
            startY = passStart[pass][1];
            incX   = passInc[pass][0];
            incY   = passInc[pass][1];
        }

        unsigned x( unsigned origX ) const
        {
            return startX + incX * origX;
        }

        unsigned y( unsigned origY ) const
        {
            return startY + incY * origY;
        }
    };

    struct Size
    {
        unsigned number, scanline;

        Size( unsigned w, unsigned h )
        {
            scanline = w;
            number = h;
        }

        Size( const Step &step, unsigned w, unsigned h )
        {
            scanline = ( w > step.startX ) ? ( ( w - step.startX + step.incX - 1 ) / step.incX ) : 0; // Scanline size in pixels
            number = ( h > step.startY ) ? ( ( h - step.startY + step.incY - 1 ) / step.incY ) : 0;
        }

        unsigned lineBytes( unsigned bits ) const
        {
            return 1 + ( scanline * bits + 7 ) / 8;
        }

        unsigned bytes( unsigned bits ) const
        {
            return number * lineBytes( bits );
        }

        bool empty() const
        {
            return scanline <= 0 || number <= 0;
        }
    };

    bool interlaced;
    int w, h;

    static int paethPredictor( int a, int b, int c );
    static unsigned scoreCandidate( const std::vector<BitList> &candidate );
    std::vector<BitList> applyFilter( const std::vector<BitList> &line, const std::vector<BitList> &previous, unsigned filterType, bool apply ) const;

    FilterAndInterlacePng( bool interlaced, int w, int h, const PixelFormat &pfmt );

    FilterAndInterlacePng( const FilterAndInterlacePng &other )
        : Compression( other.size, other ), interlaced( other.interlaced ), w( other.w ), h( other.h )
    {}

    void calculateSize();

    virtual void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    virtual void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override
    {
        if( auto fip = dynamic_cast<const FilterAndInterlacePng *>( &other ) )
            return this->Compression::operator==( other ) &&
                   interlaced == fip->interlaced &&
                   w == fip->w &&
                   h == fip->h;
        return false;
    };

    std::shared_ptr<Compression> clone() const override
    {
        return std::make_shared<FilterAndInterlacePng>( *this );
    }
};

using CompressionLayers = std::deque<std::shared_ptr<Compression>>;

struct Format : public PixelFormat
{
    CompressionLayers compression;

    // Bytes of meta data before image
    unsigned offset = 0;

    // Number of bytes in line should be divisible by this
    // If it's 0 padding is not used
    unsigned pad = 0;

    // Dimensions
    int w = 0, h = 0;

    // Computes the number of bytes needed for a line
    inline unsigned lineSize( unsigned dbits = 0 ) const
    {
        unsigned bytes = ( Abs( w ) * bits + dbits + 7 ) / 8;

        if( pad > 0 )
        {
            auto remainder = bytes % pad;
            if( remainder > 0 )
                bytes += pad - remainder;
        }

        return bytes;
    }

    // Computes the number of bytes needed for the entire image
    inline unsigned bufferSize( const Compression *peelLayer = nullptr ) const
    {
        if( !compression.empty() )
        {
            auto layer = compression.front().get();

            if( layer == peelLayer )
                layer = compression.size() > 1 ? compression[1].get() : nullptr;

            if( layer )
                return offset + layer->size;
        }

        if( pad <= 0 )
            return offset + ( Abs( w ) * Abs( h ) * bits + 7 ) / 8;

        return offset + Abs( h ) * lineSize();
    }

    bool operator==( const Format &other )const
    {
        if( pad != other.pad || w != other.w || h != other.h )
            return false;

        if( !this->PixelFormat::operator==( other ) )
            return false;

        if( compression.size() != other.compression.size() )
            return false;

        for( size_t i = 0; i < compression.size(); ++i )
        {
            if( !compression[i]->equals( *other.compression[i] ) )
                return false;
        }

        return true;
    }
};

// ---------------------------------------------------------------------------
// BMP
// ---------------------------------------------------------------------------

static void extractBmp( Format &fmt, const void *palettePtr, unsigned colorNumber, bool reserved, bool alpha )
{
    auto misc = [&]()
    {
        std::optional<Pixel> p;
        fmt.compression.push_front( std::make_shared<Misc>( fmt.bufferSize() - fmt.offset, false, true, p, fmt ) );
    };

    if( fmt.bits == 1 || fmt.bits == 4 || fmt.bits == 8 )
    {
        fmt.channels.push_back( { '#', fmt.bits } );

        unsigned colorBytes = reserved ? 4 : 3;
        makeException( colorNumber <= 1u << fmt.bits );

        PixelFormat paletteFmt;
        paletteFmt.channels.push_back( { 'B', 8 } );
        paletteFmt.channels.push_back( { 'G', 8 } );
        paletteFmt.channels.push_back( { 'R', 8 } );
        if( reserved )
            paletteFmt.channels.push_back( { alpha ? 'A' : '_', 8 } );
        paletteFmt.calculateBits();

        auto paletteShared = std::make_shared<Palette>( fmt.bufferSize() - fmt.offset, paletteFmt );
        auto &palette = *paletteShared;
        fmt.compression.push_front( paletteShared );

        auto paletteChannel = ( const uint8_t * )palettePtr;
        for( unsigned i = 0; i < colorNumber; ++i )
        {
            Pixel pixel;
            for( unsigned j = 0; j < colorBytes; ++j )
            {
                uint8_t channel;
                copy( &channel, paletteChannel++, sizeof( channel ) );
                pixel.push_back( channel );
            }
            palette.samples.push_back( pixel );
        }

        fmt.offset += colorBytes * colorNumber;

        misc();
        return;
    }

    if( fmt.bits == 16 )
    {
        fmt.channels.push_back( { 'B', 5 } );
        fmt.channels.push_back( { 'G', 5 } );
        fmt.channels.push_back( { 'R', 5 } );
        fmt.channels.push_back( { '_', 1 } );
        misc();
        return;
    }

    if( fmt.bits == 24 )
    {
        // Standard for 24bpp BI_RGB: 8 bits each for Blue, Green, and Red.
        fmt.channels.push_back( { 'B', 8 } );
        fmt.channels.push_back( { 'G', 8 } );
        fmt.channels.push_back( { 'R', 8 } );
        misc();
        return;
    }

    if( fmt.bits == 32 )
    {
        // Standard for 32bpp BI_RGB: 8 bits for Blue, Green, Red and 8 bits unused.
        fmt.channels.push_back( { 'B', 8 } );
        fmt.channels.push_back( { 'G', 8 } );
        fmt.channels.push_back( { 'R', 8 } );
        fmt.channels.push_back( { '_', 8 } );
        misc();
        return;
    }

    // Not supported in this implementation
    makeException( false );
}

static void extractBmp( Format &fmt, unsigned bytes, const BITMAPCOREHEADER *h )
{
    fmt.offset += h->bcSize;
    fmt.bits = h->bcBitCount;
    fmt.w = h->bcWidth;
    fmt.h = h->bcHeight;
    fmt.pad = 4;

    makeException( fmt.offset <= bytes );

    unsigned restBytes = fmt.bufferSize();
    makeException( restBytes <= bytes );

    unsigned paletteBytes = bytes - restBytes;
    unsigned colorNumber = paletteBytes / 3;

    // It seems, they have padding in there
    // makeException( paletteBytes % 3 == 0 );

    extractBmp( fmt, h + 1, colorNumber, false, false );
}

// Extracts and sorts channels from an array of DWORD masks.
static inline std::vector<Channel> extractBmpChannels( const uint32_t *masks, int numMasks, unsigned totalBits )
{
    const char *channelNames = "RGBA";
    makeException( numMasks <= 4 );

    std::vector<OffsettedChannel> channels;
    for( int i = 0; i < numMasks; i++ )
    {
        uint32_t mask;
        copy( &mask, masks + i, sizeof( mask ) );
        auto offset = countTrailingZeros( mask );
        auto bits = countSetBits( mask );
        makeException( bits <= totalBits );
        totalBits -= bits;
        if( bits )
            channels.push_back( { channelNames[i], bits, offset } );
    }

    std::sort( channels.begin(), channels.end(), []( const OffsettedChannel & a, const OffsettedChannel & b )
    {
        return a.offset < b.offset;
    } );

    std::vector<Channel> result;
    for( const auto &channel : channels )
        result.push_back( channel );

    if( totalBits > 0 )
        result.push_back( { '_', totalBits } );

    return result;
}

// Count the number of masks within header itself, if masks are outside header, use nullptr for masks
static void extractBmp( Format &fmt, unsigned bytes, const BITMAPINFOHEADER *header, int numMasks, const uint32_t *masks, bool reserved, bool alpha )
{
    BITMAPINFOHEADER h;
    copy( &h, header, sizeof( h ) );

    fmt.offset += h.biSize;
    fmt.bits = h.biBitCount;
    fmt.w = h.biWidth;
    fmt.h = h.biHeight;
    fmt.pad = 4;

    makeException( fmt.offset <= bytes );

    auto paletteCount = [&]() -> unsigned
    {
        if( h.biClrUsed > 0 )
            return h.biClrUsed;
        if( h.biBitCount < 16 )
            return 1 << h.biBitCount;
        return 0;
    };

    if( h.biCompression == BI_RGB )
    {
        extractBmp( fmt, header + 1, paletteCount(), reserved, alpha );
        return;
    }

    if( h.biCompression == BI_RLE4 )
    {
        makeException( fmt.bits == 4 );
        extractBmp( fmt, header + 1, paletteCount(), reserved, alpha );
        fmt.compression.push_front( std::make_shared<Rle>( h.biSizeImage, fmt, 4 ) );
        return;
    }

    if( h.biCompression == BI_RLE8 )
    {
        makeException( fmt.bits == 8 );
        extractBmp( fmt, header + 1, paletteCount(), reserved, alpha );
        fmt.compression.push_front( std::make_shared<Rle>( h.biSizeImage, fmt, 8 ) );
        return;
    }

    if( h.biCompression == BI_BITFIELDS )
    {
        if( masks == nullptr )
        {
            masks = ( const uint32_t * )( header + 1 );
            auto change = numMasks * sizeof( uint32_t );
            fmt.offset += change;
            makeException( fmt.offset <= bytes );
        }

        fmt.channels = extractBmpChannels( masks, numMasks, fmt.bits );
        fmt.calculateBits();

        std::optional<Pixel> p;
        fmt.compression.push_front( std::make_shared<Misc>( fmt.bufferSize() - fmt.offset, false, true, p, fmt ) );
        return;
    }

    // Not supported in this implementation
    makeException( false );
}

static void extractBmp( Format &fmt, const void *data, unsigned bytes )
{
    uint32_t size;
    makeException( fmt.offset + sizeof( size ) <= bytes );
    auto h = ( const uint8_t * )data + fmt.offset;
    copy( &size, h, sizeof( size ) );

    if( size == sizeof( BITMAPCOREHEADER ) )
    {
        auto core = ( const BITMAPCOREHEADER * )h;
        extractBmp( fmt, bytes, core );
        return;
    }

    if( size == sizeof( BMPCOREHEADER2 ) )
    {
        auto core2 = ( const BMPCOREHEADER2 * )h;

        BITMAPINFOHEADER info;
        info.biSize = size;
        info.biWidth = core2->bcWidth;
        info.biHeight = core2->bcHeight;
        info.biPlanes = 1;
        info.biBitCount = core2->bcBitCount;
        info.biCompression = core2->bcCompression;
        info.biSizeImage = core2->bcImageSize;
        info.biXPelsPerMeter = core2->bcXPelsPerMeter;
        info.biYPelsPerMeter = core2->bcYPelsPerMeter;
        info.biClrUsed = core2->bcClrUsed;
        info.biClrImportant = core2->bcClrImportant;

        extractBmp( fmt, bytes, &info, 3, ( const uint32_t * )( &core2 + 1 ), false, false );
        return;
    }

    if( size == sizeof( BITMAPINFOHEADER ) )
    {
        auto info = ( const BITMAPINFOHEADER * )h;
        extractBmp( fmt, bytes, info, 3, nullptr, true, false );
        return;
    }

    if( size == sizeof( BITMAPV2INFOHEADER ) )
    {
        auto v2 = ( const BITMAPV2INFOHEADER * )h;
        extractBmp( fmt, bytes, &v2->info, 3, &v2->redMask, true, false );
        return;
    }

    if( size == sizeof( BITMAPV3INFOHEADER ) )
    {
        auto v3 = ( const BITMAPV3INFOHEADER * )h;
        extractBmp( fmt, bytes, &v3->v2.info, 4, &v3->v2.redMask, true, false );
        return;
    }

    if( size == sizeof( BITMAPV4HEADER ) )
    {
        auto v4 = ( const BITMAPV4HEADER * )h;
        extractBmp( fmt, bytes, &v4->info, 4, &v4->bV4RedMask, true, true );
        return;
    }

    if( size == sizeof( BITMAPV5HEADER ) )
    {
        auto v5 = ( const BITMAPV5HEADER * )h;
        extractBmp( fmt, bytes, &v5->v4.info, 4, &v5->v4.bV4RedMask, true, true );
        return;
    }

    // Not supported in this implementation
    makeException( false );
}

static void makeBmp( const Reference &ref, bool fileHeader, bool bmpHeader, Format &format, HeaderWriter *write )
{
    format.w = ref.w;
    format.h = ref.h;
    format.offset = fileHeader ? sizeof( BITMAPFILEHEADER ) : 0;
    format.pad = 4;

    if( !write )
    {
        if( bmpHeader )
            extractBmp( format, ref.link, ref.bytes );
        return;
    }

    if( bmpHeader )
    {
        format.offset += sizeof( BITMAPV4HEADER );
        format.channels.push_back( { 'B', 8 } );
        format.channels.push_back( { 'G', 8 } );
        format.channels.push_back( { 'R', 8 } );
        format.channels.push_back( { 'A', 8 } );
        format.calculateBits();

        std::optional<Pixel> p;
        format.compression.push_front( std::make_shared<Misc>( 0, false, true, p, format ) );
    }

    *write = [fileHeader, bmpHeader]( const Format & fmt, Reference & reference )
    {
        auto pointer = ( uint8_t * )reference.link;
        unsigned offset = 0;

        if( fileHeader )
        {
            BITMAPFILEHEADER fh;
            fh.bfType = 0x4D42;
            fh.bfSize = reference.bytes;
            fh.bfReserved1 = 0;
            fh.bfReserved2 = 0;
            fh.bfOffBits = sizeof( fh ) + sizeof( BITMAPV4HEADER );
            copy( pointer + offset, &fh, sizeof( fh ) );
            offset += sizeof( fh );
        }

        if( bmpHeader )
        {
            BITMAPV4HEADER v4;
            memset( &v4, 0, sizeof( v4 ) );
            v4.info.biSize = sizeof( v4 );
            v4.info.biWidth = fmt.w;
            v4.info.biHeight = fmt.h;
            v4.info.biPlanes = 1;
            v4.info.biBitCount = 32;
            v4.info.biCompression = BI_BITFIELDS;
            v4.info.biSizeImage = 0;
            v4.info.biClrUsed = 0;
            v4.info.biClrImportant = 0;
            v4.bV4RedMask   = 0x00ff0000;
            v4.bV4GreenMask = 0x0000ff00;
            v4.bV4BlueMask  = 0x000000ff;
            v4.bV4AlphaMask = 0xff000000;
            v4.bV4CSType = 0x73524742;
            copy( pointer + offset, &v4, sizeof( v4 ) );
            offset += sizeof( v4 );
        }
    };
};

// ---------------------------------------------------------------------------
// PNG
// ---------------------------------------------------------------------------

// PNG pipeline:
// (File) chunks <-> zlib <-> ( filter >-< interlace ) <-> misc <-> misc <-> palette (Pixels)

// Verifies a PNG signature
bool PNGSignature::read( ReaderBase &r )
{
    uint8_t sgntr[8];
    if( !r.read( sizeof( sgntr ), sgntr ) )
        return false;
    return compare( sgntr, signature, sizeof( signature ) );
}

// Writes a PNG signature
bool PNGSignature::write( WriterBase &w ) const
{
    return w.write( sizeof( signature ), signature );
}

bool PNGChunkHeader::is( const char *string ) const
{
    return stringLength( string ) == sizeof( type ) && compare( type, string, sizeof( type ) );
}

void PNGChunkHeader::set( const char *string )
{
    makeException( stringLength( string ) == sizeof( type ) );
    copy( type, string, sizeof( type ) );
}

// Skips content, if and only if, include returns false
bool PNGChunk::read( ReaderBase &r, const std::function<bool( const PNGChunkHeader & )> &include )
{
    if( !r.read( sizeof( meta.length ), &meta.length ) )
        return false;
    meta.length = decodeBe32( meta.length );

    if( !r.read( sizeof( meta.type ), meta.type ) )
        return false;

    uint8_t *pointer = nullptr;
    if( !include || include( meta ) )
    {
        data.resize( meta.length );
        pointer = data.data();
    }

    makeException( r.read( meta.length, pointer ) ); // Chunk is incorrect, if it claims to occupy more space, than allocated

    if( !r.read( sizeof( crc ), &crc ) )
        return false;
    crc = decodeBe32( crc );

    makeException( !pointer || crc == calculateCrc() );
    return true;
}

bool PNGChunk::write( WriterBase &w ) const
{
    auto t = encodeBe32( meta.length );
    if( !w.write( sizeof( t ), &t ) )
        return false;

    if( !w.write( sizeof( meta.type ), meta.type ) )
        return false;

    makeException( w.write( meta.length, data.data() ) ); // Chunk is incorrect, if it claims to occupy more space, than allocated

    t = encodeBe32( crc );
    if( !w.write( sizeof( t ), &t ) )
        return false;

    return true;
}

void PNGChunk::updateCrc()
{
    crc = calculateCrc();
}

uint32_t PNGChunk::calculateCrc() const
{
    // Start with an initial CRC value of 0
    uint32_t result = crc32( 0L, nullptr, 0 );

    // Update CRC with the chunk type
    result = crc32( result, ( const Bytef * )meta.type, sizeof( meta.type ) );

    // Update CRC with the chunk data
    result = crc32( result, ( const Bytef * )data.data(), meta.length );
    return result;
}

unsigned PNGChunk::size() const
{
    return sizeof( meta ) + meta.length + sizeof( crc );
}

static void extractPng( Format &fmt, ReaderBase &r )
{
    PNGSignature sgntr;
    makeException( sgntr.read( r ) );

    // Read IHDR chunk.
    PNGChunk ihdrChunk;
    makeException( ihdrChunk.read( r ) );

    PNGIHDRData ihdr;
    makeException( ihdrChunk.meta.length == sizeof( ihdr ) && ihdrChunk.meta.is( "IHDR" ) );
    copy( &ihdr, ihdrChunk.data.data(), sizeof( ihdr ) );

    fmt.w = decodeBe32( ihdr.width );
    fmt.h = decodeBe32( ihdr.height );

    auto include = []( const PNGChunkHeader & meta )
    {
        return meta.is( "PLTE" ) || meta.is( "tRNS" );
    };

    // Extracting meta data from other chunks and calculating data size
    PNGChunk chunk;
    unsigned volume = 0, chunks = 0;
    std::optional<PNGChunk> plte, trns;
    while( chunk.read( r, include ) )
    {
        chunks += chunk.size();
        if( chunk.meta.is( "IDAT" ) )
        {
            volume += chunk.meta.length;
        }
        else if( chunk.meta.is( "PLTE" ) )
        {
            makeException( !plte );
            plte = std::move( chunk );
        }
        else if( chunk.meta.is( "tRNS" ) )
        {
            makeException( !trns );
            trns = std::move( chunk );
        }
    }

    if( ihdr.colorType == PNG_TRUECOLOR || ihdr.colorType == PNG_TRUECOLOR_ALPHA )
        plte.reset();

    makeException( plte.has_value() == ( ihdr.colorType == PNG_INDEXED ) );
    makeException( !trns.has_value() || ( ihdr.colorType != PNG_GRAYSCALE_ALPHA && ihdr.colorType != PNG_TRUECOLOR_ALPHA ) );

    fmt.clear();
    fmt.pad = 1;
    fmt.bits = ihdr.bitDepth;
    fmt.offset = sizeof( sgntr ) + ihdrChunk.size();

    switch( ihdr.colorType )
    {
    case PNG_GRAYSCALE:
        makeException( fmt.bits == 1 || fmt.bits == 2 || fmt.bits == 4 || fmt.bits == 8 || fmt.bits == 16 );
        fmt.channels.push_back( { 'G', fmt.bits } );
        if( trns )
            fmt.channels.push_back( { 'A', fmt.bits } );
        break;
    case PNG_TRUECOLOR:
        makeException( fmt.bits == 8 || fmt.bits == 16 );
        fmt.channels.push_back( { 'R', fmt.bits } );
        fmt.channels.push_back( { 'G', fmt.bits } );
        fmt.channels.push_back( { 'B', fmt.bits } );
        if( trns )
            fmt.channels.push_back( { 'A', fmt.bits } );
        break;
    case PNG_INDEXED:
        makeException( fmt.bits == 1 || fmt.bits == 2 || fmt.bits == 4 || fmt.bits == 8 );
        fmt.channels.push_back( { 'R', 8 } );
        fmt.channels.push_back( { 'G', 8 } );
        fmt.channels.push_back( { 'B', 8 } );
        if( trns )
            fmt.channels.push_back( { 'A', 8 } );
        break;
    case PNG_GRAYSCALE_ALPHA:
        makeException( fmt.bits == 8 || fmt.bits == 16 );
        fmt.channels.push_back( { 'G', fmt.bits } );
        fmt.channels.push_back( { 'A', fmt.bits } );
        break;
    case PNG_TRUECOLOR_ALPHA:
        makeException( fmt.bits == 8 || fmt.bits == 16 );
        fmt.channels.push_back( { 'R', fmt.bits } );
        fmt.channels.push_back( { 'G', fmt.bits } );
        fmt.channels.push_back( { 'B', fmt.bits } );
        fmt.channels.push_back( { 'A', fmt.bits } );
        break;
    default:
        makeException( false );
    }

    fmt.calculateBits();

    if( plte )
    {
        unsigned alphaBytes = trns ? 1 : 0;
        unsigned alphaNumber = trns ? trns->meta.length / alphaBytes : 0;

        makeException( fmt.bits > alphaBytes * 8 );

        unsigned colorBytes = fmt.bits / 8 - alphaBytes;
        unsigned colorNumber = plte->meta.length / colorBytes;
        makeException( plte->meta.length % colorBytes == 0 );

        auto palette = std::make_shared<Palette>( 0, fmt );

        fmt.clear();
        fmt.channels.push_back( { '#', ihdr.bitDepth } );
        fmt.calculateBits();
        palette->size = fmt.bufferSize();

        auto paletteChannel = plte->data.data();
        auto alphaChannel = trns ? trns->data.data() : nullptr;
        for( unsigned i = 0; i < colorNumber; ++i )
        {
            Pixel pixel;
            uint8_t channel;
            for( unsigned j = 0; j < colorBytes; ++j )
            {
                copy( &channel, paletteChannel++, sizeof( channel ) );
                pixel.push_back( channel );
            }
            if( trns )
            {
                if( alphaNumber > 0 )
                {
                    --alphaNumber;
                    copy( &channel, alphaChannel++, sizeof( channel ) );
                    pixel.push_back( channel );
                }
                else
                {
                    pixel.push_back( 255 );
                }
            }
            palette->samples.push_back( pixel );
        }
        makeException( alphaNumber == 0 );

        fmt.compression.push_front( palette );

        std::optional<Pixel> p;
        fmt.compression.push_front( std::make_shared<Misc>( fmt.bufferSize(), false, false, p, fmt ) );
    }
    else
    {
        if( trns )
        {
            Pixel pixel;

            if( ihdr.colorType == PNG_GRAYSCALE )
            {
                makeException( trns->data.size() * 8 == ihdr.bitDepth );
                if( ihdr.bitDepth == 8 )
                {
                    uint8_t p;
                    copy( &p, trns->data.data(), sizeof( p ) );
                    pixel.push_back( p );
                }
                else if( ihdr.bitDepth == 16 )
                {
                    uint16_t p;
                    copy( &p, trns->data.data(), sizeof( p ) );
                    pixel.push_back( p );
                }
                else
                {
                    makeException( false );
                }
            }
            else if( ihdr.colorType == PNG_TRUECOLOR )
            {
                makeException( trns->data.size() * 8 == ihdr.bitDepth * 3 );
                if( ihdr.bitDepth == 8 )
                {
                    uint8_t p[3];
                    copy( p, trns->data.data(), sizeof( p ) );
                    for( auto &c : p )
                        pixel.push_back( c );
                }
                else if( ihdr.bitDepth == 16 )
                {
                    uint16_t p[3];
                    copy( p, trns->data.data(), sizeof( p ) );
                    for( auto &c : p )
                        pixel.push_back( c );
                }
                else
                {
                    makeException( false );
                }
            }
            else
            {
                makeException( false );
            }

            fmt.compression.push_front( std::make_shared<Misc>( fmt.bufferSize(), false, false, pixel, fmt ) );
            fmt.channels.pop_back();
            fmt.calculateBits();
        }
        else
        {
            std::optional<Pixel> p;
            fmt.compression.push_front( std::make_shared<Misc>( fmt.bufferSize(), false, false, p, fmt ) );
        }
    }

    fmt.compression.push_front( std::make_shared<FilterAndInterlacePng>( ihdr.interlaceMethod == 1, Abs( fmt.w ), Abs( fmt.h ), fmt ) );
    fmt.clear();

    fmt.compression.push_front( std::make_shared<ZlibPng>( volume, fmt ) );
    fmt.clear();

    fmt.compression.push_front( std::make_shared<FracturePng>( chunks, fmt ) );
    fmt.clear();
}

class SimpleReader : public ReaderBase
{
private:
    const uint8_t *p, *end;
public:
    SimpleReader( const void *data, unsigned bytes )
    {
        p = ( const uint8_t * )data;
        end = p + bytes;
    }

    bool read( unsigned, BitList & ) override
    {
        makeException( false );
        return false;
    }

    bool read( unsigned bytes, void *value ) override
    {
        if( p + bytes > end )
            return false;
        if( value )
            copy( value, p, bytes );
        p += bytes;
        return true;
    }
};

class SimpleWriter : public WriterBase
{
private:
    uint8_t *p;
    const uint8_t *end;
public:
    SimpleWriter( void *data, unsigned bytes )
    {
        p = ( uint8_t * )data;
        end = p + bytes;
    }

    bool write( unsigned, BitList ) override
    {
        makeException( false );
        return false;
    }

    bool write( unsigned bytes, const void *value ) override
    {
        if( p + bytes > end )
            return false;
        copy( p, value, bytes );
        p += bytes;
        return true;
    }
};

static void makePng( const Reference &ref, Format &format, HeaderWriter *write )
{
    format.w = ref.w;
    format.h = ref.h;

    if( !write )
    {
        SimpleReader r( ref.link, ref.bytes );
        extractPng( format, r );
        return;
    }

    format.offset += sizeof( PNGSignature ) + sizeof( PNGChunkHeader ) + sizeof( PNGIHDRData ) + sizeof( PNGChunk::crc );
    format.channels.push_back( { 'R', 8 } );
    format.channels.push_back( { 'G', 8 } );
    format.channels.push_back( { 'B', 8 } );
    format.channels.push_back( { 'A', 8 } );
    format.calculateBits();

    std::optional<Pixel> p;
    format.compression.push_front( std::make_shared<Misc>( format.bufferSize(), false, false, p, format ) );

    format.compression.push_front( std::make_shared<FilterAndInterlacePng>( true, Abs( format.w ), Abs( format.h ), format ) );
    format.clear();

    format.compression.push_front( std::make_shared<ZlibPng>( 0, format ) );
    format.clear();

    format.compression.push_front( std::make_shared<FracturePng>( 0, format ) );
    format.clear();

    *write = []( const Format & fmt, Reference & dst )
    {
        SimpleWriter w( dst.link, dst.bytes );

        PNGSignature ps;
        makeException( ps.write( w ) );

        PNGIHDRData ihdr;
        ihdr.width = encodeBe32( fmt.w );
        ihdr.height = encodeBe32( fmt.h );
        ihdr.bitDepth = 8;
        ihdr.colorType = PNG_TRUECOLOR_ALPHA;
        ihdr.compressionMethod = 0;
        ihdr.filterMethod = 0;
        ihdr.interlaceMethod = 1;

        PNGChunk ihdrChunk;
        ihdrChunk.meta.set( "IHDR" );
        ihdrChunk.meta.length = sizeof( ihdr );
        ihdrChunk.data.resize( sizeof( ihdr ) );
        copy( ihdrChunk.data.data(), &ihdr, sizeof( ihdr ) );
        ihdrChunk.updateCrc();

        makeException( ihdrChunk.write( w ) );
    };
}

// ---------------------------------------------------------------------------
// Format parsing
// ---------------------------------------------------------------------------

static inline size_t getWord( const std::string &string, size_t &i, const std::vector<std::string> &words )
{
    auto word = std::find_if( words.begin(), words.end(), [&]( const auto & w )
    {
        return string.substr( i, w.size() ) == w;
    } );
    makeException( word != words.end() );

    i += word->size();
    return std::distance( words.begin(), word );
}

static inline unsigned getNumber( const std::string &string, size_t &i )
{
    unsigned result = 0;
    while( i < string.size() && std::isdigit( string[i] ) )
    {
        result = result * 10 + ( string[i] - '0' );
        ++i;
    }
    return result;
}

static Format parseFormat( const Reference &ref, HeaderWriter *write, const Format *sample )
{
    const static std::vector<std::string> types
    {
        "DIB",
        "BMP",
        "PNG",
        "ANYF"
    };

    const static std::vector<std::string> settings
    {
        "PAD",
        "SAME",
        "REP"
    };

    Format format;

    makeException( ref.format.has_value() );
    auto &string = *ref.format;

    size_t typeId = 0, i = 0;
    while( i < string.size() )
    {
        char channel = string[i++];
        if( channel == '.' )
        {
            typeId = getWord( string, i, types ) + 1;
            continue;
        }

        if( channel == '*' )
        {
            auto settingId = getWord( string, i, settings );
            if( settingId == 0 )
            {
                format.pad = getNumber( string, i );
            }
            if( settingId == 1 )
            {
                if( sample )
                    return *sample;
            }
            if( settingId == 2 )
            {
                makeException( i < string.size() );
                channel = string[i++];

                Replacement replacement;
                auto id = format.id( channel );
                makeException( id );
                replacement.id = *id;

                makeException( i < string.size() );
                channel = string[i];
                if( std::isdigit( channel ) )
                {
                    replacement.constant = getNumber( string, i );
                }
                else
                {
                    replacement.channel = channel;
                    ++i;
                }

                if( sample )
                    format.replacements.emplace_back( std::move( replacement ) );
            }
            continue;
        }

        makeException( ( 'A' <= channel && channel <= 'Z' ) || channel == '_' );

        unsigned bits = getNumber( string, i );
        format.channels.push_back( { channel, bits } );
    }

    switch( typeId )
    {
    case 0:
        makeBmp( ref, false, false, format, write );
        break;
    case 1:
        format.clear();
        makeBmp( ref, false, true, format, write );
        break;
    case 2:
        format.clear();
        makeBmp( ref, true, true, format, write );
        break;
    case 3:
        format.clear();
        makePng( ref, format, write );
        break;
    case 4:
        format.clear();
        if( !write )
        {
            makeException( ref.bytes >= 16 );
            if( compare( ref.link, "BM", 2 ) )
            {
                makeBmp( ref, true, true, format, write );
            }
            else
            {
                PNGSignature ps;
                makeException( compare( ref.link, &ps, sizeof( ps ) ) );
                makePng( ref, format, write );
            }
        }
        else
        {
            makePng( ref, format, write );
        }
        break;
    default:
        makeException( false );
    }

    format.calculateBits();
    return format;
}

// ---------------------------------------------------------------------------
// Pixel access helpers
// ---------------------------------------------------------------------------

template<typename A, typename B>
static inline B toInt( A x, const Channel &c )
{
    makeException( 0 <= x && x <= 1 );
    auto max = c.max();
    return max > 0 ? B( x * max + 0.5 ) : B( 0 );
}

template<typename A, typename B>
static inline B toFloat( A x, const Channel &c )
{
    auto max = c.max();
    makeException( 0 <= x && x <= max );
    return max > 0 ? B( x ) / B( max ) : B( 0 );
}

// Converts between integer Pixel channels and normalized double Color channels in range [0, 1]
// Matches formats channels of source and destination
// For channels with 0 bits, the result is 0
// If the channel is '_' its value is ignored, when read and is written as 0
template<typename VA, typename VB>
static VB convert( const VA &src, const PixelFormat &srcFmt, const PixelFormat &dstFmt )
{
    using A = typename VA::value_type;
    using B = typename VB::value_type;

    auto atob = []( A a, const Channel & cA, const Channel & cB ) -> B
    {
        if( std::is_same_v<A, B> &&( std::is_same_v<A, double> || cA.bits == cB.bits ) )
        {
            return a;
        }

        double tmp;
        if constexpr( std::is_same_v<A, double> )
        {
            tmp = a;
        }
        else
        {
            tmp = toFloat<A, double>( a, cA );
        }

        if constexpr( std::is_same_v<B, double> )
        {
            return tmp;
        }
        else
        {
            return toInt<double, B>( tmp, cB );
        }
    };

    VB dst;
    unsigned dstId = 0;
    for( const auto &dstChannel : dstFmt.channels )
    {
        if( dstChannel.channel == '_' )
        {
            dst.push_back( B( 0 ) );
            ++dstId;
            continue;
        }

        auto srcId = srcFmt.id( dstChannel.channel );
        if( !srcId )
        {
            auto replacement = dstFmt.replace( dstId, srcFmt, srcId );
            if( !srcId )
            {
                makeException( replacement && replacement->constant );
                if constexpr( std::is_same_v<B, double> )
                    dst.push_back( toFloat<B, BitList>( *replacement->constant, dstChannel ) );
                else
                    dst.push_back( *replacement->constant );
                ++dstId;
                continue;
            }
            makeException( srcId );
        }

        auto &srcChannel = srcFmt.channels[*srcId];
        dst.push_back( atob( src[*srcId], srcChannel, dstChannel ) );
        ++dstId;
    }
    return dst;
}

static void sync( const Format &dstFmt, Reference &destination )
{
    auto bytes = dstFmt.bufferSize();
    if( destination.w != dstFmt.w || destination.h != dstFmt.h || destination.bytes < bytes )
    {
        destination.w = dstFmt.w;
        destination.h = dstFmt.h;
        destination.bytes = dstFmt.bufferSize();
        makeException( destination.reset( destination ) );
    }
    else
    {
        destination.bytes = bytes;
    }
}

class Reader : public ReaderBase
{
protected:
    BitPointer<const uint8_t> p;
    unsigned bitPosition, bitVolume;
    const uint8_t *start;

    const Format fmt;
    const Reference &ref;
public:
    Reader( const Format &f, const Reference &r ) : fmt( f ),  ref( r )
    {
        makeException( ref.bytes >= fmt.offset );

        p = start = ( const uint8_t * )ref.link + fmt.offset;
        bitVolume = ( ref.bytes - fmt.offset ) * 8;
        bitPosition = 0;
    }

    bool read( unsigned bits, BitList &value ) override
    {
        if( ( bitPosition += bits ) > bitVolume )
            return false;

        readBits( p.pointer, p.bitOffset, bits, value );
        return true;
    };

    bool read( unsigned bytes, void *value ) override
    {
        makeException( p.bitOffset == 0 );

        if( ( bitPosition += 8 * bytes ) > bitVolume )
            return false;

        copy( value, p.pointer, bytes );
        p.pointer += bytes;
        return true;
    }

    unsigned bytesLeft( unsigned limit ) const
    {
        makeException( p.bitOffset == 0 );
        makeException( bitPosition <= bitVolume );

        auto bytes = bitVolume - bitPosition;
        makeException( bytes % 8 == 0 );
        bytes /= 8;

        if( bytes > limit )
            return limit;
        return bytes;
    }
};

class PixelReader : public Reader
{
protected:
    unsigned x, y, width, height, totalLineBits, previousBitPosition, linePixelBits;
public:
    PixelReader( const Format &f, const Reference &r ) : Reader( f, r )
    {
        makeException( fmt.bits > 0 );

        width  = Abs( fmt.w );
        height = Abs( fmt.h );

        x = y = 0;
        previousBitPosition = linePixelBits = totalLineBits = 0;
    }

    void nextLine()
    {
        unsigned lineBits = bitPosition - previousBitPosition;

        if( totalLineBits <= 0 )
            totalLineBits = fmt.lineSize( lineBits - linePixelBits ) * 8;

        makeException( totalLineBits >= linePixelBits );

        auto delta = totalLineBits - lineBits;
        makeException( ( bitPosition += delta ) <= bitVolume );
        p.addBits( delta );
        linePixelBits = 0;
        x = 0;
        ++y;

        previousBitPosition = bitPosition;
    };

    bool getPixel( Pixel &pixel )
    {
        pixel.clear();
        BitList value;
        for( const auto &channel : fmt.channels )
        {
            if( !read( channel.bits, value ) )
                return false;
            pixel.push_back( value );
        }

        linePixelBits += fmt.bits;
        ++x;
        return true;
    };

    bool getPixelLn( Pixel &pixel )
    {
        if( x >= width )
            nextLine();
        return getPixel( pixel );
    };

    void set( unsigned x0, unsigned y0 )
    {
        x = x0;
        y = y0;

        if( totalLineBits <= 0 )
        {
            makeException( bitPosition == linePixelBits );
            totalLineBits = fmt.lineSize() * 8;
        }

        makeException( x < width );
        makeException( y < height );

        p = start;
        linePixelBits = x * fmt.bits;
        previousBitPosition = y * totalLineBits;
        bitPosition = previousBitPosition + linePixelBits;
        p.addBits( bitPosition );
    };

    void add( unsigned dx, unsigned dy )
    {
        set( x + dx, y + dy );
    }
};

class Writer : public WriterBase
{
protected:
    BitPointer<uint8_t> p;
    unsigned bitPosition, bitVolume;
    uint8_t *start;

    const Format fmt;
    const Reference &ref;
public:
    Writer( const Format &f, const Reference &r ) : fmt( f ),  ref( r )
    {
        makeException( ref.bytes >= fmt.offset );

        p = start = ( uint8_t * )ref.link + fmt.offset;
        bitVolume = ( ref.bytes - fmt.offset ) * 8;
        bitPosition = 0;
    }

    bool write( unsigned bits, BitList value ) override
    {
        if( ( bitPosition += bits ) > bitVolume )
            return false;

        writeBits( p.pointer, p.bitOffset, bits, value );
        return true;
    };

    bool write( unsigned bytes, const void *value ) override
    {
        makeException( p.bitOffset == 0 );

        if( ( bitPosition += 8 * bytes ) > bitVolume )
            return false;

        copy( p.pointer, value, bytes );
        p.pointer += bytes;
        return true;
    }
};

class PixelWriter : public Writer
{
protected:
    unsigned x, y, width, height, lineBits, linePixelBits;
public:
    PixelWriter( const Format &f, const Reference &r ) : Writer( f, r )
    {
        makeException( fmt.bits > 0 );

        width  = Abs( fmt.w );
        height = Abs( fmt.h );

        x = y = 0;
        linePixelBits = lineBits = 0;
    }

    void nextLine()
    {
        if( lineBits <= 0 )
            lineBits = fmt.lineSize( bitPosition - linePixelBits ) * 8;

        makeException( lineBits >= linePixelBits );

        auto delta = lineBits - linePixelBits;
        makeException( write( delta, ( BitList )0 ) );
        linePixelBits = 0;
        x = 0;
        ++y;
    };

    bool putPixel( const Pixel &pixel )
    {
        size_t i = 0;
        for( const auto &channel : fmt.channels )
        {
            write( channel.bits, pixel[i] );
            ++i;
        }

        linePixelBits += fmt.bits;
        ++x;
        return true;
    };

    bool putPixelLn( const Pixel &pixel )
    {
        if( x >= width )
            nextLine();
        return putPixel( pixel );
    };

    void set( unsigned x0, unsigned y0 )
    {
        x = x0;
        y = y0;

        if( lineBits <= 0 )
        {
            makeException( bitPosition == linePixelBits );
            lineBits = fmt.lineSize() * 8;
        }

        makeException( x < width );
        makeException( y < height );

        linePixelBits = x * fmt.bits;
        auto newBitPosition = y * lineBits + linePixelBits;

        if( newBitPosition > bitPosition )
        {
            write( newBitPosition - bitPosition, ( BitList )0 );
        }
        else
        {
            p = start;
            bitPosition = newBitPosition;
            p.addBits( bitPosition );
        }
    };

    void add( unsigned dx, unsigned dy )
    {
        set( x + dx, y + dy );
    }
};

// ---------------------------------------------------------------------------
// Compression algorithms
// ---------------------------------------------------------------------------

void Palette::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Palette::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    PixelReader sourcePixelReader( fmt, source );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( fmt, destination );

    PixelWriter destinationPixelWriter( fmt, destination );

    unsigned width = Abs( fmt.w );
    unsigned height = Abs( fmt.h );
    unsigned area = width * height;

    Pixel pixel;
    while( area > 0 )
    {
        makeException( sourcePixelReader.getPixelLn( pixel ) );
        makeException( pixel.size() == 1 );
        makeException( pixel[0] < samples.size() );
        pixel = convert<Pixel, Pixel>( samples[pixel[0]], *this, fmt );
        makeException( destinationPixelWriter.putPixelLn( pixel ) );
        --area;
    }
}

void Rle::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Rle::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( 8 % granule == 0 && fmt.bits == granule );
    makeException( fmt.compression.front().get() == this );

    PixelReader sourcePixelReader( fmt, source );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( fmt, destination );

    PixelWriter destinationPixelWriter( fmt, destination );

    BitList count, command;

    auto read = [&]( unsigned b, BitList & c )
    {
        makeException( sourcePixelReader.read( b, c ) );
    };

    auto getPixel = [&]( Pixel & pixel )
    {
        makeException( sourcePixelReader.getPixel( pixel ) );
    };

    auto putPixelLn = [&]( const Pixel & pixel )
    {
        makeException( destinationPixelWriter.putPixelLn( pixel ) );
    };

    do
    {
        read( 8, count );

        // Encoded run
        if( count > 0 )
        {
            std::vector<Pixel> pixels( 8 / granule );
            for( auto &pixel :  pixels )
                getPixel( pixel );

            size_t i = 0;
            while( count > 0 )
            {
                putPixelLn( pixels[i] );
                i = ( i + 1 ) % pixels.size();
                --count;
            }
        }
        // Command
        else
        {
            read( 8, command );

            // Literal run
            if( command > 2 )
            {
                Pixel pixel;
                count = command;
                unsigned pad = 16 * ( ( count * granule + 15 ) / 16 ) - count * granule;

                while( count > 0 )
                {
                    getPixel( pixel );
                    putPixelLn( pixel );
                    --count;
                }

                BitList padding;
                read( pad, padding );
            }

            switch( command )
            {
            case 0:
                // End of line
                destinationPixelWriter.nextLine();
                break;
            case 1:
                // End of bitmap
                break;
            case 2:
                // Delta
                {
                    BitList dx, dy;
                    read( 8, dx );
                    read( 8, dy );
                    destinationPixelWriter.add( dx, dy );
                }
                break;
            default:
                break;
            }
        }
    }
    while( command != 1 );
}

void Misc::compress( Format &fmt, const Reference &source, Reference &destination )
{
    makeException( fmt.compression.front().get() == this );

    PixelReader sourcePixelReader( fmt, source );

    auto id = fmt.id( 'A' );

    fmt.offset = 0;
    copy( fmt );
    size = fmt.bufferSize( this );

    if( fixX )
        fmt.w = -fmt.w;

    if( fixY )
        fmt.h = -fmt.h;

    sync( fmt, destination );

    PixelWriter destinationPixelWriter( fmt, destination );

    auto getPixelLn = [&]( Pixel & pixel )
    {
        sourcePixelReader.getPixelLn( pixel );
    };

    auto putPixelLn = [&]( const Pixel & pixel )
    {
        destinationPixelWriter.putPixelLn( pixel );
    };

    unsigned width = Abs( fmt.w );
    unsigned height = Abs( fmt.h );

    std::vector<std::vector<Pixel>> image( height, std::vector<Pixel>( width ) );
    for( unsigned y = 0; y < height; ++y )
    {
        for( unsigned x = 0; x < width; ++x )
        {
            auto &pixel = image[y][x];
            getPixelLn( pixel );

            if( transparent )
            {
                makeException( id );
                auto position = pixel.begin();
                position += *id;
                pixel.erase( position );
            }
        }
    }

    bool flipX = fmt.w < 0;
    bool flipY = fmt.h < 0;

    for( unsigned y = 0; y < height; ++y )
    {
        for( unsigned x = 0; x < width; ++x )
        {
            unsigned fx = flipX ? ( width - 1 - x ) : x;
            unsigned fy = flipY ? ( height - 1 - y ) : y;
            putPixelLn( image[fy][fx] );
        }
    }

    fmt.w = Abs( fmt.w );
    fmt.h = Abs( fmt.h );
}

void Misc::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    PixelReader sourcePixelReader( fmt, source );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );

    if( fixX )
        fmt.w = -fmt.w;

    if( fixY )
        fmt.h = -fmt.h;

    sync( fmt, destination );

    PixelWriter destinationPixelWriter( fmt, destination );

    auto getPixelLn = [&]( Pixel & pixel )
    {
        sourcePixelReader.getPixelLn( pixel );
    };

    auto putPixelLn = [&]( const Pixel & pixel )
    {
        destinationPixelWriter.putPixelLn( pixel );
    };

    unsigned width = Abs( fmt.w );
    unsigned height = Abs( fmt.h );

    auto id = fmt.id( 'A' );

    std::vector<std::vector<Pixel>> image( height, std::vector<Pixel>( width ) );
    for( unsigned y = 0; y < height; ++y )
    {
        for( unsigned x = 0; x < width; ++x )
        {
            auto &pixel = image[y][x];
            getPixelLn( pixel );

            if( transparent )
            {
                makeException( id );
                auto position = pixel.begin();
                position += *id;
                pixel.insert( position, pixel == *transparent ? 0 : fmt.channels[*id].max() );
            }
        }
    }

    for( unsigned y = 0; y < height; ++y )
    {
        for( unsigned x = 0; x < width; ++x )
        {
            putPixelLn( image[y][x] );
        }
    }
}

void FracturePng::compress( Format &fmt, const Reference &source, Reference &destination )
{
    makeException( fmt.compression.front().get() == this );

    Reader r( fmt, source );

    copy( fmt );
    fmt.offset = 0;
    size = fmt.bufferSize( this );
    fmt.clear();

    constexpr unsigned maxChunkSize = 64 * 1024;
    size += ( ( size + maxChunkSize - 1 ) / maxChunkSize + 1 ) * ( sizeof( PNGChunkHeader ) + sizeof( PNGChunk::crc ) );

    sync( fmt, destination );

    Writer w( fmt, destination );

    PNGChunk chunk;
    unsigned chunkSize;

    chunk.meta.set( "IDAT" );
    while( ( chunkSize = r.bytesLeft( maxChunkSize ) ) > 0 )
    {
        chunk.meta.length = chunkSize;
        chunk.data.resize( chunkSize );
        r.read( chunkSize, chunk.data.data() );
        chunk.updateCrc();
        makeException( chunk.write( w ) );
    }

    chunk.meta.set( "IEND" );
    chunk.meta.length = 0;
    chunk.data.resize( 0 );
    chunk.updateCrc();
    makeException( chunk.write( w ) );
}

void FracturePng::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader r( fmt, source );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( fmt, destination );

    Writer w( fmt, destination );

    PNGChunk chunk;
    while( chunk.read( r ) )
    {
        if( chunk.meta.is( "IDAT" ) )
        {
            makeException( w.write( chunk.meta.length, chunk.data.data() ) );
        }
        else if( chunk.meta.is( "IEND" ) )
        {
            break;
        }
    }
}

void ZlibPng::compress( Format &fmt, const Reference &source, Reference &destination )
{
    makeException( fmt.compression.front().get() == this );

    auto srcSize = source.bytes - fmt.offset;
    auto srcData = ( uint8_t * )source.link + fmt.offset;

    copy( fmt );
    fmt.offset = 0;
    fmt.clear();

    std::vector<uint8_t> outBuffer( srcSize + 1000 ); // ???

    z_stream strm = {};
    makeException( deflateInit( &strm, Z_BEST_COMPRESSION ) == Z_OK );

    strm.avail_in = srcSize;
    strm.next_in = srcData;
    strm.avail_out = outBuffer.size();
    strm.next_out = outBuffer.data();
    makeException( deflate( &strm, Z_FINISH ) == Z_STREAM_END );
    deflateEnd( &strm );

    outBuffer.resize( outBuffer.size() - strm.avail_out );
    size = outBuffer.size();
    sync( fmt, destination );

    ::copy( destination.link, outBuffer.data(), size );
}

void ZlibPng::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    z_stream strm = {};
    strm.avail_in = source.bytes - fmt.offset;
    strm.next_in = ( Bytef * )( ( const uint8_t * )source.link + fmt.offset );
    makeException( inflateInit( &strm ) == Z_OK );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( fmt, destination );

    std::vector<uint8_t> outBuffer( fmt.bufferSize() + 1000 ); // ???
    strm.avail_out = outBuffer.size();
    strm.next_out = outBuffer.data();

    auto result = inflate( &strm, Z_FINISH );
    makeException( result == Z_STREAM_END );
    inflateEnd( &strm );

    outBuffer.resize( outBuffer.size() - strm.avail_out );
    ::copy( destination.link, outBuffer.data(), destination.bytes );
}

int FilterAndInterlacePng::paethPredictor( int a, int b, int c )
{
    int p = a + b - c;
    int pa = Abs( p - a );
    int pb = Abs( p - b );
    int pc = Abs( p - c );
    if( pa <= pb && pa <= pc )
        return a;
    if( pb <= pc )
        return b;
    return c;
}

unsigned FilterAndInterlacePng::scoreCandidate( const std::vector<BitList> &candidate )
{
    unsigned score = 0;
    for( auto v : candidate )
    {
        int8_t diff = ( uint8_t )v;
        score += Abs( diff );
    }
    return score;
}

// Use empty 'previous', if it's a first line
std::vector<BitList> FilterAndInterlacePng::applyFilter(
    const std::vector<BitList> &line, const std::vector<BitList> &previous, unsigned filterType, bool apply ) const
{
    size_t width = line.size();
    std::vector<BitList> result( width );

    const auto &orig = apply ? line : result;

    auto pixelBytes = ( bits + 7 ) / 8;

    auto sub = [&apply]( BitList a, BitList b )
    {
        makeException( a <= 255 );
        makeException( b <= 255 );
        if( apply )
            b = 0x100 - b;
        return ( a + b ) & 0xff;
    };

    switch( filterType )
    {
    case PNG_NONE:
        result = line;
        break;
    case PNG_SUB:
        for( size_t i = 0; i < width; ++i )
        {
            BitList left = i >= pixelBytes ? orig[i - pixelBytes] : 0;
            result[i] = sub( line[i], left );
        }
        break;
    case PNG_UP:
        for( size_t i = 0; i < width; ++i )
        {
            BitList up = !previous.empty() ? previous[i] : 0;
            result[i] = sub( line[i], up );
        }
        break;
    case PNG_AVERAGE:
        for( size_t i = 0; i < width; ++i )
        {
            int left = i >= pixelBytes ? orig[i - pixelBytes] : 0;
            int up = !previous.empty() ? previous[i] : 0;
            auto avg = ( left + up ) / 2;
            result[i] = sub( line[i], avg );
        }
        break;
    case PNG_PAETH:
        for( size_t i = 0; i < width; ++i )
        {
            int left = i >= pixelBytes ? orig[i - pixelBytes] : 0;
            int up = !previous.empty() ? previous[i] : 0;
            int upLeft = i >= pixelBytes && !previous.empty() ? previous[i - pixelBytes] : 0;
            auto paeth = paethPredictor( left, up, upLeft );
            result[i] = sub( line[i], paeth );
        }
        break;
    default:
        makeException( false );
    }
    return result;
}

FilterAndInterlacePng::FilterAndInterlacePng( bool i, int width, int height, const PixelFormat &pfmt )
    : Compression( 0, pfmt ), interlaced( i ), w( width ), h( height )
{
    calculateSize();
}

void FilterAndInterlacePng::calculateSize()
{
    size = 0;
    if( interlaced )
    {
        // Iterate over the 7 Adam7 passes
        for( unsigned pass = 0; pass < 7; ++pass )
        {
            Step passStep( pass );
            Size passSize( passStep, w, h );

            if( passSize.empty() )
                continue;

            size += passSize.bytes( bits );
        }
    }
    else
    {
        Size passSize( w, h );
        size += passSize.bytes( bits );
    }
}

void FilterAndInterlacePng::compress( Format &fmt, const Reference &source, Reference &destination )
{
    makeException( fmt.compression.front().get() == this );

    auto fmtSrc = fmt;
    PixelReader sourcePixelReader( fmtSrc, source );
    fmtSrc.offset = 0;

    unsigned width = Abs( fmt.w );
    unsigned height = Abs( fmt.h );
    w = width;
    h = height;
    calculateSize();

    copy( fmt );
    fmt.offset = 0;
    fmt.clear();

    sync( fmtSrc, destination );

    PixelWriter destinationPixelWriter( fmtSrc, destination );
    Reader destinationReader( fmt, destination );
    Writer destinationWriter( fmt, destination );

    auto read = [&]( BitList & value )
    {
        makeException( destinationReader.read( 8, value ) );
    };

    auto write = [&]( BitList value )
    {
        makeException( destinationWriter.write( 8, value ) );
    };

    std::vector<std::vector<Pixel>> image( height, std::vector<Pixel>( width ) );
    for( unsigned y = 0; y < height; ++y )
    {
        for( unsigned x = 0; x < width; ++x )
        {
            makeException( sourcePixelReader.getPixelLn( image[y][x] ) );
        }
    }

    auto putPass = [&]( const Size & passSize, const std::function<bool( unsigned &x, unsigned &y )> &position )
    {
        auto bytes = passSize.lineBytes( bits ) - 1;
        auto number = passSize.number;

        auto padding = 8 * bytes - bits * passSize.scanline;
        for( unsigned py = 0; py < passSize.number; ++py )
        {
            makeException( destinationPixelWriter.write( 8, ( BitList )0 ) );
            for( unsigned px = 0; px < passSize.scanline; ++px )
            {
                auto x = px;
                auto y = py;
                if( position( x, y ) )
                    makeException( destinationPixelWriter.putPixel( image[y][x] ) );
            }
            makeException( destinationPixelWriter.write( padding, ( BitList )0 ) );
        }

        std::vector<BitList> previous, line( bytes );

        while( number > 0 )
        {
            BitList bestFilter;
            read( bestFilter );
            for( unsigned i = 0; i < bytes; ++i )
                read( line[i] );

            // Compute candidate filtered lines
            const unsigned numFilters = 5;
            std::vector<std::vector<BitList>> candidates( numFilters );
            std::vector<unsigned> scores( numFilters, 0 );

            for( unsigned filter = 0; filter < numFilters; ++filter )
            {
                candidates[filter] = applyFilter( line, previous, filter, true );
                scores[filter] = scoreCandidate( candidates[filter] );
            }

            // Choose the filter with the lowest score
            bestFilter = 0;
            unsigned bestScore = scores[0];
            for( unsigned f = 1; f < numFilters; ++f )
            {
                if( scores[f] < bestScore )
                {
                    bestScore = scores[f];
                    bestFilter = f;
                }
            }

            write( bestFilter );
            for( unsigned i = 0; i < bytes; ++i )
                write( candidates[bestFilter][i] );

            previous = line;
            --number;
        }
    };

    if( interlaced )
    {
        for( unsigned pass = 0; pass < 7; ++pass )
        {
            Step passStep( pass );
            Size passSize( passStep, width, height );

            if( passSize.empty() )
                continue;

            putPass( passSize, [&]( unsigned & x, unsigned & y )
            {
                x = passStep.x( x );
                y = passStep.y( y );
                return x < width && y < height;
            } );
        }
    }
    else
    {
        Size passSize( width, height );
        putPass( passSize, [&]( unsigned &, unsigned & )
        {
            return true;
        } );
    }
}

void FilterAndInterlacePng::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader sourceReader( fmt, source );

    fmt.offset = 0;

    Reference unfilterSource;
    unfilterSource.fill();
    sync( fmt, unfilterSource );
    Writer unfilterSourceWriter( fmt, unfilterSource );

    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( fmt, destination );

    PixelReader unfilterSourcePixelReader( fmt, unfilterSource );
    PixelWriter destinationPixelWriter( fmt, destination );

    auto read = [&]( BitList & value )
    {
        makeException( sourceReader.read( 8, value ) );
    };

    auto write = [&]( BitList value )
    {
        makeException( unfilterSourceWriter.write( 8, value ) );
    };

    unsigned width = Abs( fmt.w );
    unsigned height = Abs( fmt.h );

    std::vector<std::vector<Pixel>> image( height, std::vector<Pixel>( width ) );

    auto getPass = [&]( const Size & passSize, const std::function<bool( unsigned &x, unsigned &y )> &position )
    {
        auto bytes = passSize.lineBytes( bits ) - 1;
        auto number = passSize.number;

        std::vector<BitList> previous, line( bytes );

        while( number > 0 )
        {
            BitList filter;
            read( filter );
            for( unsigned i = 0; i < bytes; ++i )
                read( line[i] );

            line = applyFilter( line, previous, filter, false );

            write( 0 );
            for( unsigned i = 0; i < bytes; ++i )
                write( line[i] );

            previous = line;
            --number;
        }

        auto padding = 8 * bytes - bits * passSize.scanline;
        for( unsigned py = 0; py < passSize.number; ++py )
        {
            BitList filter, remainder;
            makeException( unfilterSourcePixelReader.read( 8, filter ) );
            for( unsigned px = 0; px < passSize.scanline; ++px )
            {
                auto x = px;
                auto y = py;
                if( position( x, y ) )
                    makeException( unfilterSourcePixelReader.getPixel( image[y][x] ) );
            }
            makeException( unfilterSourcePixelReader.read( padding, remainder ) );
        }
    };

    if( interlaced )
    {
        for( unsigned pass = 0; pass < 7; ++pass )
        {
            Step passStep( pass );
            Size passSize( passStep, width, height );

            if( passSize.empty() )
                continue;

            getPass( passSize, [&]( unsigned & x, unsigned & y )
            {
                x = passStep.x( x );
                y = passStep.y( y );
                return x < width && y < height;
            } );
        }
    }
    else
    {
        Size passSize( width, height );
        getPass( passSize, [&]( unsigned &, unsigned & )
        {
            return true;
        } );
    }

    for( unsigned y = 0; y < height; ++y )
    {
        for( unsigned x = 0; x < width; ++x )
        {
            makeException( destinationPixelWriter.putPixelLn( image[y][x] ) );
        }
    }
}

// ---------------------------------------------------------------------------
// Translation helpers
// ---------------------------------------------------------------------------

static void copyTranslate( const Format &srcFmt, const Reference &source, Format &dstFmt, Reference &destination )
{
    makeException( source.bytes >= srcFmt.offset );
    auto imageBytes = source.bytes - srcFmt.offset;
    auto bytes = imageBytes + dstFmt.offset;

    dstFmt.w = srcFmt.w;
    dstFmt.h = srcFmt.h;
    sync( dstFmt, destination );
    makeException( destination.bytes <= bytes );
    makeException( destination.bytes >= dstFmt.offset );

    if( dstFmt.offset != srcFmt.offset )
    {
        copy( ( uint8_t * )destination.link + dstFmt.offset, ( const uint8_t * )source.link + srcFmt.offset, destination.bytes - dstFmt.offset );
    }
    else
    {
        copy( destination.link, source.link, destination.bytes );
    }
}

// Performs a per–pixel conversion when the source and destination have the same dimensions
// With flipping image, if signs of dimensions change between images
// Matching channels with different bit sizes will be first normalized
static void directTranslate( const Format &srcFmt, const Reference &source, Format &dstFmt, Reference &destination, bool flip )
{
    makeException( srcFmt.compression.empty() && dstFmt.compression.empty() );

    if( srcFmt == dstFmt )
    {
        // Perfect match
        copyTranslate( srcFmt, source, dstFmt, destination );
        return;
    }

    int width  = Abs( srcFmt.w );
    int height = Abs( srcFmt.h );

    // Read the entire source image into a temporary buffer
    std::vector<Pixel> srcPixels( width * height );

    PixelReader sourcePixelReader( srcFmt, source );
    for( int y = 0; y < height; ++y )
    {
        for( int x = 0; x < width; ++x )
        {
            makeException( sourcePixelReader.getPixelLn( srcPixels[y * width + x] ) );
        }
    }

    // Determine whether we need to flip in each direction
    bool flipX = ( ( srcFmt.w < 0 ) ^ ( dstFmt.w < 0 ) );
    bool flipY = ( ( srcFmt.h < 0 ) ^ ( dstFmt.h < 0 ) );

    if( width != Abs( dstFmt.w ) || height != Abs( dstFmt.h ) || ( !flip && ( flipX || flipY ) ) )
    {
        dstFmt.w = srcFmt.w;
        dstFmt.h = srcFmt.h;
        flipX = false;
        flipY = false;
    }
    sync( dstFmt, destination );

    PixelWriter destinationPixelWriter( dstFmt, destination );
    for( int y = 0; y < height; ++y )
    {
        for( int x = 0; x < width; ++x )
        {
            int srcX = flipX ? ( width - 1 - x ) : x;
            int srcY = flipY ? ( height - 1 - y ) : y;

            auto &srcPixel = srcPixels[srcY * width + srcX];
            auto dstPixel = convert<Pixel, Pixel>( srcPixel, srcFmt, dstFmt );
            makeException( destinationPixelWriter.putPixelLn( dstPixel ) );
        }
    }
}

// Performs area–weighted scaling when the source and destination dimensions differ
// For each destination pixel, computes the overlapping source region, averages the normalized values
static void scaleTranslate( const Format &srcFmt, const Reference &source, Format &dstFmt, Reference &destination )
{
    makeException( srcFmt.compression.empty() && dstFmt.compression.empty() );

    int srcWidth  = Abs( srcFmt.w );
    int srcHeight = Abs( srcFmt.h );
    int dstWidth  = Abs( dstFmt.w );
    int dstHeight = Abs( dstFmt.h );

    if( srcWidth == dstWidth && srcHeight == dstHeight )
    {
        directTranslate( srcFmt, source, dstFmt, destination, true );
        return;
    }

    // Use absolute dimensions for scaling
    double scaleX = double( srcWidth )  / double( dstWidth );
    double scaleY = double( srcHeight ) / double( dstHeight );

    // Determine whether a flip is needed in each dimension
    bool flipX = ( ( srcFmt.w < 0 ) ^ ( dstFmt.w < 0 ) );
    bool flipY = ( ( srcFmt.h < 0 ) ^ ( dstFmt.h < 0 ) );

    // Read the entire source image into a temporary buffer
    std::vector<Color> srcColors( srcWidth * srcHeight );
    PixelReader sourcePixelReader( srcFmt, source );
    Pixel pixel;
    for( int y = 0; y < srcHeight; ++y )
    {
        for( int x = 0; x < srcWidth; ++x )
        {
            makeException( sourcePixelReader.getPixelLn( pixel ) );
            srcColors[y * srcWidth + x] = convert<Pixel, Color>( pixel, srcFmt, srcFmt );
        }
    }

    sync( dstFmt, destination );

    // For each destination pixel, compute the corresponding source region
    PixelWriter destinationPixelWriter( dstFmt, destination );
    for( int dy = 0; dy < dstHeight; ++dy )
    {
        for( int dx = 0; dx < dstWidth; ++dx )
        {
            double srcX0, srcX1, srcY0, srcY1;
            if( flipX )
            {
                srcX0 = srcWidth - ( dx + 1 ) * scaleX;
                srcX1 = srcWidth - dx * scaleX;
            }
            else
            {
                srcX0 = dx * scaleX;
                srcX1 = ( dx + 1 ) * scaleX;
            }
            if( flipY )
            {
                srcY0 = srcHeight - ( dy + 1 ) * scaleY;
                srcY1 = srcHeight - dy * scaleY;
            }
            else
            {
                srcY0 = dy * scaleY;
                srcY1 = ( dy + 1 ) * scaleY;
            }

            // Compute the overlapping source pixel indices
            int sx0 = Max( 0, int( RoundDown( srcX0 ) ) );
            int sy0 = Max( 0, int( RoundDown( srcY0 ) ) );
            int sx1 = Min( srcWidth, int( RoundUp( srcX1 ) ) );
            int sy1 = Min( srcHeight, int( RoundUp( srcY1 ) ) );

            // Accumulate weighted color channel
            Color accum( dstFmt.channels.size(), 0.0 );
            Color areaSum( dstFmt.channels.size(), 0.0 );

            for( int sy = sy0; sy < sy1; ++sy )
            {
                for( int sx = sx0; sx < sx1; ++sx )
                {
                    double overlapX = Min( srcX1, double( sx + 1 ) ) - Max( srcX0, double( sx ) );
                    double overlapY = Min( srcY1, double( sy + 1 ) ) - Max( srcY0, double( sy ) );
                    double area = overlapX * overlapY;
                    if( area <= 0 )
                        continue;

                    // Retrieve the source pixel color
                    // Convert it to the destination color space
                    Color dstColor = convert<Color, Color>( srcColors[ sy * srcWidth + sx ], srcFmt, dstFmt );
                    for( size_t i = 0; i < dstColor.size(); ++i )
                    {
                        accum[i] += dstColor[i] * area;
                        areaSum[i] += area;
                    }
                }
            }

            Color dstColor;
            for( size_t i = 0; i < accum.size(); ++i )
            {
                makeException( areaSum[i] > 0 );
                dstColor.push_back( accum[i] / areaSum[i] );
            }
            makeException( destinationPixelWriter.putPixelLn( convert<Color, Pixel>( dstColor, dstFmt, dstFmt ) ) );
        }
    }
}

// ---------------------------------------------------------------------------
// Implementation of translate
// ---------------------------------------------------------------------------

void translate( const Reference &source, Reference &destination, bool scale )
{
    makeException( source.format.has_value() && source.link && destination.reset );

    // If destination.format is not provided, default it to source.format
    if( !destination.format.has_value() )
        destination.format = source.format;

    HeaderWriter write;
    Format srcFmt = parseFormat( source, nullptr, nullptr );
    Format dstFmt = parseFormat( destination, &write, &srcFmt );

    makeException( source.bytes >= srcFmt.bufferSize() );

    if( srcFmt == dstFmt )
    {
        copyTranslate( srcFmt, source, dstFmt, destination );
        return;
    }

    Format intemidiateFmt, resultFmt;
    Reference intemidiate, result;

    auto next = [&]()
    {
        intemidiateFmt = std::move( resultFmt );
        intemidiate = std::move( result );
        result.fill();
    };

    next();
    resultFmt = srcFmt;
    resultFmt.offset = 0;
    copyTranslate( srcFmt, source, resultFmt, result );

    for( size_t i = 0; i < srcFmt.compression.size(); ++i )
    {
        next();
        auto &compression = srcFmt.compression[i];

        resultFmt = intemidiateFmt;
        compression->decompress( resultFmt, intemidiate, result );
    }

    next();
    resultFmt = dstFmt;
    resultFmt.offset = 0;

    if( !resultFmt.compression.empty() )
        resultFmt.copy( *resultFmt.compression.back() );
    resultFmt.compression.clear();

    if( scale )
        scaleTranslate( intemidiateFmt, intemidiate, resultFmt, result );
    else
        directTranslate( intemidiateFmt, intemidiate, resultFmt, result, false );

    for( size_t i = 0; i < dstFmt.compression.size(); ++i )
    {
        next();
        auto &compression = dstFmt.compression[dstFmt.compression.size() - i - 1];

        resultFmt = intemidiateFmt;
        resultFmt.compression.push_front( compression );
        compression->compress( resultFmt, intemidiate, result );
    }

    copyTranslate( resultFmt, result, dstFmt, destination );
    write( dstFmt, destination );
}
}
