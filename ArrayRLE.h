#pragma once

#include <functional>
#include <algorithm>
#include <variant>
#include <vector>

template<typename T>
class ArrayRLE
{
public:
    struct Repetitive
    {
        size_t repeats;
        T value;
    };

    using Chunk = std::variant<Repetitive, std::vector<T>>;
private:
    // Chunk #i occupies interval: [ i == 0 ? 0 : ends[i - 1], ends[i] )
    std::vector<Chunk> chunks;
    std::vector<size_t> ends;
private:
    static size_t chunkSize( const Chunk& chunk ) noexcept
    {
        return std::visit( []( const auto & c ) noexcept -> size_t
        {
            using C = std::decay_t<decltype( c )>;

            if constexpr( std::is_same_v<C, Repetitive> )
                return c.repeats;
            else
                return c.size();
        }, chunk );
    }

    static bool isEmpty( const Chunk& chunk ) noexcept
    {
        return chunkSize( chunk ) == 0;
    }

    static void normalizeVector( Chunk& chunk )
    {
        auto* v = std::get_if<std::vector<T>>( &chunk );

        if( !v || v->size() != 1 )
            return;

        T value = std::move( ( *v )[0] );

        chunk = Repetitive{1, std::move( value )};
    }

    // Removes the first 'count' elements from v
    static void removePrefix( std::vector<T>& v, size_t count )
    {
        if( count == 0 )
            return;

        if( count == v.size() )
        {
            v.clear();
            return;
        }

        std::move( v.begin() + count, v.end(), v.begin() );
        v.resize( v.size() - count );
    }

    // Normalize two adjacent chunks. The result contains at most three chunks.
    static void normalizePair( Chunk left, Chunk right, std::vector<Chunk>& result )
    {
        result.clear();
        result.reserve( 3 );

        normalizeVector( left );
        normalizeVector( right );

        if( isEmpty( left ) )
        {
            if( !isEmpty( right ) )
                result.emplace_back( std::move( right ) );
            return;
        }

        if( isEmpty( right ) )
        {
            result.emplace_back( std::move( left ) );
            return;
        }

        // Repetitive + Repetitive
        if( auto* a = std::get_if<Repetitive>( &left ) )
        {
            if( auto* b = std::get_if<Repetitive>( &right ) )
            {
                if( a->value == b->value )
                {
                    a->repeats += b->repeats;
                    result.emplace_back( std::move( left ) );
                }
                else
                {
                    result.emplace_back( std::move( left ) );
                    result.emplace_back( std::move( right ) );
                }

                return;
            }
        }

        // Repetitive + vector
        if( auto* a = std::get_if<Repetitive>( &left ) )
        {
            auto* b = std::get_if<std::vector<T>>( &right );

            if( b )
            {
                if( a->value != b->front() )
                {
                    result.emplace_back( std::move( left ) );
                    result.emplace_back( std::move( right ) );
                    return;
                }

                // Find equal prefix of the vector.
                size_t prefix = 1;

                while( prefix < b->size() && ( *b )[prefix] == a->value )
                {
                    ++prefix;
                }

                a->repeats += prefix;

                removePrefix( *b, prefix );

                result.emplace_back( std::move( left ) );

                if( !b->empty() )
                {
                    normalizeVector( right );

                    if( !isEmpty( right ) )
                        result.emplace_back( std::move( right ) );
                }

                return;
            }
        }

        // vector + Repetitive
        if( auto* a = std::get_if<std::vector<T>>( &left ) )
        {
            auto* b = std::get_if<Repetitive>( &right );

            if( b )
            {
                if( a->back() != b->value )
                {
                    result.emplace_back( std::move( left ) );
                    result.emplace_back( std::move( right ) );
                    return;
                }

                // Find equal suffix.
                size_t suffix = 1;

                while( suffix < a->size() && ( *a )[a->size() - 1 - suffix] == b->value )
                {
                    ++suffix;
                }

                // Entire vector consists of the same value.
                if( suffix == a->size() )
                {
                    b->repeats += suffix;

                    result.emplace_back( std::move( right ) );
                    return;
                }

                // Remove the equal suffix from the vector.
                a->resize( a->size() - suffix );

                b->repeats += suffix;

                // A vector of one element is not allowed.
                normalizeVector( left );

                result.emplace_back( std::move( left ) );
                result.emplace_back( std::move( right ) );

                return;
            }
        }

        // vector + vector
        auto* a = std::get_if<std::vector<T>>( &left );
        auto* b = std::get_if<std::vector<T>>( &right );

        // At this point both must be vectors.
        if( !a || !b )
            return;

        // Different boundary values can simply be merged into one vector.
        if( a->back() != b->front() )
        {
            a->reserve( a->size() + b->size() );
            std::move( b->begin(), b->end(), std::back_inserter( *a ) );
            normalizeVector( left );
            result.emplace_back( std::move( left ) );
            return;
        }

        const T boundaryValue = a->back();

        // Find equal suffix in left.
        size_t suffix = 1;
        while( suffix < a->size() && ( *a )[a->size() - 1 - suffix] == boundaryValue )
            ++suffix;

        // Find equal prefix in right.
        size_t prefix = 1;
        while( prefix < b->size() && ( *b )[prefix] == boundaryValue )
            ++prefix;

        // Entire left and right are the same value.
        if( suffix == a->size() && prefix == b->size() )
        {
            result.emplace_back( Repetitive{suffix + prefix, std::move( ( *a )[0] )} );
            return;
        }

        // Remove suffix from left.
        const bool leftBecomesEmpty = suffix == a->size();
        if( !leftBecomesEmpty )
            a->resize( a->size() - suffix );

        // Remove prefix from right.
        const bool rightBecomesEmpty = prefix == b->size();
        if( !rightBecomesEmpty )
            removePrefix( *b, prefix );

        // Left part.
        if( !leftBecomesEmpty )
        {
            normalizeVector( left );
            result.emplace_back( std::move( left ) );
        }

        // Common boundary run.
        result.emplace_back( Repetitive{suffix + prefix, std::move( const_cast<T&>( boundaryValue ) )} );

        // Right part.
        if( !rightBecomesEmpty )
        {
            normalizeVector( right );
            result.emplace_back( std::move( right ) );
        }
    }

