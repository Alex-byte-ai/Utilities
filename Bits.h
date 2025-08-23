#pragma once

#include <cstdint>

using BitList = uint64_t;

// The caller must provide enough space for these, don't forget about bit offset

inline void readBits( const uint8_t *&pointer, unsigned &bitOffset, unsigned bits, BitList &value )
{
    BitList byte;

    {
        unsigned compliment = 8 - bitOffset;
        BitList mask = ( 1u << compliment ) - 1;
        unsigned compensation = 0;
        if( bits < compliment )
        {
            compensation = compliment - bits;
            bits = compliment;
            mask >>= compensation;
            mask <<= compensation;
        }

        bits += bitOffset;
        bitOffset = 0;

        bits -= 8;
        byte = *pointer & mask;
        value = byte >> compensation;

        if( compensation > 0 )
        {
            bitOffset = 8 - compensation;
        }
        else
        {
            ++pointer;
        }
    }

    while( 8 <= bits )
    {
        byte = *pointer;

        value <<= 8;
        value = value | byte;

        bits -= 8;
        ++pointer;
    }

    if( bits > 0 )
    {
        byte = *pointer;
        byte >>= 8 - bits;

        value <<= bits;
        value = value | byte;

        bitOffset = bits;
    }
}

inline void writeBits( uint8_t *&pointer, unsigned &bitOffset, unsigned bits, BitList value )
{
    BitList byte;

    {
        unsigned compliment = 8 - bitOffset;
        BitList mask = ( 1u << compliment ) - 1;
        unsigned compensation = 0;
        if( bits < compliment )
        {
            compensation = compliment - bits;
            value <<= compensation;
            bits = compliment;
            mask >>= compensation;
            mask <<= compensation;
        }

        bits += bitOffset;
        bitOffset = 0;

        bits -= 8;

        if( bits < sizeof( value ) * 8 )
        {
            BitList suffix = value >> bits;
            byte = ( *pointer & ~mask ) | suffix;
            value = value - ( suffix << bits );
        }
        else
        {
            byte = *pointer & ~mask;
        }

        *pointer = byte;

        if( compensation > 0 )
        {
            bitOffset = 8 - compensation;
        }
        else
        {
            ++pointer;
        }
    }

    while( 8 <= bits )
    {
        bits -= 8;
        if( bits < sizeof( value ) * 8 )
        {
            byte = value >> bits;
            *pointer = ( uint8_t )byte;
            value = value - ( byte << bits );
        }
        else
        {
            byte = 0;
            *pointer = 0;
        }
        ++pointer;
    }

    if( bits > 0 )
    {
        unsigned compliment = 8 - bits;
        BitList mask = ( 1u << compliment ) - 1;

        byte = value << compliment;
        *pointer = ( uint8_t )( ( *pointer & mask ) | byte );

        bitOffset = bits;
    }
}
