#pragma once

#include <vector>
#include <memory>
#include <deque>

#include "Image/Reference.h"

#include "Basic.h"
#include "Bits.h"

namespace ImageConvert
{
// Holds one channel ('A' – 'Z', '_' for unused) and its bit–width.
struct Channel
{
    char channel;
    unsigned bits;

    // Maximum value, that can be stored in this channel
    BitList max() const;

    bool operator==( const Channel &other ) const;
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

    // Name of alpha channel
    char alpha = 'A';

    // Calculates total number of bits of all channels
    void calculateBits();

    void copy( const PixelFormat &other );

    void clear();

    std::optional<unsigned> id( char channel ) const;

    const Replacement *replace( unsigned id, const PixelFormat &source, std::optional<unsigned> &srcId ) const;

    bool operator==( const PixelFormat &other ) const;
};

struct Format;

struct Compression : public PixelFormat
{
    // Size of the compressed data
    unsigned size;

    Compression( unsigned s, const PixelFormat &pfmt );

    virtual void compress( Format &fmt, const Reference &source, Reference &destination ) = 0;
    virtual void decompress( Format &fmt, const Reference &source, Reference &destination ) const = 0;

    virtual bool equals( const Compression &other ) const;

    virtual ~Compression();
};

struct Format : public PixelFormat
{
    std::deque<std::shared_ptr<Compression>> compression;

    // Bytes of meta data before image
    unsigned offset = 0;

    // Number of bytes in line should be divisible by this
    // If it's 0 padding is not used
    unsigned pad = 0;

    // Dimensions
    int w = 0, h = 0;

    // Computes the number of bytes needed for a line
    unsigned lineSize( unsigned dbits = 0 ) const;

    // Computes the number of bytes needed for the entire image
    unsigned bufferSize( const Compression *peelLayer = nullptr ) const;

    bool operator==( const Format &other )const;
};
}
