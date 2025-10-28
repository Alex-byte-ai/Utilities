#pragma once

#include "Exception.h"
#include "Basic.h"
#include "Bits.h"

class ReaderBase
{
public:
    virtual bool read( long long unsigned bits, BitList &value ) = 0;
    virtual bool read( long long unsigned bytes, void *value ) = 0;
    virtual ~ReaderBase()
    {}
};

class WriterBase
{
public:
    virtual bool write( long long unsigned bits, BitList value ) = 0;
    virtual bool write( long long unsigned bytes, const void *value ) = 0;
    virtual ~WriterBase()
    {}
};

class SimpleReader : public ReaderBase
{
private:
    const uint8_t *p, *end;
public:
    SimpleReader( const void *data, long long unsigned bytes )
    {
        p = ( const uint8_t * )data;
        end = p + bytes;
    }

    bool read( long long unsigned, BitList & ) override
    {
        makeException( false );
        return false;
    }

    bool read( long long unsigned bytes, void *value ) override
    {
        if( p + bytes > end )
            return false;
        if( value )
            copy( value, p, bytes );
        p += bytes;
        return true;
    }
};

class SimpleWriter : public WriterBase
{
private:
    uint8_t *p;
    const uint8_t *end;
public:
    SimpleWriter( void *data, long long unsigned bytes )
    {
        p = ( uint8_t * )data;
        end = p + bytes;
    }

    bool write( long long unsigned, BitList ) override
    {
        makeException( false );
        return false;
    }

    bool write( long long unsigned bytes, const void *value ) override
    {
        if( p + bytes > end )
            return false;
        copy( p, value, bytes );
        p += bytes;
        return true;
    }
};

template<typename T>
struct BitPointer
{
    T *pointer = nullptr;
    long long unsigned bitOffset = 0;

    BitPointer &operator=( T *p )
    {
        pointer = p;
        bitOffset = 0;
        return *this;
    }

    operator const BitPointer<const T>()
    {
        BitPointer<const T> result;
        result.pointer = pointer;
        result.bitOffset = bitOffset;
        return result;
    }

    void addBits( long long unsigned delta )
    {
        bitOffset += delta;
        auto blockBits = 8 * sizeof( T );
        pointer += bitOffset / blockBits;
        bitOffset = bitOffset % blockBits;
    }
};

class Reader : public ReaderBase
{
protected:
    BitPointer<const uint8_t> p;
    long long unsigned bitPosition, bitVolume;
    const uint8_t *start;
public:
    Reader( const void *link, long long unsigned bytes, long long unsigned offset )
    {
        makeException( bytes >= offset );

        p = start = ( const uint8_t * )link + offset;
        bitVolume = ( bytes - offset ) * 8;
        bitPosition = 0;
    }

    bool read( long long unsigned bits, BitList &value ) override
    {
        if( ( bitPosition += bits ) > bitVolume )
            return false;

        readBits( p.pointer, p.bitOffset, bits, value );
        return true;
    }

    bool read( long long unsigned bytes, void *value ) override
    {
        makeException( p.bitOffset == 0 );

        if( ( bitPosition += 8 * bytes ) > bitVolume )
            return false;

        copy( value, p.pointer, bytes );
        p.pointer += bytes;
        return true;
    }

    unsigned bytesLeft( long long unsigned limit ) const
    {
        makeException( p.bitOffset == 0 );
        makeException( bitPosition <= bitVolume );

        auto bytes = bitVolume - bitPosition;
        makeException( bytes % 8 == 0 );
        bytes /= 8;

        if( bytes > limit )
            return limit;
        return bytes;
    }
};

class Writer : public WriterBase
{
protected:
    BitPointer<uint8_t> p;
    long long unsigned bitPosition, bitVolume;
    uint8_t *start;
public:
    Writer( void *link, long long unsigned bytes, long long unsigned offset )
    {
        makeException( bytes >= offset );

        p = start = ( uint8_t * )link + offset;
        bitVolume = ( bytes - offset ) * 8;
        bitPosition = 0;
    }

    bool write( long long unsigned bits, BitList value ) override
    {
        if( ( bitPosition += bits ) > bitVolume )
            return false;

        writeBits( p.pointer, p.bitOffset, bits, value );
        return true;
    }

    bool write( long long unsigned bytes, const void *value ) override
    {
        makeException( p.bitOffset == 0 );

        if( ( bitPosition += 8 * bytes ) > bitVolume )
            return false;

        copy( p.pointer, value, bytes );
        p.pointer += bytes;
        return true;
    }
};

inline uint16_t swapBe16( uint16_t v ) noexcept
{
    return uint16_t( ( v << 8 ) | ( v >> 8 ) );
}

inline uint32_t swapBe32( uint32_t v ) noexcept
{
    return ( ( v & 0x000000FFU ) << 24 ) |
           ( ( v & 0x0000FF00U ) << 8 )  |
           ( ( v & 0x00FF0000U ) >> 8 )  |
           ( ( v & 0xFF000000U ) >> 24 );
}
