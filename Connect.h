#pragma once

#include <vector>
#include <string>
#include <cstdint>

class Connect
{
private:
    class Data;
    Data *data;
public:
    class Message
    {
    public:
        virtual bool input( const std::vector<uint8_t> &data ) = 0;
        virtual bool output( std::vector<uint8_t> &data ) const = 0;
        virtual ~Message() {}
    };

    Connect( const std::wstring &id, size_t bufferSize = 32 * 1048576 );
    ~Connect();

    Connect( const Connect & ) = delete;
    Connect &operator=( const Connect & ) = delete;

    bool input( Message &message ) const;
    bool output( const Message &message ) const;

    bool isServer() const;
};
