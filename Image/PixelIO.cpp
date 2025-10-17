#include "Image/PixelIO.h"

namespace ImageConvert
{
PixelReader::PixelReader( const Format &f, const Reference &r ) : Reader( r.link, r.bytes, f.offset ), fmt( f )
{
    makeException( fmt.bits > 0 );

    width  = Abs( fmt.w );
    height = Abs( fmt.h );

    x = y = 0;
    previousBitPosition = linePixelBits = totalLineBits = 0;
}

void PixelReader::nextLine()
{
    auto lineBits = bitPosition - previousBitPosition;

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

bool PixelReader::getPixel( Pixel &pixel )
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

bool PixelReader::getPixelLn( Pixel &pixel )
{
    if( x >= width )
        nextLine();
    return getPixel( pixel );
};

void PixelReader::set( long long unsigned x0, long long unsigned y0 )
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

void PixelReader::add( long long unsigned dx, long long unsigned dy )
{
    set( x + dx, y + dy );
}

PixelWriter::PixelWriter( const Format &f, const Reference &r ) : Writer( r.link, r.bytes, f.offset ), fmt( f )
{
    makeException( fmt.bits > 0 );

    width  = Abs( fmt.w );
    height = Abs( fmt.h );

    x = y = 0;
    linePixelBits = lineBits = 0;
}

void PixelWriter::nextLine()
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

bool PixelWriter::putPixel( const Pixel &pixel )
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

bool PixelWriter::putPixelLn( const Pixel &pixel )
{
    if( x >= width )
        nextLine();
    return putPixel( pixel );
};

void PixelWriter::set( long long unsigned x0, long long unsigned y0 )
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

void PixelWriter::add( long long unsigned dx, long long unsigned dy )
{
    set( x + dx, y + dy );
}
}
