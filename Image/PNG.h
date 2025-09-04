#pragma once

#include <cstdint>

#include "Image/Format.h"
#include "Image/ANYF.h"

#include "BitIO.h"

// https://www.w3.org/TR/PNG-Filters.html

namespace ImageConvert
{
// (File) chunks <-> zlib <-> ( filter >-< interlace ) <-> misc <-> misc <-> palette (Pixels)

// Ensure structures are packed without padding
#pragma pack(push, 1)
struct PNGSignature
{
    uint8_t signature[8] {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    bool read( ReaderBase &r );
    bool write( WriterBase &w ) const;
};
#pragma pack(pop)

struct FracturePng : public Compression
{
    FracturePng( unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct ZlibPng : public Compression
{
    ZlibPng( unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
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

        Step( unsigned pass );

        unsigned x( unsigned origX ) const;
        unsigned y( unsigned origY ) const;
    };

    struct Size
    {
        unsigned number, scanline;

        Size( unsigned w, unsigned h );
        Size( const Step &step, unsigned w, unsigned h );

        unsigned lineBytes( unsigned bits ) const;
        unsigned bytes( unsigned bits ) const;
        bool empty() const;
    };

    bool interlaced;
    int w, h;

    static int paethPredictor( int a, int b, int c );
    static unsigned scoreCandidate( const std::vector<BitList> &candidate );
    std::vector<BitList> applyFilter( const std::vector<BitList> &line, const std::vector<BitList> &previous, unsigned filterType, bool apply ) const;

    FilterAndInterlacePng( bool interlaced, int w, int h, const PixelFormat &pfmt );
    FilterAndInterlacePng( const FilterAndInterlacePng &other );

    void calculateSize();

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

void makePng( const Reference &ref, Format &format, HeaderWriter *write );
}
