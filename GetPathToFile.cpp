#include "GetPathToFile.h"

#include <windows.h>

#include "Lambda.h"

static std::optional<std::filesystem::path> GetPathToFile( bool open )
{
    Finalizer _;

    wchar_t path[1024], current[1024];
    OPENFILENAMEW ofn;

    memset( path, 0, sizeof( path ) );
    memset( current, 0, sizeof( current ) );
    memset( &ofn, 0, sizeof( ofn ) );

    GetCurrentDirectoryW( sizeof( current ), current );

    std::wstring currentPath = current;
    _.push( [currentPath]()
    {
        SetCurrentDirectoryW( currentPath.c_str() );
    } );

    ofn.lStructSize = sizeof( ofn );
    ofn.lpstrFile = path;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof( path );
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if( open )
    {
        if( GetOpenFileNameW( &ofn ) )
            return path;
    }
    else
    {
        if( GetSaveFileNameW( &ofn ) )
            return path;
    }

    return {};
}

std::optional<std::filesystem::path> SavePath()
{
    return GetPathToFile( false );
}

std::optional<std::filesystem::path> OpenPath()
{
    return GetPathToFile( true );
}
