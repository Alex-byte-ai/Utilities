#pragma once

#include "Basic.h"

class BufferBase
{
protected:
    void *pointer, *data;
    unsigned dataLength, bufferLength;
public:
    BufferBase()
    {
        data = nullptr;
        pointer = nullptr;
        bufferLength = 0;
        dataLength = 0;
    }

    virtual ~BufferBase()
    {}

    unsigned length() const
    {
        return dataLength;
    }

    unsigned store() const
    {
        return bufferLength;
    }

    template<typename T>
    friend class Buffer;
};

template<typename T>
class Buffer : public BufferBase
{
private:
    using Self = Buffer<T>;
    using Parent = BufferBase;
public:
    Buffer() : Parent()
    {}

    Buffer( unsigned n ) : Parent()
    {
        reset( n );
    }

    Buffer( const Self &other ) : Parent()
    {
        reset( other );
    }

    Buffer( const BufferBase &other ) : Parent()
    {
        reset( other );
    }

    virtual ~Buffer()
    {
        delete[]( T * )data;
    }

    T *operator[]( unsigned i )
    {
        if( ( i + 1 ) * sizeof( T ) <= dataLength )
            return ( T * )pointer + i;
        return nullptr;
    }

    const T *operator[]( unsigned i ) const
    {
        if( ( i + 1 ) * sizeof( T ) <= dataLength )
            return ( const T * )pointer + i;
        return nullptr;
    }

    unsigned count() const
    {
        return dataLength / sizeof( T ) + ( dataLength % sizeof( T ) > 0 );
    }

    void reset( unsigned n )
    {
        Self::dataLength = Self::bufferLength = n * sizeof( T );

        delete[]( T * )Self::data;
        Self::pointer = Self::data = n ? new T[n] : nullptr;

        if( Self::pointer )
            clear( Self::pointer, Self::bufferLength );
    }

    void reset( const BufferBase &other )
    {
        if( this == &other )
            return;

        delete[]( T * )Self::data;
        data = nullptr;

        Self::pointer = other.pointer;
        Self::bufferLength = other.bufferLength;
        Self::dataLength = other.dataLength;
    }

    Self &operator=( const Self &other )
    {
        if( this == &other )
            return *this;

        if( Self::data )
        {
            Self::dataLength = other.dataLength;
            if( Self::bufferLength < other.length() )
            {
                reset( count() );
                Self::dataLength = other.dataLength;
            }

            copy( Self::pointer, other.pointer, Self::dataLength );
            return *this;
        }

        Self::dataLength = Min( Self::bufferLength, other.dataLength );
        copy( Self::pointer, other.pointer, Self::dataLength );
        return *this;
    }

    Self &operator=( Self &&other )
    {
        if( this == &other )
            return *this;

        delete[]( T * )Self::data;

        Self::data = other.data;
        Self::pointer = other.pointer;
        Self::bufferLength = other.bufferLength;
        Self::dataLength = other.dataLength;

        other.data = nullptr;
        other.pointer = nullptr;
        other.bufferLength = 0;
        other.dataLength = 0;

        return *this;
    }
};
