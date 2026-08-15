#include "GetTime.h"

#include <windows.h>

long long unsigned getTime()
{
    long long unsigned time = 0;
    GetSystemTimeAsFileTime( ( LPFILETIME )( void * )&time );
    return time;
}
