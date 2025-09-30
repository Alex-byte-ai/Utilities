#include "Image/Reference.h"

#include <cstdint>

#include "Basic.h"

namespace ImageConvert
{
Reference::Reference()
{
    link = nullptr;
    bytes = 0;
    w = h = 0;
}

Reference::Reference( Reference &&other ) noexcept
{
    if( clear )
        clear( *this );

    reset = std::move( other.reset );
    clear = std::move( other.clear );
    format = std::move( other.format );
    bytes = other.bytes;
    link = other.link;
    w = other.w;
    h = other.h;

    other.reset = nullptr;
    other.clear = nullptr;
    other.format.reset();
    other.link = nullptr;
    other.bytes = 0;
    other.w = other.h = 0;
}

Reference::~Reference()
{
    if( clear )
        clear( *this );
}

Reference &Reference::operator=( Reference &&other ) noexcept
{
    if( this == &other )
        return *this;

    if( clear )
        clear( *this );

    reset = std::move( other.reset );
    clear = std::move( other.clear );
    format = std::move( other.format );
    bytes = other.bytes;
    link = other.link;
    w = other.w;
    h = other.h;

    other.reset = nullptr;
    other.clear = nullptr;
    other.format.reset();
    other.link = nullptr;
    other.bytes = 0;
    other.w = other.h = 0;

    return *this;
}

bool Reference::operator==( const Reference &other ) const
{
    return
        w == other.w && h == other.h &&
        format == other.format &&
        bytes == other.bytes &&
        ( ( !link && !other.link ) || compare( link, other.link, bytes ) );
}

// Creates reference to purely internal data, which starts empty
void Reference::fill()
{
    if( clear )
        clear( *this );

    format.reset();
    link = nullptr;
    bytes = 0;
    w = h = 0;

    reset = []( Reference & ref )
    {
        delete[]( uint8_t * )ref.link;
        if( ref.bytes > 0 )
        {
            ref.link = new uint8_t[ref.bytes];
        }
        else
        {
            ref.link = nullptr;
        }
        return true;
    };

    clear = []( Reference & ref )
    {
        delete[]( uint8_t * )ref.link;
    };
}
}
