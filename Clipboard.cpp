#include "Clipboard.h"

#include <windows.h>

#include "Exception.h"
#include "Lambda.h"
#include "Basic.h"

namespace Clipboard
{
// If failures of some WinApi functions don't affect return value of these methods
// Their failures are not checked, so code is simplified and some actions, that need to be done on exit are done any way
// without long nested repeated 'if' statements.
static bool Check()
{
    return GetLastError() == NO_ERROR;
}

bool output( const Item &item )
{
    Finalizer _;

    if( !OpenClipboard( nullptr ) )
        return false;

    _.push( []()
    {
        CloseClipboard();
    } );

    UINT type;
    SIZE_T size;
    const void *data = nullptr;
    std::visit( [&type, &size, &data]( const auto & option )
    {
        using T = std::decay_t<decltype( option )>;

        if constexpr( std::is_same_v<T, Text> )
        {
            size = ( option.size() + 1 ) * sizeof( wchar_t );
            data = option.c_str();
            type = CF_UNICODETEXT;
        }
        else if constexpr( std::is_same_v<T, Image> )
        {
            size = option.bytes;
            data = option.link;
            type = CF_DIB;
        }
    }, item );

    if( !data )
        return EmptyClipboard();

    HGLOBAL hGlobal = GlobalAlloc( GMEM_MOVEABLE, size );
    if( !hGlobal )
        return false;

    void *clipboardData = GlobalLock( hGlobal );
    if( !clipboardData )
        return false;

    copy( clipboardData, data, size );

    GlobalUnlock( hGlobal );

    if( !Check() )
        return false;

    if( !EmptyClipboard() || !SetClipboardData( type, hGlobal ) )
        return false;

    return Check();
}

bool input( Item &item )
{
    Finalizer _;

    if( !OpenClipboard( nullptr ) )
        return false;

    _.push( []()
    {
        CloseClipboard();
    } );

    UINT type = 0;

    if( IsClipboardFormatAvailable( CF_UNICODETEXT ) )
    {
        type = CF_UNICODETEXT;
    }
    else if( IsClipboardFormatAvailable( CF_DIB ) )
    {
        type = CF_DIB;
    }

    if( type == 0 )
    {
        item = {};
        return true;
    }

    SIZE_T size;
    Item temporary;
    void *data = nullptr;

    HANDLE hData = GetClipboardData( type );
    if( !hData )
        return false;

    size = GlobalSize( hData );

    if( type == CF_UNICODETEXT )
    {
        if( size % sizeof( wchar_t ) != 0 || size < sizeof( wchar_t ) )
            return false;

        temporary = std::wstring( size / sizeof( wchar_t ) - 1, L'0' );
        data = std::get<Text>( temporary ).data();
    }
    else if( type == CF_DIB )
    {
        ImageConvert::Reference image;
        image.link = new uint8_t[size];
        image.clear = []( ImageConvert::Reference & ref )
        {
            delete[]( uint8_t * )ref.link;
        };
        image.format = ".DIB";
        image.bytes = size;

        temporary = std::move( image );
        data = std::get<Image>( temporary ).link;
    }

    void *clipboardData = GlobalLock( hData );
    if( !clipboardData )
        return false;

    copy( data, clipboardData, size );

    GlobalUnlock( hData );
    if( !Check() )
        return false;

    _.pop();
    if( !CloseClipboard() )
        return false;

    item = std::move( temporary );
    return true;
}

bool isEmpty()
{
    Finalizer _;

    if( !OpenClipboard( nullptr ) )
        return false;

    _.push( []()
    {
        CloseClipboard();
    } );

    return CountClipboardFormats() == 0;
}
}
