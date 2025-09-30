#pragma once

#include "Image/Reference.h"
#include "Image/Format.h"
#include "Image/Data.h"

#include "BitIO.h"

namespace ImageConvert
{
class PixelReader : public Reader
{
protected:
    unsigned x, y, width, height, totalLineBits, previousBitPosition, linePixelBits;
    const Format fmt;
public:
    PixelReader( const Format &f, const Reference &r );

    void nextLine();

    bool getPixel( Pixel &pixel );
    bool getPixelLn( Pixel &pixel );

    void set( unsigned x0, unsigned y0 );
    void add( unsigned dx, unsigned dy );
};

class PixelWriter : public Writer
{
protected:
    unsigned x, y, width, height, lineBits, linePixelBits;
    const Format fmt;
public:
    PixelWriter( const Format &f, const Reference &r );

    void nextLine();
    bool putPixel( const Pixel &pixel );

    bool putPixelLn( const Pixel &pixel );

    void set( unsigned x0, unsigned y0 );
    void add( unsigned dx, unsigned dy );
};
}
