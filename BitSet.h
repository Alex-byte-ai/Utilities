#pragma once

#include <type_traits>
#include <stdexcept>
#include <numeric>
#include <vector>
#include <string>

#include "Exception.h"
#include "Basic.h"

template<typename Block>
class BitsetTemplate
{
    static_assert( std::is_unsigned_v<Block> &&std::is_integral_v<Block>, "'Block' must be an unsigned integer." );
public:
    BitsetTemplate() : periphery( false )
    {}

    BitsetTemplate( bool p, size_t numBits = 0 )
        : core( numBlocks( numBits ), p ? ~Block( 0 ) : Block( 0 ) ),
          periphery( p )
    {}

    BitsetTemplate( const BitsetTemplate &other )
        : core( other.core ), periphery( other.periphery )
    {}

    template<typename OtherBlock>
    BitsetTemplate( const BitsetTemplate<OtherBlock> &other )
    {
        copy( other.core.data(), other.core.size() * sizeof( OtherBlock ), other.periphery );
    }

    BitsetTemplate( const std::string &binary, bool p = false, char zero = '0', char one = '1' )
        : BitsetTemplate( p, binary.length() )
    {
        size_t bitIndex = 0;
        for( auto it = binary.rbegin(); it != binary.rend(); ++it )
        {
            if( *it == one )
            {
                set( bitIndex );
            }
            else if( *it == zero )
            {
                reset( bitIndex );
            }
            else
            {
                // String contains characters other than 'zero' or 'one'
                makeException( false );
            }

            ++bitIndex;
        }
    }

    BitsetTemplate( const char *binary, bool p = false, char zero = '0', char one = '1' )
        : BitsetTemplate( std::string( binary ), p, zero, one )
    {}

    template<typename B>
    BitsetTemplate( const std::vector<B> &value, bool p = false ) : periphery( p )
    {
        static_assert( !std::is_class<B>::value && !std::is_union<B>::value && !std::is_pointer<B>::value );
        copy( value.data(), value.size() * sizeof( B ), p );
    }

    template<typename B>
    BitsetTemplate( const B &value, bool p = false ) : periphery( p )
    {
        static_assert( !std::is_class<B>::value && !std::is_union<B>::value && !std::is_pointer<B>::value );
        copy( &value, sizeof( value ), p );
    }

    virtual ~BitsetTemplate() = default;

    size_t size() const
    {
        return core.size() * sizeof( Block ) * 8;
    }

    bool isFinite() const
    {
        return !periphery;
    }

    bool any() const
    {
        for( const auto &block : core )
        {
            if( block != 0 )
                return true;
        }
        return periphery;
    }

    bool none() const
    {
        return !any();
    }

    size_t count() const
    {
        if( periphery )
            return -1;

        auto bitCount = []( Block block )
        {
            size_t count = 0;
            while( block > 0 )
            {
                count += block % 2;
                block /= 2;
            }
            return count;
        };
        auto accumulator = [&]( size_t sum, Block block )
        {
            return sum + bitCount( block );
        };
        return std::accumulate( core.begin(), core.end(), size_t( 0 ), accumulator );
    }

    void resize( size_t numBits )
    {
        ensureCoreSize( numBlocks( numBits ) );
    }

    void clear()
    {
        core.clear();
        periphery = false;
    }

    bool test( size_t pos ) const
    {
        size_t blockId = blockIndex( pos );
        size_t bitId = bitIndex( pos );

        if( blockId < core.size() )
            return ( core[blockId] >> bitId ) & 1;

        return periphery;
    }

    void set( size_t pos, bool value = true )
    {
        size_t blockId = blockIndex( pos );
        size_t bitId = bitIndex( pos );

        ensureCoreSize( blockId + 1 );

        if( value )
            core[blockId] |= ( Block( 1 ) << bitId );
        else
            core[blockId] &= ~( Block( 1 ) << bitId );
    }

    void reset( size_t pos )
    {
        set( pos, false );
    }

    void flip( size_t pos )
    {
        size_t blockId = blockIndex( pos );
        size_t bitId = bitIndex( pos );

        ensureCoreSize( blockId + 1 );

        core[blockId] ^= ( Block( 1 ) << bitId );
    }

    bool operator[]( size_t pos ) const
    {
        return test( pos );
    }

    std::string toString( char zero = '0', char one = '1' ) const
    {
        std::string result = "...";
        result += ( periphery ? one : zero );
        for( size_t i = size(); i-- > 0; )
            result += ( test( i ) ? one : zero );
        return result;
    }

    BitsetTemplate &operator=( const BitsetTemplate &other )
    {
        core = other.core;
        periphery = other.periphery;
        return *this;
    }

    BitsetTemplate &operator&=( const BitsetTemplate &other )
    {
        ensureCoreSize( other.core.size() );
        for( size_t i = 0; i < core.size(); ++i )
            core[i] &= other.getBlock( i );
        periphery &= other.periphery;
        return *this;
    }

    BitsetTemplate &operator|=( const BitsetTemplate &other )
    {
        ensureCoreSize( other.core.size() );
        for( size_t i = 0; i < core.size(); ++i )
            core[i] |= other.getBlock( i );
        periphery |= other.periphery;
        return *this;
    }

    BitsetTemplate &operator^=( const BitsetTemplate &other )
    {
        ensureCoreSize( other.core.size() );
        for( size_t i = 0; i < core.size(); ++i )
            core[i] ^= other.getBlock( i );
        periphery ^= other.periphery;
        return *this;
    }

