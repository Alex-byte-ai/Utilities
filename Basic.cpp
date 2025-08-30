#include "Basic.h"

#include <cstring>

void copy( void *destination, const void *source, unsigned bytes )
{
    if( bytes > 0 )
        std::memcpy( destination, source, bytes );
}

void move( void *destination, const void *source, unsigned bytes )
{
    if( bytes > 0 )
        std::memmove( destination, source, bytes );
}

void clear( void *destination, unsigned bytes )
{
    if( bytes > 0 )
        std::memset( destination, 0, bytes );
}

void clear( void *destination, unsigned char sample, unsigned bytes )
{
    if( bytes > 0 )
        std::memset( destination, sample, bytes );
}

bool compare( const void *source0, const void *source1, unsigned bytes )
{
    if( bytes <= 0 )
        return true;

    return std::memcmp( source0, source1, bytes ) == 0;
}

unsigned stringLength( const char *string )
{
    return std::strlen( string );
}
