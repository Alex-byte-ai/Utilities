#include "Image/BMP.h"

#include <algorithm>

#include "Image/PixelIO.h"
#include "Basic.h"

namespace ImageConvert
{
#define BI_RGB 0
#define BI_RLE4 2
#define BI_RLE8 1
#define BI_BITFIELDS 3

// Ensure structures are packed without padding
#pragma pack(push, 1)

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

#pragma pack(pop)

RleBmp::RleBmp( unsigned s, const PixelFormat &pfmt, unsigned g ): Compression( s, pfmt ), granule( g )
{}

void RleBmp::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void RleBmp::decompress( Format &fmt, const Reference &source, Reference &destination ) const
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

bool RleBmp::equals( const Compression &other ) const
{
    if( auto otherPtr = dynamic_cast<const RleBmp *>( &other ) )
        return granule == otherPtr->granule && this->Compression::operator==( other );
    return false;
};

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
        fmt.compression.push_front( std::make_shared<RleBmp>( h.biSizeImage, fmt, 4 ) );
        return;
    }

    if( h.biCompression == BI_RLE8 )
    {
        makeException( fmt.bits == 8 );
        extractBmp( fmt, header + 1, paletteCount(), reserved, alpha );
        fmt.compression.push_front( std::make_shared<RleBmp>( h.biSizeImage, fmt, 8 ) );
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

void makeBmp( const Reference &ref, bool fileHeader, bool bmpHeader, Format &format, HeaderWriter *write )
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
            clear( &v4, sizeof( v4 ) );
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
}
}
