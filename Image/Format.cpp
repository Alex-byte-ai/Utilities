#include <Image/Format.h>

#include "Exception.h"

namespace ImageConvert
{

BitList Channel::max() const
{
    return bits > 0 ? ( ( ( BitList )1 ) << bits ) - 1 : 0;
}

bool Channel::operator==( const Channel &other ) const
{
    return channel == other.channel && bits == other.bits;
}

void PixelFormat::calculateBits()
{
    bits = 0;
    for( const auto &channel : channels )
        bits += channel.bits;
}

void PixelFormat::copy( const PixelFormat &other )
{
    replacements = other.replacements;
    channels = other.channels;
    bits = other.bits;
}

void PixelFormat::clear()
{
    replacements.clear();
    channels.clear();
    bits = 0;
}

std::optional<unsigned> PixelFormat::id( char channel ) const
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

const Replacement *PixelFormat::replace( unsigned id, const PixelFormat &source, std::optional<unsigned> &srcId ) const
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

bool PixelFormat::operator==( const PixelFormat &other ) const
{
    return channels == other.channels;
}

Compression::Compression( unsigned s, const PixelFormat &pfmt )
{
    copy( pfmt );
    size = s;
}

bool Compression::equals( const Compression &other ) const
{
    return this->PixelFormat::operator==( other );
}

Compression::~Compression()
{}

unsigned Format::lineSize( unsigned dbits ) const
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

unsigned Format::bufferSize( const Compression *peelLayer ) const
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

bool Format::operator==( const Format &other )const
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
}
