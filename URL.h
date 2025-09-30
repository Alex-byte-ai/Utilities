#pragma once

#include <string>

class URL
{
public:
    [[nodiscard]] static bool Open( const std::string &url );

    URL();
    ~URL();
};
