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

void swap( void* destination, void* source, unsigned bytes )
{
    if( destination == source )
        return;

    char buffer[1024];

    while( bytes > 0 )
    {
        unsigned chunk = ( bytes < sizeof( buffer ) ) ? bytes : sizeof( buffer );

        std::memcpy( buffer, destination, chunk );
        std::memcpy( destination, source, chunk );
        std::memcpy( source, buffer, chunk );

        destination = ( char* )destination + chunk;
        source = ( char* )source + chunk;
        bytes -= chunk;
    }
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
