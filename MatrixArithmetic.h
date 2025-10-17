#pragma once

#include "Exception.h"
#include "Matrix.h"

template<typename T>
class MatrixArithmetic : public MatrixBase<T>
{
public:
    using MatrixBase<T>::w;
    using MatrixBase<T>::h;
    using MatrixBase<T>::operator();
    using MatrixBase<T>::reset;

    MatrixArithmetic()
        : MatrixBase<T>()
    {}

    MatrixArithmetic( int width, int height )
        : MatrixBase<T>( width, height )
    {}

    MatrixArithmetic( const MatrixBase<T> &other )
        : MatrixBase<T>( other )
    {}

    MatrixArithmetic( const MatrixBase<T> &&other )
        : MatrixBase<T>( other )
    {}

    MatrixArithmetic &operator=( const MatrixBase<T> &other )
    {
        return *( ( MatrixBase<T> * )this ) = other;
    }

    MatrixArithmetic &operator=( const MatrixBase<T> &&other )
    {
        return *( ( MatrixBase<T> * )this ) = other;
    }

    MatrixArithmetic &operator+=( const MatrixArithmetic &other )
    {
        // Matrix dimensions must match for addition
        makeException( w() == other.w() && h() == other.h() );

        for( int i = 0; i < h(); ++i )
        {
            for( int j = 0; j < w(); ++j )
            {
                *( *this )( j, i ) += *other( j, i );
            }
        }
        return *this;
    }

    MatrixArithmetic &operator*=( const MatrixArithmetic &other )
    {
        // Matrix dimensions must match for multiplication
        makeException( w() == other.h() );

        MatrixArithmetic result( other.w(), h() );
        for( int i = 0; i < result.h(); ++i )
        {
            for( int j = 0; j < result.w(); ++j )
            {
                T value = T();
                for( int k = 0; k < w(); ++k )
                {
                    value += *( *this )( k, i ) * *other( j, k );
                }
                *result( j, i ) = value;
            }
        }

        *this = std::move( result );
        return *this;
    }

    MatrixArithmetic &operator*=( const T &scalar )
    {
        for( int i = 0; i < h(); ++i )
        {
            for( int j = 0; j < w(); ++j )
            {
                *( *this )( j, i ) *= scalar;
            }
        }
        return *this;
    }

    MatrixArithmetic operator+( const MatrixArithmetic &other ) const
    {
        MatrixArithmetic result = *this;
        result += other;
        return result;
    }

    MatrixArithmetic operator*( const MatrixArithmetic &other ) const
    {
        MatrixArithmetic result = *this;
        result *= other;
        return result;
    }

    MatrixArithmetic operator*( const T &scalar ) const
    {
        MatrixArithmetic result = *this;
        result *= scalar;
        return result;
    }

    MatrixArithmetic convolution( const MatrixArithmetic &kernel ) const
    {
        // Kernel size must not exceed matrix size
        makeException( kernel.w() <= w() && kernel.h() <= h() );

        MatrixArithmetic result( w() - kernel.w() + 1, h() - kernel.h() + 1 );

        for( int i = 0; i < result.h(); ++i )
        {
            for( int j = 0; j < result.w(); ++j )
            {
                T value = T();
                for( int ki = 0; ki < kernel.h(); ++ki )
                {
                    for( int kj = 0; kj < kernel.w(); ++kj )
                    {
                        value += *( *this )( j + kj, i + ki ) * *kernel( kj, ki );
                    }
                }
                *result( j, i ) = value;
            }
        }

        return result;
    }

    MatrixArithmetic<T> minor( int col, int row ) const
    {
        MatrixArithmetic<T> result( w() - 1, h() - 1 );

        for( int i = 0, minorRow = 0; i < w(); ++i )
        {
            if( i == row )
                continue;

            for( int j = 0, minorCol = 0; j < h(); ++j )
            {
                if( j == col )
                    continue;

                *result( minorCol++, minorRow ) = *( *this )( j, i );
            }
            ++minorRow;
        }

        return result;
    }

    T det() const
    {
        // Determinant is only defined for square martices
        makeException( w() == h() );

        int size = w();
        if( size == 1 )
            return *( *this )( 0, 0 );

        if( size == 2 )
            return *( *this )( 0, 0 ) * *( *this )( 1, 1 ) - *( *this )( 0, 1 ) * *( *this )( 1, 0 );

        T result = T();
        for( int j = 0; j < size; ++j )
        {
            T cofactor = *( *this )( j, 0 ) * ( ( j % 2 == 0 ) ? 1 : -1 );
            result += cofactor * minor( j, 0 ).det();
        }

        return result;
    }

    MatrixArithmetic hadamardProduct( const MatrixArithmetic &other ) const
    {
        // Matrix dimensions must match for Hadamard product
        makeException( w() == other.w() && h() == other.h() );

        MatrixArithmetic result( w(), h() );
        for( int i = 0; i < h(); ++i )
        {
            for( int j = 0; j < w(); ++j )
            {
                *result( j, i ) = *( *this )( j, i ) * *other( j, i );
            }
        }

        return result;
    }

    MatrixArithmetic<T> adjugate() const
    {
        // Adjugate is only defined for square martices
        makeException( w() == h() );

        int size = w();
        MatrixArithmetic<T> result( size, size );

        for( int i = 0; i < size; ++i )
        {
            for( int j = 0; j < size; ++j )
            {
                *result( i, j ) = minor( j, i ).det() * ( ( ( i + j ) % 2 == 0 ) ? 1 : -1 );
            }
        }

        return result;
    }

    MatrixArithmetic inverse() const
    {
        // Inverse is only defined for square matrices
        makeException( w() == h() );

        T determinantValue = det();

        // Matrix is singular and cannot be inverted
        makeException( determinantValue < T() || determinantValue > T() );

        MatrixArithmetic<T> adj = adjugate();
        MatrixArithmetic<T> result( w(), h() );
        T invDet = T( 1 ) / determinantValue;

        for( int i = 0; i < h(); ++i )
        {
            for( int j = 0; j < w(); ++j )
            {
                *result( j, i ) = *adj( j, i ) * invDet;
            }
        }

        return result;
    }
};
