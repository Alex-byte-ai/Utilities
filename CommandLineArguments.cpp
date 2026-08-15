#include "CommandLineArguments.h"

#include <windows.h>

std::vector<std::wstring> commandLineArguments()
{
    std::vector<std::wstring> result;

    int n = 0;
    auto args = CommandLineToArgvW( GetCommandLineW(), &n );
    if( args )
    {
        for( int i = 0; i < n; ++i )
            result.emplace_back( args[i] );
        LocalFree( args );
    }

    return result;
}
