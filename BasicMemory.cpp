#include "BasicMemory.h"

#include <string.h>

void copy( void *destination, const void *source, unsigned bytes )
{
    if( bytes > 0 )
        memcpy( destination, source, bytes );
}

void clear( void *destination, unsigned bytes )
{
    if( bytes > 0 )
        memset( destination, 0, bytes );
}

void clear( void *destination, unsigned char byte, unsigned bytes )
{
    if( bytes > 0 )
        memset( destination, byte, bytes );
}

bool compare( const void *source0, const void *source1, unsigned bytes )
{
    if( bytes <= 0 )
        return true;

    return memcmp( source0, source1, bytes ) == 0;
}

unsigned stringLength( const char *string )
{
    return strlen( string );
}