    // Append one already-created chunk. Only the boundary with the existing last chunk needs work.
    void appendChunk( Chunk chunk )
    {
        if( isEmpty( chunk ) )
            return;

        normalizeVector( chunk );

        if( chunks.empty() )
        {
            chunks.emplace_back( std::move( chunk ) );
            rebuildEnds();
            return;
        }

        Chunk left = std::move( chunks.back() );
        chunks.pop_back();

        std::vector<Chunk> boundary;

        normalizePair( std::move( left ), std::move( chunk ), boundary );

        for( Chunk& c : boundary )
            chunks.emplace_back( std::move( c ) );

        rebuildEnds();
    }

    // Fast path.
    // The source's first chunk is moved separately.
    // The remaining chunks are moved directly into chunks.
    void appendMoved( ArrayRLE&& other )
    {
        if( other.chunks.empty() )
            return;

        if( chunks.empty() )
        {
            chunks = std::move( other.chunks );
            ends = std::move( other.ends );
            return;
        }

        // Move only the two chunks touching the boundary.
        Chunk left = std::move( chunks.back() );
        chunks.pop_back();

        Chunk right = std::move( other.chunks.front() );

        std::vector<Chunk> boundary;
        boundary.reserve( 3 );

        normalizePair( std::move( left ), std::move( right ), boundary );

        // Reserve once for the final number of chunks.
        chunks.reserve( chunks.size() + boundary.size() + other.chunks.size() - 1 );

        // Add normalized boundary.
        for( Chunk& c : boundary )
            chunks.emplace_back( std::move( c ) );

        // Move every remaining source chunk directly.
        for( size_t i = 1; i < other.chunks.size(); ++i )
            chunks.emplace_back( std::move( other.chunks[i] ) );

        // Rebuild cumulative indexes once.
        rebuildEnds();

        // Release moved-from source chunks.
        other.chunks.clear();
        other.ends.clear();
    }

    void rebuildEnds()
    {
        ends.resize( chunks.size() );

        size_t position = 0;

        for( size_t i = 0; i < chunks.size(); ++i )
        {
            position += chunkSize( chunks[i] );
            ends[i] = position;
        }
    }
public:
    ArrayRLE()
    {}

    ArrayRLE( const ArrayRLE& other ) : chunks( other.chunks ), ends( other.ends )
    {}

    ArrayRLE( ArrayRLE&& other ) noexcept : chunks( std::move( other.chunks ) ), ends( std::move( other.ends ) )
    {}

    ArrayRLE& operator=( const ArrayRLE& other )
    {
        if( this != &other )
        {
            chunks = other.chunks;
            ends   = other.ends;
        }
        return *this;
    }

    ArrayRLE& operator=( ArrayRLE&& other ) noexcept
    {
        if( this != &other )
        {
            chunks = std::move( other.chunks );
            ends   = std::move( other.ends );
        }
        return *this;
    }


    // Append a repeated value.
    void append( T value, size_t repeats = 1 )
    {
        if( repeats != 0 )
            appendChunk( Repetitive{repeats, std::move( value ) } );
    }

    // Append an ordinary vector.
    void append( std::vector<T> values )
    {
        if( !values.empty() )
            appendChunk( std::move( values ) );
    }

    // Copy append.
    void append( const ArrayRLE& other )
    {
        if( this == &other )
        {
            ArrayRLE copy( other );
            appendMoved( std::move( copy ) );
            return;
        }

        ArrayRLE copy( other );
        appendMoved( std::move( copy ) );
    }

    // Move append.
    void append( ArrayRLE&& other )
    {
        if( this == &other )
            return;

        appendMoved( std::move( other ) );
    }

    // Random access. Returns nullptr for an out-of-range index.
    const T* operator[]( size_t index ) const
    {
        if( index >= size() )
            return nullptr;

        const size_t chunkIndex = std::upper_bound( ends.begin(), ends.end(), index ) - ends.begin();
        const size_t chunkBegin = chunkIndex == 0 ? 0 : ends[chunkIndex - 1];
        const size_t localIndex = index - chunkBegin;

        const Chunk& chunk = chunks[chunkIndex];

        if( const auto* r = std::get_if<Repetitive>( &chunk ) )
            return &r->value;

        const auto& v = std::get<std::vector<T>>( chunk );
        return &v[localIndex];
    }

    void forEach( const std::function<void( size_t, const T& )>& f ) const
    {
        for( auto& chunk : chunks )
        {
            if( auto values = std::get_if<std::vector<T>>( &chunk ) )
            {
                for( auto& v : *values )
                    f( 1, v );
                continue;
            }
            for( auto& v : *std::get_if<Repetitive>( &chunk ) )
                f( v.repeats, v.value );
        }
    }

    size_t size() const noexcept
    {
        return ends.empty() ? 0 : ends.back();
    }

    bool empty() const noexcept
    {
        return chunks.empty();
    }

    void clear() noexcept
    {
        chunks.clear();
        ends.clear();
    }
};
