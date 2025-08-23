#pragma once

#include <cstring>

inline void copy( void *destination, const void *source, unsigned bytes )
{
    if( bytes > 0 )
        std::memcpy( destination, source, bytes );
}

inline void move( void *destination, const void *source, unsigned bytes )
{
    if( bytes > 0 )
        std::memmove( destination, source, bytes );
}

inline void clear( void *destination, unsigned bytes )
{
    if( bytes > 0 )
        std::memset( destination, 0, bytes );
}

inline void clear( void *destination, unsigned char sample, unsigned bytes )
{
    if( bytes > 0 )
        std::memset( destination, sample, bytes );
}

inline bool compare( const void *source0, const void *source1, unsigned bytes )
{
    if( bytes <= 0 )
        return true;

    return std::memcmp( source0, source1, bytes ) == 0;
}

inline unsigned stringLength( const char *string )
{
    return std::strlen( string );
}
