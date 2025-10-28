#include "URL.h"

#include <cstdlib>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/regex.hpp>
#pragma GCC diagnostic pop

#include "Exception.h"

static std::string sanitize( const std::string &url )
{
    std::string sanitizedUrl;
    const std::string specialChars = "&|<>^\"";
    for( char ch : url )
    {
        if( specialChars.find( ch ) >= specialChars.size() )
            sanitizedUrl += ch;
    }
    return sanitizedUrl;
}

static bool isValid( const std::string &url )
{
    bool match = false;
    try
    {
        const boost::regex urlRegex( R"((https?|ftp)://[^\s/$.?#].[^\s]*)" );
        match = boost::regex_match( url, urlRegex );
    }
    catch( ... )
    {
        match = false;
    }
    return match;
}

bool URL::Open( const std::string &url )
{
    auto sanitized = sanitize( url );
    if( !isValid( sanitized ) )
        return false;

    auto command = "start " + sanitized;
    return std::system( command.c_str() ) == 0;
}

URL::URL()
{}

URL::~URL()
{}
