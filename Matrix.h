#pragma once

#include <functional>

#include "Buffer.h"
#include "Basic.h"

template<typename T>
class MatrixBase
{
private:
    int width, height, stride;
    Buffer<T> data;
    T *pointer;
public:
    MatrixBase()
    {
        pointer = nullptr;
        width = 0;
        height = 0;
        stride = 0;
    }

    MatrixBase( int w, int h ) : MatrixBase()
    {
        reset( w, h );
    }

    MatrixBase( const MatrixBase &other ) : MatrixBase()
    {
        reset( other.w(), other.h() * Sign( other.s() ) );
        data = other.data;
    }

    MatrixBase( MatrixBase &&other ) : MatrixBase()
    {
        data = std::move( other.data );
        pointer = other.pointer;
        width = other.width;
        height = other.height;
        stride = other.stride;
    }

    MatrixBase &operator=( const MatrixBase &other )
    {
        reset( other.w(), other.h() * Sign( other.s() ) );
        data << other.data();
        return *this;
    }

    MatrixBase &operator=( MatrixBase &&other )
    {
        data = std::move( other.data );
        pointer = other.pointer;
        width = other.width;
        height = other.height;
        stride = other.stride;
        return *this;
    }

    virtual ~MatrixBase()
    {}

    virtual void reset( int w, int h ) final
    {
        bool negative;
        int area;

        negative = h < 0;
        if( negative )
            h = -h;

        if( ( w > 0 ) && ( h > 0 ) )
        {
            width = w;
            height = h;
            area = w * h;
            data.reset( area );
        }
        else
        {
            width = 0;
            height = 0;
            data.reset( 0 );
        }

        setStride( negative );
    }

    virtual void reset( int w, int h, const T &value ) final
    {
        int i, area;
        T *buffer;

        reset( w, h );

        buffer = data[0];
        area = width * height;
        for( i = 0; i < area; ++i )
            buffer[i] = value;
    }

    virtual bool empty() const final
    {
        return height <= 0;
    }

    virtual int w() const final
    {
        return width;
    }

    virtual int h() const final
    {
        return height;
    }

    virtual int s() const final
    {
        return stride;
    }

    // Returns true, if image was flipped upside via change of stride's sign
    virtual bool setStride( bool negative ) final
    {
        bool result = negative != ( stride < 0 );
        if( negative )
        {
            stride = -width;
            pointer = data[0] + width * ( height - 1 );
        }
        else
        {
            stride = width;
            pointer = data[0];
        }
        return result;
    }

    virtual const T *operator()( int j, int i ) const final
    {
        if( !( ( 0 <= j ) && ( j < width ) && ( 0 <= i ) && ( i < height ) ) )
            return nullptr;
        return &pointer[i * stride + j];
    }

    virtual T *operator()( int j, int i ) final
    {
        if( !( ( 0 <= j ) && ( j < width ) && ( 0 <= i ) && ( i < height ) ) )
            return nullptr;
        return &pointer[i * stride + j];
    }

    virtual Buffer<const T> rawData() const final
    {
        return data;
    }

    virtual Buffer<T> rawData() final
    {
        return data;
    }

    virtual void copy( MatrixBase<T> &out ) const final
    {
        if( this == &out )
            return;

        if( ( out.width != width ) || ( out.height != height ) )
            out.reset( width, height * Sign( stride ) );

        int area = width * height * sizeof( T );
        ::copy( out.data[0], data[0], area );
    }

    virtual void flipX( MatrixBase<T> &out ) const final
    {
        T input, *output0, *output1;
        int i, j, halfWidth;

        if( this == &out )
        {
            halfWidth = width / 2;
            for( j = 0; j < halfWidth; ++j )
            {
                for( i = 0; i < height; ++i )
                {
                    output0 = out( j, i );
                    output1 = out( width - j - 1, i );

                    input = *output1;
                    *output1 = *output0;
                    *output0 = input;
                }
            }
            return;
        }

        if( ( out.width != width ) || ( out.height != height ) )
            out.reset( width, height );

        for( j = 0; j < width; ++j )
        {
            for( i = 0; i < height; ++i )
            {
                input = *( *this )( j, i );
                output0 = out( width - j - 1, i );

                *output0 = input;
            }
        }
    }

    virtual void flipY( MatrixBase<T> &out ) const final
    {
        int i, halfHeight, size;
        T *output0, *output1;

        size = width * sizeof( T );

        if( this == &out )
        {
            T *input = new T[width];
            halfHeight = height / 2;
            for( i = 0; i < halfHeight; ++i )
            {
                output0 = out( 0, i );
                output1 = out( 0, height - i - 1 );

                ::copy( input, output1, size );
                ::copy( output1, output0, size );
                ::copy( output0, input, size );
            }
            delete[] input;
            return;
        }

        if( ( out.width != width ) || ( out.height != height ) )
            out.reset( width, height );

        const T *input;
        for( i = 0; i < height; ++i )
        {
            input = ( *this )( 0, i );
            output0 = out( 0, height - i - 1 );

            ::copy( output0, input, size );
        }
    }

