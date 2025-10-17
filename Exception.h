#pragma once

#include <string>

class Exception
{
public:
    static std::wstring extract( const char *bytes );
    static std::wstring extract( int number );

    Exception( std::wstring message );
    Exception( const char *file, int line );
    ~Exception();

    const std::wstring &message() const;

    static void terminate( bool condition );
private:
    std::wstring msg;
};

#define makeException(X) do{if(!(X)){throw Exception(__FILE__,__LINE__);}}while(false)
