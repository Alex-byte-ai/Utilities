#pragma once

#include <functional>
#include <optional>
#include <vector>
#include <memory>

#include "Image/Reference.h"
#include "Image/Format.h"
#include "Image/Data.h"

namespace ImageConvert
{
using HeaderWriter = std::function<void( const Format &, Reference & )>;

void sync( unsigned bytes, const Format &dstFmt, Reference &destination );
void sync( const Format &dstFmt, Reference &destination );

struct Misc : public Compression
{
    // Those pixels will be considered transparent
    std::optional<Pixel> transparent;

    bool fixX, fixY;

    Misc( unsigned s, bool x, bool y, std::optional<Pixel> t, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};

struct Palette : public Compression
{
    std::vector<Pixel> samples;

    Palette( unsigned s, const PixelFormat &pfmt );

    void compress( Format &fmt, const Reference &source, Reference &destination ) override;
    void decompress( Format &fmt, const Reference &source, Reference &destination ) const override;

    bool equals( const Compression &other ) const override;
};
}
