#pragma once

#include <type_traits>

template<typename T, T mask, class Derived>
class Enum
{
    static_assert( std::is_arithmetic_v<T> &&std::is_unsigned_v<T>, "T must be an unsigned arithmetic type" );
public:
    constexpr Enum( const Derived &other ) : value( other.value )
    {}

    Derived operator|( const Derived &other ) const
    {
        return Enum( value | other.value );
    }

    Derived operator&( const Derived &other ) const
    {
        return Enum( value & other.value );
    }

    Derived operator^( const Derived &other ) const
    {
        return Enum( value ^ other.value );
    }

    Derived operator~() const
    {
        return Enum( ~value );
    }

    Derived &operator|=( const Derived &other )
    {
        value |= other.value;
        return *this;
    }

    Derived &operator&=( const Derived &other )
    {
        value &= other.value;
        return *this;
    }

    Derived &operator^=( const Derived &other )
    {
        value ^= other.value;
        return *this;
    }

    bool operator==( const Derived &other ) const
    {
        return value == other.value;
    }

    bool operator!=( const Derived &other ) const
    {
        return value != other.value;
    }

    Derived operator*() const
    {
        return Enum( value & mask );
    }

    constexpr operator const T() const
    {
        return value;
    }
protected:
    constexpr Enum( T v ) : value( v )
    {}

    constexpr static Enum produce( unsigned value )
    {
        return Enum( value );
    }

    T value;
};
