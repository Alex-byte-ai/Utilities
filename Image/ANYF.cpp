#include "Image/ANYF.h"

#include "Image/PixelIO.h"

namespace ImageConvert
{
void sync( unsigned bytes, const Format &dstFmt, Reference &destination )
{
    if( destination.w != dstFmt.w || destination.h != dstFmt.h || destination.bytes < bytes )
    {
        destination.w = dstFmt.w;
        destination.h = dstFmt.h;
        destination.bytes = bytes;
        makeException( destination.reset( destination ) );
    }
    else
    {
        destination.bytes = bytes;
    }
}

void sync( const Format &dstFmt, Reference &destination )
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

Misc::Misc( unsigned s, bool x, bool y, std::optional<Pixel> t, const PixelFormat &pfmt ) : Compression( s, pfmt ),
    transparent( std::move( t ) ), fixX( x ), fixY( y )
{}

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

bool Misc::equals( const Compression &other ) const
{
    if( auto misc = dynamic_cast<const Misc *>( &other ) )
        return this->Compression::operator==( other ) &&
               fixX == misc->fixX &&
               fixY == misc->fixY &&
               transparent == misc->transparent;
    return false;
};

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

Palette::Palette( unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt )
{}

bool Palette::equals( const Compression &other ) const
{
    if( auto otherPtr = dynamic_cast<const Palette *>( &other ) )
        return samples == otherPtr->samples && this->Compression::operator==( other );
    return false;
}
}
