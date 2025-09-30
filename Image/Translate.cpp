#include "Image/Translate.h"

#include <algorithm>
#include <vector>

#include "Image/Reference.h"
#include "Image/PixelIO.h"
#include "Image/Format.h"

#include "Image/BMP.h"
#include "Image/PNG.h"
#include "Image/JPG.h"

#include "Exception.h"
#include "Basic.h"

namespace ImageConvert
{
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
        "JPG",
        "ANYF"
    };

    const static std::vector<std::string> settings
    {
        "PAD",
        "SAME",
        "REP",
        "ALPHA"
    };

    Format format;

    makeException( ref.format.has_value() );
    auto &string = *ref.format;

    auto check = []( char c )
    {
        makeException( ( 'A' <= c && c <= 'Z' ) || c == '_' );
    };

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
            if( settingId == 3 )
            {
                makeException( i < string.size() );
                channel = string[i++];
                check( channel );
                format.alpha = channel;
            }
            continue;
        }

        check( channel );

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
        makeJpg( ref, format, write );
        break;
    case 5:
        format.clear();
        if( !write )
        {
            uint8_t jpgMarker[2] = {0xFF, 0xD8};
            char bmpMarker[2] = {'B', 'M'};

            makeException( ref.bytes >= 16 );

            if( compare( ref.link, jpgMarker, sizeof( jpgMarker ) ) )
            {
                makeJpg( ref, format, write );
            }
            else if( compare( ref.link, bmpMarker, sizeof( bmpMarker ) ) )
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

    auto alphaId = dstFmt.id( 'A' );

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
                        auto alpha = alphaId && alphaId != i ? dstColor[*alphaId] : 1;
                        alpha *= area;
                        accum[i] += dstColor[i] * alpha;
                        areaSum[i] += alpha;
                    }
                }
            }

            int abc = 0;

            Color dstColor;
            for( size_t i = 0; i < accum.size(); ++i )
            {
                makeException( areaSum[i] > 0 || ( 0 <= accum[i] && accum[i] <= areaSum[i] && areaSum[i] <= 0 ) );
                dstColor.push_back( accum[i] / ( areaSum[i] > 0 ? areaSum[i] : 1 ) );

                if( areaSum[i] > 0 )
                {
                    ++abc;
                }
            }
            makeException( destinationPixelWriter.putPixelLn( convert<Color, Pixel>( dstColor, dstFmt, dstFmt ) ) );
        }
    }
}

// ---------------------------------------------------------------------------
// Translate
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
