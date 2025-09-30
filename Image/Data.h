#pragma once

#include <vector>

#include "Image/Format.h"
#include "Exception.h"
#include "Bits.h"

namespace ImageConvert
{
using Pixel = std::vector<BitList>;
using Color = std::vector<double>;

template<typename A, typename B>
static inline B toInt( A x, const Channel &c )
{
    makeException( 0 <= x && x <= 1 );
    auto max = c.max();
    return max > 0 ? B( x * max + 0.5 ) : B( 0 );
}

template<typename A, typename B>
static inline B toFloat( A x, const Channel &c )
{
    auto max = c.max();
    makeException( 0 <= x && x <= max );
    return max > 0 ? B( x ) / B( max ) : B( 0 );
}

// Converts between integer Pixel channels and normalized double Color channels in range [0, 1]
// Matches formats channels of source and destination
// For channels with 0 bits, the result is 0
// If the channel is '_' its value is ignored, when read and is written as 0
template<typename VA, typename VB>
static inline VB convert( const VA &src, const PixelFormat &srcFmt, const PixelFormat &dstFmt )
{
    using A = typename VA::value_type;
    using B = typename VB::value_type;

    auto atob = []( A a, const Channel & cA, const Channel & cB ) -> B
    {
        if( std::is_same_v<A, B> &&( std::is_same_v<A, double> || cA.bits == cB.bits ) )
        {
            return a;
        }

        double tmp;
        if constexpr( std::is_same_v<A, double> )
        {
            tmp = a;
        }
        else
        {
            tmp = toFloat<A, double>( a, cA );
        }

        if constexpr( std::is_same_v<B, double> )
        {
            return tmp;
        }
        else
        {
            return toInt<double, B>( tmp, cB );
        }
    };

    VB dst;
    unsigned dstId = 0;
    for( const auto &dstChannel : dstFmt.channels )
    {
        if( dstChannel.channel == '_' )
        {
            dst.push_back( B( 0 ) );
            ++dstId;
            continue;
        }

        auto srcId = srcFmt.id( dstChannel.channel );
        if( !srcId )
        {
            auto replacement = dstFmt.replace( dstId, srcFmt, srcId );
            if( !srcId )
            {
                makeException( replacement && replacement->constant );
                if constexpr( std::is_same_v<B, double> )
                    dst.push_back( toFloat<B, BitList>( *replacement->constant, dstChannel ) );
                else
                    dst.push_back( *replacement->constant );
                ++dstId;
                continue;
            }
            makeException( srcId );
        }

        auto &srcChannel = srcFmt.channels[*srcId];
        dst.push_back( atob( src[*srcId], srcChannel, dstChannel ) );
        ++dstId;
    }
    return dst;
}
}
