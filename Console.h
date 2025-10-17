#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "Connect.h"

class Console
{
public:
    class Data;
    class ServerInformation;
private:
    Data *data;
    ServerInformation *serverInfo;
    Connect connect;
public:
    struct Color
    {
        float r, g, b;
        Color() : r( 0 ), g( 0 ), b( 0 ) {};
        Color( float red, float green, float blue ) : r( red ), g( green ), b( blue ) {};
    };

    Console();
    ~Console();

    bool run();

    void operator()( const std::wstring &msg );

    // Entered lines will all be shifted after these procedures
    void operator++(); // Shift right
    void operator--(); // Shift left

    void color( const std::optional<Color> &c );

    bool configure( const std::optional<std::filesystem::path> &configFile = {} );
    bool save( const std::optional<std::filesystem::path> &path = {} );
    bool command( const std::wstring &cmd );

    void flush();
    void clear();

    bool focused();
    bool running();
};
