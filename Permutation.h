#pragma once

#include <functional>
#include <vector>
#include <array>

#include "Exception.h"

template <typename T>
using Permute = std::function<bool( std::vector<T>& )>;

template <size_t N>
class Permutation
{
public:
    Permutation()
    {
        for( size_t i = 0; i < N; ++i )
            indices[i] = i;
    }

    Permutation( const std::array<size_t, N> &ind ) : indices( ind )
    {
        std::array<bool, N> existingIndices;
        for( size_t i = 0; i < N; ++i )
            existingIndices[i] = false;

        for( size_t i = 0; i < N; ++i )
        {
            makeException( indices[i] < N );
            existingIndices[indices[i]] = true;
        }

        for( size_t i = 0; i < N; ++i )
            makeException( existingIndices[i] );
    }

    size_t size() const
    {
        return N;
    }

    size_t operator[]( size_t i ) const
    {
        makeException( i < N );
        return indices[i];
    }

    template <typename T>
    std::array<T, N> operator()( const std::array<T, N> &data ) const
    {
        std::array<T, N> result;
        for( size_t i = 0; i < N; ++i )
            result[i] = data[indices[i]];
        return result;
    }

    template <typename T>
    std::vector<T> operator()( const std::vector<T> &data ) const
    {
        makeException( data.size() == N );

        std::vector<T> result;
        result.reserve( N );
        for( size_t i = 0; i < N; ++i )
            result.emplace_back( data[indices[i]] );
        return result;
    }

    template <typename T>
    Permute<T> apply() const
    {
        return [o = *this]( std::vector<T> &data )
        {
            if( data.size() != o.indices.size() )
                return false;

            std::vector<T> result;
            result.reserve( o.indices.size() );
            for( size_t i = 0; i < o.indices.size(); ++i )
                result.emplace_back( std::move( data[o.indices[i]] ) );

            data = std::move( result );
            return true;
        };
    }

    Permutation<N> operator*( const Permutation<N> &other ) const
    {
        std::array<size_t, N> result;
        for( size_t i = 0; i < N; ++i )
            result[i] = other.indices[indices[i]];
        return Permutation<N>( result );
    }

    Permutation<N> inverse() const
    {
        std::array<size_t, N> result;
        for( size_t i = 0; i < N; ++i )
            result[indices[i]] = i;
        return Permutation<N>( result );
    }

    Permutation<N> reverse() const
    {
        std::array<size_t, N> result;
        for( size_t i = 0; i < N; ++i )
            result[i] = indices[N - i - 1];
        return Permutation<N>( result );
    }

    bool operator==( const Permutation<N> &other ) const
    {
        return indices == other.indices;
    }

    bool operator!=( const Permutation<N> &other ) const
    {
        return indices != other.indices;
    }
private:
    std::array<size_t, N> indices;
};
