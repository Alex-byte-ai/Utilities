#pragma once

#include <cstdint>

#include "Image/Format.h"
#include "Image/ANYF.h"

#include "BitIO.h"

// https://en.wikipedia.org/wiki/BMP_file_format

namespace ImageConvert
{
struct RleBmp : public Compression
{
    unsigned granule = 0;

    RleBmp( unsigned s, const PixelFormat &pfmt, unsigned g );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

void makeBmp( const Reference &ref, bool fileHeader, bool bmpHeader, Format &format, HeaderWriter *write );
}
