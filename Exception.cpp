#include "Exception.h"

#include "UnicodeString.h"
#include "Basic.h"

const wchar_t *error = L"?";

std::wstring Exception::extract( const char *bytes )
{
    String string;
    size_t pos = 0;
    std::vector<uint8_t> data;
    data.resize( stringLength( bytes ) );
    copy( data.data(), bytes, data.size() );

    if( !string.DecodeUtf8( data, pos ) )
        return error;

    std::wstring result;
    if( !string.EncodeW( result ) )
        return error;

    return result;
}

std::wstring Exception::extract( int number )
{
    String string;
    string << number;

    std::wstring result;
    if( !string.EncodeW( result ) )
        return error;

    return result;
}

Exception::Exception( std::wstring m )
{
    msg = std::move( m );
}

Exception::Exception( const char *file, int line )
{
    msg = extract( file ) + L" : " + extract( line );
}

Exception::~Exception()
{}

const std::wstring &Exception::message() const
{
    return msg;
}

void Exception::terminate( bool condition )
{
    if( !condition )
        std::terminate();
}
