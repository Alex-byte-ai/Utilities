#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "UnicodeString.h"
#include "Exception.h"
#include "Connect.h"

class Console
{
public:
    class Data;
    class ServerInformation;
private:
    ServerInformation *serverInfo;
    Unicode::String buffer;
    Connect connect;
    Data *data;
public:
    struct Color
    {
        float r, g, b, a;

        Color();
        Color( float red, float green, float blue, float alpha = 1.0f );
        uint32_t get() const;
    };

    Console();
    ~Console();

    bool run();

    void msg( const std::wstring &message );

    // Entered text will be shifted after calling these:
    void operator++(); // Shift right
    void operator--(); // Shift left

    void color( const std::optional<Color> &c );

    void configure( const std::optional<std::filesystem::path> &configFile = {} );
    void save( const std::optional<std::filesystem::path> &path = {} );
    void command( const std::wstring &cmd );

    void clear();

    bool focused();
    bool running();

    void numericBase( short int value );
    short int numericBase();
    void showBase( std::optional<short int> value );
    std::optional<short int> showBase();

    template<typename T>
    inline Console &operator<<( const T &sample )
    {
        buffer.Clear();
        buffer << sample;
        std::wstring result;
        makeException( buffer.EncodeW( result ) );
        msg( result );
        return *this;
    }
private:
    void sysMsg( const std::wstring &message );
};