    virtual void transpose( MatrixBase<T> &out ) const final
    {
        MatrixBase<T> temporary;
        temporary.reset( height, width );

        T input, *output;
        int i, j;

        for( j = 0; j < width; ++j )
        {
            for( i = 0; i < height; ++i )
            {
                input = *( *this )( j, i );
                output = temporary( i, j );

                *output = input;
            }
        }

        out.data = std::move( temporary.data );
        out.pointer = temporary.pointer;
        out.width = temporary.width;
        out.height = temporary.height;
        out.stride = temporary.stride;
    }

    virtual void sub( MatrixBase<T> &out, int x0, int y0, int x1, int y1 ) const final
    {
        if( x1 > width )
            x1 = width;
        if( y1 > height )
            y1 = height;
        if( x1 < 0 )
            x1 = 0;
        if( y1 < 0 )
            y1 = 0;

        if( ( x0 > x1 ) || ( y0 > y1 ) )
        {
            out.reset( 0, 0 );
            return;
        }

        MatrixBase<T> temporary;
        temporary.reset( x1 - x0, y1 - y0 );

        auto output = temporary( 0, 0 );
        auto input = ( *this )( x0, y0 );
        auto size = temporary.w() * sizeof( T );

        for( int i = 0; i < temporary.h(); ++i )
        {
            ::copy( output, input, size );
            output += temporary.stride;
            input += stride;
        }

        out.data = std::move( temporary.data );
        out.pointer = temporary.pointer;
        out.width = temporary.width;
        out.height = temporary.height;
        out.stride = temporary.stride;
    }

    virtual void crop( MatrixBase<T> &out, int x0, int y0, int x1, int y1, const T &background ) const final
    {
        if( ( x0 > x1 ) || ( y0 > y1 ) )
        {
            out.reset( 0, 0 );
            return;
        }

        int cropWidth  = x1 - x0;
        int cropHeight = y1 - y0;

        MatrixBase<T> temporary;
        bool inPlace = this == &out;
        auto &target = inPlace ? temporary : out;
        target.reset( cropWidth, cropHeight, background );

        int srcX0 = ( x0 < 0 )   ? 0   : x0;
        int srcX1 = ( x1 > w() ) ? w() : x1;
        int srcY0 = ( y0 < 0 )   ? 0   : y0;
        int srcY1 = ( y1 > h() ) ? h() : y1;

        auto size = ( srcX1 - srcX0 ) * sizeof( T );
        if( size > 0 )
        {
            for( int iSrc = srcY0; iSrc < srcY1; ++iSrc )
            {
                int iDst = iSrc - y0;
                int jDst = srcX0 - x0;

                const T *srcPtr = ( *this )( srcX0, iSrc );
                T *dstPtr = target( jDst, iDst );

                if( srcPtr && dstPtr )
                    ::copy( dstPtr, srcPtr, size );
            }
        }

        if( inPlace )
        {
            out.data = std::move( temporary.data );
            out.pointer = temporary.pointer;
            out.width = temporary.width;
            out.height = temporary.height;
            out.stride = temporary.stride;
        }
    }

    virtual void place( MatrixBase<T> &out, int x, int y ) const final
    {
        int i, mini, minj, maxi, maxj;

        mini = y;
        if( mini < 0 )
            mini = 0;

        minj = x;
        if( minj < 0 )
            minj = 0;

        maxj = x + width;
        if( maxj > out.width )
            maxj = out.width;

        maxi = y + height;
        if( maxi > out.height )
            maxi = out.height;

        auto size = maxj - minj;
        if( size <= 0 )
            return;
        size *= sizeof( T );

        MatrixBase<T> temporary;

        bool useTemporary = this == &out;
        if( useTemporary )
            out.copy( temporary );
        auto &o = useTemporary ? temporary : out;

        auto input = ( *this )( minj - x, mini - y );
        auto output = o( minj, mini );
        for( i = mini; i < maxi; ++i )
        {
            ::copy( output, input, size );
            output += o.stride;
            input += stride;
        }

        if( useTemporary )
        {
            out.data = std::move( temporary.data );
            out.pointer = temporary.pointer;
            out.width = temporary.width;
            out.height = temporary.height;
            out.stride = temporary.stride;
        }
    }

    template<typename D>
    void transform( MatrixBase<D> &out, const std::function<void( int, int, int, int, const T &, D & )> &f ) const
    {
        const T *input;
        D *output;
        int i, j;

        if( ( out.w() != width ) || ( out.h() != height ) )
            out.reset( width, height );

        for( j = 0; j < width; ++j )
        {
            for( i = 0; i < height; ++i )
            {
                input = ( *this )( j, i );
                output = out( j, i );
                f( width, height, j, i, *input, *output );
            }
        }
    }
};
