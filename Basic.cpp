#include "Basic.h"

#include <shlwapi.h>
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

bool compare( const wchar_t *string0, const wchar_t *string1 )
{
    return StrCmpLogicalW( string0, string1 ) < 0;
}

unsigned stringLength( const char *string )
{
    return std::strlen( string );
}
