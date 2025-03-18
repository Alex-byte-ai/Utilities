#pragma once

void copy( void *destination, const void *source, unsigned bytes );
void clear( void *destination, unsigned bytes );
void clear( void *destination, unsigned char byte, unsigned bytes );
bool compare( const void *source0, const void *source1, unsigned bytes );

unsigned stringLength( const char *string );