    BitsetTemplate &operator-=( const BitsetTemplate &other )
    {
        ensureCoreSize( other.core.size() );
        for( size_t i = 0; i < core.size(); ++i )
            core[i] &= ~other.getBlock( i );
        periphery &= !other.periphery;
        return *this;
    }

    BitsetTemplate &operator<<=( size_t shift )
    {
        const size_t blockShift = shift / ( sizeof( Block ) * 8 );
        const size_t bitShift = shift % ( sizeof( Block ) * 8 );

        if( blockShift > 0 )
        {
            core.insert( core.begin(), blockShift, Block( 0 ) );
            core.resize( core.size() - blockShift );
        }

        if( bitShift > 0 )
        {
            Block carry = Block( 0 );
            for( size_t i = 0; i < core.size(); ++i )
            {
                Block newCarry = ( core[i] >> ( sizeof( Block ) * 8 - bitShift ) );
                core[i] = ( core[i] << bitShift ) | carry;
                carry = newCarry;
            }
        }

        return *this;
    }

    BitsetTemplate &operator>>=( size_t shift )
    {
        size_t blockShift = shift / ( sizeof( Block ) * 8 );
        size_t bitShift = shift % ( sizeof( Block ) * 8 );

        if( blockShift > 0 )
        {
            if( blockShift >= core.size() )
            {
                core.assign( core.size(), periphery ? ~Block( 0 ) : Block( 0 ) );
            }
            else
            {
                core.erase( core.begin(), core.begin() + blockShift );
                core.resize( core.size() + blockShift, periphery ? ~Block( 0 ) : Block( 0 ) );
            }
        }

        if( bitShift > 0 )
        {
            size_t complementary = sizeof( Block ) * 8 - bitShift;
            Block carry = ( ( ( long long unsigned )( periphery ? ~Block( 0 ) : Block( 0 ) ) ) << complementary ) & ( ~Block( 0 ) );
            for( size_t i = core.size(); i-- > 0; )
            {
                Block newCarry = ( core[i] << complementary );
                core[i] = ( core[i] >> bitShift ) | carry;
                carry = newCarry;
            }
        }

        return *this;
    }

    BitsetTemplate operator~() const
    {
        auto result = *this;
        for( auto &block : result.core )
            block = ~block;
        result.periphery = !periphery;
        return result;
    }

    BitsetTemplate operator&( const BitsetTemplate &other ) const
    {
        auto result = *this;
        result &= other;
        return result;
    }

    BitsetTemplate operator|( const BitsetTemplate &other ) const
    {
        auto result = *this;
        result |= other;
        return result;
    }

    BitsetTemplate operator^( const BitsetTemplate &other ) const
    {
        auto result = *this;
        result ^= other;
        return result;
    }

    BitsetTemplate operator-( const BitsetTemplate &other ) const
    {
        auto result = *this;
        result -= other;
        return result;
    }

    BitsetTemplate operator<<( size_t shift ) const
    {
        auto result = *this;
        result <<= shift;
        return result;
    }

    BitsetTemplate operator>>( size_t shift ) const
    {
        auto result = *this;
        result >>= shift;
        return result;
    }

    bool operator==( const BitsetTemplate &other ) const
    {
        size_t minSize = std::min( core.size(), other.core.size() );
        for( size_t i = 0; i < minSize; ++i )
        {
            if( core[i] != other.core[i] )
                return false;
        }

        if( core.size() > minSize )
        {
            for( size_t i = minSize; i < core.size(); ++i )
            {
                if( core[i] != ( other.periphery ? ~Block( 0 ) : Block( 0 ) ) )
                    return false;
            }
        }
        else if( other.core.size() > minSize )
        {
            for( size_t i = minSize; i < other.core.size(); ++i )
            {
                if( other.core[i] != ( periphery ? ~Block( 0 ) : Block( 0 ) ) )
                    return false;
            }
        }

        return periphery == other.periphery;
    }

    bool operator!=( const BitsetTemplate &other ) const
    {
        return !( *this == other );
    }
private:
    std::vector<Block> core;
    bool periphery;

    template<typename OtherBlock>
    friend class BitsetTemplate;

    size_t blockIndex( size_t pos ) const
    {
        return pos / ( sizeof( Block ) * 8 );
    }

    size_t bitIndex( size_t pos ) const
    {
        return pos % ( sizeof( Block ) * 8 );
    }

    size_t numBlocks( size_t numBits ) const
    {
        constexpr size_t bitsPerBlock = sizeof( Block ) * 8;
        return numBits / bitsPerBlock + ( numBits % bitsPerBlock > 0 ? 1 : 0 );
    }

    Block getBlock( size_t blockId ) const
    {
        return blockId < core.size() ? core[blockId] : ( periphery ? ~Block( 0 ) : Block( 0 ) );
    }

    void ensureCoreSize( size_t numBlocks )
    {
        if( core.size() < numBlocks )
            core.resize( numBlocks, periphery ? ~Block( 0 ) : Block( 0 ) );
    }

    void copy( const void *pointer, size_t bytes, bool p )
    {
        periphery = p;
        ensureCoreSize( numBlocks( bytes * 8 ) );
        ::clear( core.data(), p ? 0b11111111 : 0, core.size() * sizeof( Block ) );
        ::copy( core.data(), pointer, bytes );
    }
};

using Bitset = BitsetTemplate<unsigned long long>;
