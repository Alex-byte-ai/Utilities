#include "Image/JPG.h"

#include <unordered_map>
#include <algorithm>
#include <array>
#include <map>

#include "Image/PixelIO.h"

#include "Matrix3D.h"

namespace ImageConvert
{
// ---------------------------------------------------------------------------
// Segment parsing
// ---------------------------------------------------------------------------

// Seek the next JPEG non-restart marker byte
// Returns true and sets `marker` when a valid segment marker is found
// Returns false on EOF/error or if the sequence is invalid here
static bool readNextMarker( ReaderBase &r, uint8_t &marker )
{
    uint8_t b;

    // Search for 0xFF
    while( true )
    {
        if( !r.read( 1, &b ) )
            return false;

        if( b == 0xFF )
            break;
    }

    // Collapse runs of 0xFF and inspect the first non-0xFF byte
    while( true )
    {
        if( !r.read( 1, &b ) )
            return false;

        if( b == 0xFF )
            continue; // Padding

        if( b == 0x00 || ( b >= 0xD0 && b <= 0xD7 ) )
            return false; // Byte-stuffing (0x00) or restart markers (0xD0..0xD7) inside entropy data

        // Otherwise this is a legitimate inter-segment marker byte
        marker = b;
        return true;
    }

    // Unreachable
    return false;
}

SegmentGeneric::SegmentGeneric( uint8_t m, bool l ) : marker( m ), hasLength( l )
{}

bool SegmentGeneric::read( ReaderBase &r, uint16_t length )
{
    data.clear();
    if( length == 0 )
        return true;

    data.resize( length );

    if( !r.read( length, data.data() ) )
        return false;

    return true;
}

bool SegmentGeneric::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, marker };
    if( !w.write( sizeof( m ), m ) )
        return false;

    if( hasLength )
    {
        // Write length
        uint16_t len = swapBe16( data.size() + 2 );
        if( !w.write( sizeof( len ), &len ) )
            return false;

        if( !data.empty() )
        {
            // Write data
            if( !w.write( data.size(), data.data() ) )
                return false;
        }
    }

    return true;
}

bool SegmentSOI::read( ReaderBase &, uint16_t )
{
    return true;
}

bool SegmentSOI::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xD8 };
    return w.write( sizeof( m ), m );
}

bool SegmentEOI::read( ReaderBase &, uint16_t )
{
    return true;
}

bool SegmentEOI::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xD9 };
    return w.write( sizeof( m ), m );
}

bool SegmentTEM::read( ReaderBase &, uint16_t )
{
    return true;
}

bool SegmentTEM::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0x01 };
    return w.write( sizeof( m ), m );
}

bool SegmentJFIF::read( ReaderBase &r, uint16_t length )
{
    if( !r.read( sizeof( info ), &info ) )
        return false;

    info.xDensity = swapBe16( info.xDensity );
    info.yDensity = swapBe16( info.yDensity );

    size_t thumbBytes = size_t( 3 ) * size_t( info.xThumbnail ) * size_t( info.yThumbnail );
    if( thumbBytes )
    {
        thumbnail.resize( thumbBytes );
        if( !r.read( thumbBytes, thumbnail.data() ) )
            return false;
    }

    size_t consumed = sizeof( info ) + thumbBytes;
    if( consumed != length )
        return false;

    return true;
}

bool SegmentJFIF::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xE0 };
    if( !w.write( sizeof( m ), m ) )
        return false;

    DataJFIF be = info;
    be.xDensity = swapBe16( info.xDensity );
    be.yDensity = swapBe16( info.yDensity );

    // Write length
    uint16_t len = swapBe16( sizeof( be ) + thumbnail.size() + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    if( !w.write( sizeof( be ), &be ) )
        return false;

    if( !thumbnail.empty() )
    {
        if( !w.write( thumbnail.size(), thumbnail.data() ) )
            return false;
    }

    return true;
}

bool SegmentEXIF::read( ReaderBase &r, uint16_t length )
{
    tiffData.clear();
    if( length == 0 )
        return true;

    tiffData.resize( length );

    if( !r.read( length, tiffData.data() ) )
        return false;

    return true;
}

bool SegmentEXIF::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[2] = { 0xFF, 0xE1 };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Write length
    uint16_t len = swapBe16( tiffData.size() + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    if( !tiffData.empty() )
    {
        // Write data
        if( !w.write( tiffData.size(), tiffData.data() ) )
            return false;
    }

    return true;
}

bool SegmentICC::read( ReaderBase &r, uint16_t length )
{
    if( length < sizeof( hdr ) )
        return false;

    if( !r.read( sizeof( hdr ), &hdr ) )
        return false;

    chunkData.clear();

    size_t rem = length - sizeof( hdr );
    if( rem )
    {
        chunkData.resize( rem );
        if( !r.read( rem, chunkData.data() ) )
            return false;
    }

    return true;
}

bool SegmentICC::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xE2 };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Write length
    uint16_t len = swapBe16( sizeof( hdr ) + chunkData.size() + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    if( !w.write( sizeof( hdr ), &hdr ) )
        return false;

    if( !chunkData.empty() )
    {
        if( !w.write( chunkData.size(), chunkData.data() ) )
            return false;
    }

    return true;
}

bool SegmentAdobe::read( ReaderBase &r, uint16_t length )
{
    if( length < sizeof( hdr ) )
        return false;

    if( !r.read( sizeof( hdr ), &hdr ) )
        return false;

    hdr.version = swapBe16( hdr.version );
    hdr.flags0 = swapBe16( hdr.flags0 );
    hdr.flags1 = swapBe16( hdr.flags1 );

    // Validate identifier
    if( compare( hdr.identifier, "Adobe", sizeof( hdr.identifier ) ) )
        return false;

    // Read any remaining bytes as extraData
    extraData.clear();
    size_t rem = length - sizeof( hdr );
    if( rem > 0 )
    {
        extraData.resize( rem );
        if( !r.read( rem, extraData.data() ) )
            return false;
    }

    return true;
}

bool SegmentAdobe::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xEE };
    if( !w.write( sizeof( m ), m ) )
        return false;

    DataAdobe be = hdr;
    be.version = swapBe16( be.version );
    be.flags0  = swapBe16( be.flags0 );
    be.flags1  = swapBe16( be.flags1 );

    // Write length
    uint16_t len = swapBe16( sizeof( be ) + extraData.size() + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    // Write header
    if( !w.write( sizeof( be ), &be ) )
        return false;

    // Write trailing data
    if( !extraData.empty() )
    {
        if( !w.write( extraData.size(), extraData.data() ) )
            return false;
    }

    return true;
}

bool SegmentCOM::read( ReaderBase &r, uint16_t length )
{
    commentary.clear();
    if( length == 0 )
        return true;

    std::vector<char> tmp( length );
    if( !r.read( length, tmp.data() ) )
        return false;

    commentary.assign( tmp.data(), tmp.size() );
    return true;
}

bool SegmentCOM::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xFE };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Write length
    uint16_t len = swapBe16( commentary.size() + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    if( !commentary.empty() )
    {
        // Write data
        if( !w.write( commentary.size(), commentary.data() ) )
            return false;
    }

    return true;
}

bool SegmentSOF::read( ReaderBase &r, uint16_t length )
{
    if( !r.read( sizeof( header ), &header ) )
        return false;

    size_t consumed = sizeof( header );

    header.imageHeight = swapBe16( header.imageHeight );
    header.imageWidth  = swapBe16( header.imageWidth );

    auto size = sizeof( components[0] ) * header.numComponents;
    components.resize( header.numComponents );
    if( !r.read( size, components.data() ) )
        return false;

    consumed += size;

    if( consumed != length )
        return false;

    return true;
}

bool SegmentSOF::write( WriterBase &w ) const
{
    // Write marker
    if( !writeMarker( w ) )
        return false;

    // Write length
    auto size = sizeof( components[0] ) * header.numComponents;
    uint16_t len = swapBe16( sizeof( header ) + size + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    // Prepare DataSOF with BE fields
    auto be = header;
    be.imageHeight = swapBe16( header.imageHeight );
    be.imageWidth  = swapBe16( header.imageWidth );
    if( !w.write( sizeof( be ), &be ) )
        return false;

    if( !w.write( size, components.data() ) )
        return false;

    return true;
}

bool SegmentSOF0::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xC0 };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF1::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xC1 };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF2::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xC2 };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF3::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xC3 };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF5::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xC5 };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF6::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xC6 };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF7::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xC7 };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF9::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xC9 };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF10::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xCA };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF11::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xCB };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF13::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xCD };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF14::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xCE };
    return w.write( sizeof( m ), m );
}

bool SegmentSOF15::writeMarker( WriterBase &w ) const
{
    uint8_t m[] = { 0xFF, 0xCF };
    return w.write( sizeof( m ), m );
}

bool SegmentDNL::read( ReaderBase &r, uint16_t length )
{
    if( length != sizeof( numberOfLines ) )
        return false;

    if( !r.read( length, &numberOfLines ) )
        return false;

    numberOfLines = swapBe16( numberOfLines );
    return true;
}

bool SegmentDNL::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xDC };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Write length
    uint16_t len = swapBe16( sizeof( numberOfLines ) + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    uint16_t lines = swapBe16( numberOfLines );
    if( !w.write( sizeof( lines ), &lines ) )
        return false;

    return true;
}

bool SegmentDAC::read( ReaderBase &r, uint16_t length )
{
    if( length % sizeof( tables[0] ) != 0 )
        return false;

    size_t n = length / sizeof( tables[0] );
    tables.resize( n );

    for( auto& table : tables )
    {
        if( !r.read( sizeof( table ), &table ) )
            return false;

        // Validate ranges per ITU-T T.81 / ISO/IEC 10918-1

        if( table.tb > 3 )
            return false;

        if( table.tc > 1 )
            return false;

        if( table.tc == 1 )
        {
            // AC conditioning:
            if( table.cs < 1 || table.cs > 63 )
                return false;
        }
        else
        {
            // DC conditioning:
            uint8_t U = table.cs >> 4;
            uint8_t L = table.cs & 0x0F;
            if( L > U || U > 15 )
                return false;
        }
    }

    return true;
}

bool SegmentDAC::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xCC };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Write length
    uint16_t len = swapBe16( tables.size() * sizeof( tables[0] ) + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    if( !w.write( sizeof( tables[0] ) * tables.size(), tables.data() ) )
        return false;

    return true;
}

bool SegmentDQT::read( ReaderBase &r, uint16_t length )
{
    size_t remaining = length;

    tables.clear();
    while( remaining > 0 )
    {
        uint8_t pq_tq;
        if( !r.read( 1, &pq_tq ) )
            return false;

        remaining -= 1;

        size_t entrySize = ( ( ( pq_tq >> 4 ) & 0x0F ) == 0 ) ? 1 : 2;

        Table t;
        if( entrySize == 1 )
            t = DataDQT::Table8();
        if( entrySize == 2 )
            t = DataDQT::Table16();

        bool ok = std::visit( [&]( auto & table )
        {
            table.pq_tq = pq_tq;

            for( auto& v : table.values )
            {
                if( remaining < entrySize )
                    return false;

                if( !r.read( entrySize, &v ) )
                    return false;

                if( entrySize == 2 )
                    v = swapBe16( v );

                remaining -= entrySize;
            }

            tables.emplace_back( t );
            return true;
        }, t );

        if( !ok )
            return false;
    }

    return true;
}

bool SegmentDQT::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xDB };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Compute body size
    uint32_t bodySize = 0;
    for( auto const &t : tables )
    {
        std::visit( [&]( const auto & table )
        {
            bodySize += sizeof( table );
        }, t );
    }

    // Write length
    uint16_t len = swapBe16( bodySize + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    for( auto const &t : tables )
    {
        bool ok = std::visit( [&]( const auto & table )
        {
            if( !w.write( sizeof( table.pq_tq ), &table.pq_tq ) )
                return false;

            for( auto& v : table.values )
            {
                auto raw = v;
                if( sizeof( raw ) == 2 )
                    raw = swapBe16( raw );

                if( !w.write( sizeof( raw ), &raw ) )
                    return false;
            }

            return true;
        }, t );

        if( !ok )
            return false;
    }

    return true;
}

bool SegmentDHT::read( ReaderBase &r, uint16_t length )
{
    size_t remaining = length;
    tables.clear();

    while( remaining > 0 )
    {
        uint8_t tc_th;
        if( !r.read( 1, &tc_th ) )
            return false;

        remaining -= 1;

        Table t;
        t.tc_th = tc_th;
        size_t total = 0;
        for( int i = 0; i < 16; ++i )
        {
            if( !r.read( 1, &t.counts[i] ) )
                return false;

            remaining -= 1;

            total += t.counts[i];
        }

        if( total > remaining )
            return false;

        t.symbols.resize( total );
        if( total && !r.read( total, t.symbols.data() ) )
            return false;

        remaining -= total;
        tables.push_back( std::move( t ) );
    }

    return true;
}

bool SegmentDHT::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xC4 };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Compute body size
    uint32_t bodySize = 0;
    for( auto const &t : tables )
        bodySize += 1 + 16 + ( uint32_t )t.symbols.size();

    // Write length
    uint16_t len = swapBe16( bodySize + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    for( auto const &t : tables )
    {
        if( !w.write( 1, &t.tc_th ) )
            return false;

        for( int i = 0; i < 16; ++i )
        {
            if( !w.write( 1, &t.counts[i] ) )
                return false;
        }

        if( !t.symbols.empty() )
        {
            if( !w.write( t.symbols.size(), t.symbols.data() ) )
                return false;
        }
    }

    return true;
}

bool SegmentDRI::read( ReaderBase &r, uint16_t length )
{
    if( length != 2 )
        return false;

    if( !r.read( sizeof( restartInterval ), &restartInterval ) )
        return false;

    restartInterval = swapBe16( restartInterval );

    return true;
}

bool SegmentDRI::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xDD };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Write length
    uint16_t len = swapBe16( 4 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    auto b = swapBe16( restartInterval );
    if( !w.write( sizeof( b ), &b ) )
        return false;

    return true;
}

bool SegmentSOS::read( ReaderBase &r, uint16_t length )
{
    auto readByte = [&r]( uint8_t &b )
    {
        return r.read( 1, &b );
    };

    // Track how many header bytes we consumed
    size_t consumed = 0;

    if( !readByte( numScanComponents ) )
        return false;

    consumed += 1;

    size_t size = numScanComponents * sizeof( components[0] );
    components.resize( numScanComponents );
    if( !r.read( size, components.data() ) )
        return false;

    consumed += size;

    if( !readByte( spectralStart ) )
        return false;

    if( !readByte( spectralEnd ) )
        return false;

    if( !readByte( successiveApproximation ) )
        return false;

    consumed += 3;

    if( consumed > length )
        return false;

    // If there are extra header bytes (some encoders might include additional data), discard them to consume exactly 'length' header bytes
    if( consumed < length )
    {
        size_t extra = length - consumed;

        // Read and discard in small chunks to avoid large temporary buffers
        const size_t CHUNK = 256;
        std::vector<uint8_t> tmp;
        tmp.resize( std::min( extra, CHUNK ) );

        while( extra > 0 )
        {
            size_t toRead = std::min( extra, tmp.size() );

            if( !r.read( toRead, tmp.data() ) )
                return false;

            extra -= toRead;
        }
    }

    // Read entropy-coded data:

    entropy.clear();
    rawEntropy.clear();
    nextMarker.reset();

    auto slice = &entropy.emplace_back();

    while( true )
    {
        uint8_t b;
        if( !readByte( b ) )
            return false;

        // Normal entropy byte
        if( b != 0xFF )
        {
            slice->data.push_back( b );
            rawEntropy.push_back( b );
            continue;
        }

        // Read bytes following 0xFF until decision can be made
        uint8_t c;
        while( true )
        {
            if( !readByte( c ) )
                return false;

            // Byte-stuffing
            if( c == 0x00 )
            {
                // 0xFF is encoded as 0xFF 0x00
                slice->data.push_back( b );
                rawEntropy.push_back( b );
                rawEntropy.push_back( c );
                break;
            }

            // Padding
            if( c == 0xFF )
            {
                // Skip until next non-0xFF byte
                rawEntropy.push_back( c );
                continue;
            }

            // Restart marker
            if( 0xD0 <= c && c <= 0xD7 )
            {
                slice = &entropy.emplace_back();
                slice->restartMarker = c;
                rawEntropy.push_back( b );
                rawEntropy.push_back( c );
                break;
            }

            // Otherwise we encountered a terminating marker byte, not included into entropy data
            nextMarker = c;
            return true;
        }
    }

    // Unreachable
    return true;
}

bool SegmentSOS::write( WriterBase &w ) const
{
    // Write marker
    uint8_t m[] = { 0xFF, 0xDA };
    if( !w.write( sizeof( m ), m ) )
        return false;

    // Write length
    size_t size = numScanComponents * sizeof( components[0] );
    uint16_t len = swapBe16( 1 + size + 3 + 2 );
    if( !w.write( sizeof( len ), &len ) )
        return false;

    if( !w.write( 1, &numScanComponents ) )
        return false;

    // Components
    if( !w.write( size, components.data() ) )
        return false;

    if( !w.write( 1, &spectralStart ) )
        return false;

    if( !w.write( 1, &spectralEnd ) )
        return false;

    if( !w.write( 1, &successiveApproximation ) )
        return false;

    if( !rawEntropy.empty() )
    {
        // Write data
        if( !w.write( rawEntropy.size(), rawEntropy.data() ) )
            return false;
    }

    return true;
}

void JPEG::read( ReaderBase &r )
{
    std::optional<uint8_t> markerOpt;
    std::shared_ptr<SegmentSOS> sos;
    uint16_t length = 0;

    auto addSegment = [&]( std::shared_ptr<Segment> segment )
    {
        if( segment )
        {
            makeException( segment->read( r, length ) );
            segments.emplace_back( std::move( segment ) );

            markerOpt.reset();
            if( sos )
            {
                markerOpt = sos->nextMarker;
                makeException( markerOpt );
                sos = nullptr;
            }
        }
    };

    auto addGeneric = [&]( uint8_t marker, bool hasLength )
    {
        makeException( hasLength || length == 0 );
        addSegment( std::make_shared<SegmentGeneric>( marker, hasLength ) );
    };

    segments.clear();

    uint8_t soiMarker[2];
    makeException( r.read( sizeof( soiMarker ), soiMarker ) && soiMarker[0] == 0xFF && soiMarker[1] == 0xD8 );

    addSegment( std::make_shared<SegmentSOI>() );

    while( true )
    {
        if( !markerOpt )
        {
            markerOpt.emplace();
            makeException( readNextMarker( r, *markerOpt ) );
        }

        uint8_t &marker = *markerOpt;

        length = 0;

        if( marker == 0xD9 )
        {
            addSegment( std::make_shared<SegmentEOI>() );
            break;
        }

        switch( marker )
        {
        case 0x01:
            addSegment( std::make_shared<SegmentTEM>() );
            break;
        default:
            // Check for other marker types
            // Problem will be exposed latter, if current marker is zero-length marker not listed here
            break;
        }

        // Read length
        makeException( r.read( sizeof( length ), &length ) );
        length = swapBe16( length ) - 2;

        switch( marker )
        {
        case 0xE0:
            addSegment( std::make_shared<SegmentJFIF>() );
            break;
        case 0xE1:
            addSegment( std::make_shared<SegmentEXIF>() );
            break;
        case 0xE2:
            addSegment( std::make_shared<SegmentICC>() );
            break;
        case 0xEE:
            addSegment( std::make_shared<SegmentAdobe>() );
            break;
        case 0xFE:
            addSegment( std::make_shared<SegmentCOM>() );
            break;
        case 0xC0:
            addSegment( std::make_shared<SegmentSOF0>() );
            break;
        case 0xC1:
            addSegment( std::make_shared<SegmentSOF1>() );
            break;
        case 0xC2:
            addSegment( std::make_shared<SegmentSOF2>() );
            break;
        case 0xC3:
            addSegment( std::make_shared<SegmentSOF3>() );
            break;
        case 0xC5:
            addSegment( std::make_shared<SegmentSOF5>() );
            break;
        case 0xC6:
            addSegment( std::make_shared<SegmentSOF6>() );
            break;
        case 0xC7:
            addSegment( std::make_shared<SegmentSOF7>() );
            break;
        case 0xC9:
            addSegment( std::make_shared<SegmentSOF9>() );
            break;
        case 0xCA:
            addSegment( std::make_shared<SegmentSOF10>() );
            break;
        case 0xCB:
            addSegment( std::make_shared<SegmentSOF11>() );
            break;
        case 0xCD:
            addSegment( std::make_shared<SegmentSOF13>() );
            break;
        case 0xCE:
            addSegment( std::make_shared<SegmentSOF14>() );
            break;
        case 0xCF:
            addSegment( std::make_shared<SegmentSOF15>() );
            break;
        case 0xDC:
            addSegment( std::make_shared<SegmentDNL>() );
            break;
        case 0xCC:
            addSegment( std::make_shared<SegmentDAC>() );
            break;
        case 0xDB:
            addSegment( std::make_shared<SegmentDQT>() );
            break;
        case 0xC4:
            addSegment( std::make_shared<SegmentDHT>() );
            break;
        case 0xDD:
            addSegment( std::make_shared<SegmentDRI>() );
            break;
        case 0xDA:
            addSegment( sos = std::make_shared<SegmentSOS>() );
            break;
        default:
            // Generic markers
            makeException( 0xE0 <= marker && marker <= 0xEF ); // APPn
            addGeneric( marker, true );
            continue;
        }
    }
}

void JPEG::write( WriterBase &w ) const
{
    for( auto const &s : segments )
    {
        makeException( s->write( w ) );
    }
}

// ---------------------------------------------------------------------------
// Compression pipeline
// ---------------------------------------------------------------------------

struct IDCT
{
    // Implementation for integer values
    // Two-pass separable integer IDCT with high-precision integer coefficients
    // Uses 64-bit accumulators
    // The per-pass scale factor 0.5 from the double algorithm is included into the integer coefficients
    static void idct8x8( const int32_t *in, int32_t *out )
    {
        constexpr int K = 20; // Scaling bits for coefficient precision
        constexpr int64_t HALF = 1LL << ( K - 1 );

        // Build coefficient table
        static int32_t coef[8][8];
        static bool coefInitialized = false;
        if( !coefInitialized )
        {
            const double PI = 3.14159265358979323846;
            double scale = double( 1LL << K );
            for( int u = 0; u < 8; ++u )
            {
                double Cu = ( u == 0 ) ? 1.0 / Sqrt( 2.0 ) : 1.0;
                for( int x = 0; x < 8; ++x )
                {
                    double c = Cu * Cos( ( 2.0 * x + 1.0 ) * u * PI / 16.0 ) * 0.5; // Include the per-pass scale factor 0.5
                    long long ci = Round( c * scale );
                    coef[u][x] = ( int32_t )ci;
                }
            }
            coefInitialized = true;
        }

        int32_t tmp[64];

        // Rows
        for( int y = 0; y < 8; ++y )
        {
            for( int x = 0; x < 8; ++x )
            {
                int64_t sum = 0;
                for( int u = 0; u < 8; ++u )
                    sum += int64_t( in[y * 8 + u] ) * int64_t( coef[u][x] );

                // Rounding and descaling
                tmp[y * 8 + x] = int32_t( ( sum + HALF ) >> K );
            }
        }

        // Columns
        for( int x = 0; x < 8; ++x )
        {
            for( int y = 0; y < 8; ++y )
            {
                int64_t sum = 0;
                for( int v = 0; v < 8; ++v )
                    sum += int64_t( tmp[v * 8 + x] ) * int64_t( coef[v][y] );

                // Rounding and descaling
                out[y * 8 + x] = int32_t( ( sum + HALF ) >> K );
            }
        }
    }

    // Implementation for double values
    static void idct8x8( const double *in, double *out )
    {
        double tmp[64];
        const double PI = 3.14159265358979323846;

        // Rows
        for( int y = 0; y < 8; ++y )
        {
            for( int x = 0; x < 8; ++x )
            {
                double s = 0.0;
                for( int u = 0; u < 8; ++u )
                {
                    double Cu = ( u == 0 ) ? 1.0 / Sqrt( 2.0 ) : 1.0;
                    s += Cu * in[y * 8 + u] * Cos( ( 2 * x + 1 ) * u * PI / 16.0 );
                }
                tmp[y * 8 + x] = s * 0.5;
            }
        }

        // Columns
        for( int x = 0; x < 8; ++x )
        {
            for( int y = 0; y < 8; ++y )
            {
                double s = 0.0;
                for( int v = 0; v < 8; ++v )
                {
                    double Cv = ( v == 0 ) ? 1.0 / Sqrt( 2.0 ) : 1.0;
                    s += Cv * tmp[v * 8 + x] * Cos( ( 2 * y + 1 ) * v * PI / 16.0 );
                }
                out[y * 8 + x] = s * 0.5;
            }
        }
    }

    // Compare integer AAN output to double version
    static void verify( const int32_t *iin )
    {
        double fin[64], fout[64];
        for( int i = 0; i < 64; ++i )
            fin[i] = double( iin[i] );

        idct8x8( fin, fout );

        int32_t iout[64];
        idct8x8( iin, iout );

        double maxErr = 0.0;
        for( int i = 0; i < 64; ++i )
        {
            double e = Abs( fout[i] - double( iout[i] ) );
            if( e > maxErr )
                maxErr = e;
        }

        makeException( maxErr < 2 );
    }
};

Huffman::Huffman( std::shared_ptr<JPEG> img, unsigned s, const PixelFormat &pfmt ) :
    Compression( s, pfmt ),
    image( std::move( img ) ),
    sof( image->findSingle<SegmentSOF>() ),
    dri( image->findSingle<SegmentDRI>() ),
    dht( image->find<SegmentDHT>() ),
    sos( image->find<SegmentSOS>() )
{
    makeException( sof && !dht.empty() && !sos.empty() );
}

void Huffman::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Huffman::decompress( Format &fmt, const Reference &, Reference &destination ) const
{
    struct Block
    {
        int32_t coefficients[64] = {0};
        uint8_t componentId = 0; // SOF componentId
    };

    struct HuffmanTable
    {
        unsigned minCode[17];
        unsigned maxCode[17];
        unsigned valPtr[17];
        std::vector<uint8_t> symbols;
    };

    struct MCU
    {
        std::vector<Block> blocks;
    };

    struct Accumulator
    {
        std::vector<MCU> mcus;
    };

    makeException( fmt.compression.front().get() == this );

    const SegmentSOS* currentSegment = nullptr;
    int Ss = 0, Se = 0, Ah = 0, Al = 0;
    Accumulator acc;

    // Key { tableClass ( 0 = DC, 1 = AC ), tableID ( 0..3 ) }
    std::map<std::pair<uint8_t, uint8_t>, HuffmanTable> huffmanTables;

    auto buildTable = []( const SegmentDHT::Table & t )
    {
        HuffmanTable table;
        table.symbols = t.symbols;

        // List of code lengths in symbol order
        std::vector<unsigned> lengthHuffman;
        lengthHuffman.reserve( 256 );
        for( unsigned l = 1; l <= 16; ++l )
        {
            uint8_t count = t.counts[l - 1];
            for( uint8_t i = 0; i < count; ++i )
                lengthHuffman.push_back( l );
        }
        lengthHuffman.push_back( 0 ); // Terminator

        // Validate symbol count
        size_t symbolsCount = lengthHuffman.size() - 1;
        makeException( symbolsCount == table.symbols.size() );

        // List of codes in symbol order
        std::vector<unsigned> codeHuffman;
        codeHuffman.resize( symbolsCount );

        unsigned code = 0, p = 0;
        size_t length = lengthHuffman[0];

        // Produce codes in increasing code length order
        while( lengthHuffman[p] != 0 )
        {
            while( lengthHuffman[p] == length )
            {
                codeHuffman[p] = code;
                ++p;
                ++code;

                // Code must fit in 'length' bits
                makeException( length == 0 || code <= ( 1u << length ) );
            }

            // After finishing codes of 'length' length , shift left for next length
            if( lengthHuffman[p] != 0 ) // Only shift, if there are more codes
            {
                code <<= 1;
                ++length;
                makeException( length <= 16 );
            }
        }

        p = 0;
        for( unsigned l = 1; l <= 16; ++l )
        {
            uint8_t count = t.counts[l - 1];
            if( count )
            {
                table.valPtr[l]  = p;
                table.minCode[l] = codeHuffman[p];

                p += count;
                table.maxCode[l] = codeHuffman[p - 1];
            }
            else
            {
                // Signal empty length
                table.valPtr[l] = 0;
                table.minCode[l] = 1;
                table.maxCode[l] = 0;
            }
        }

        // Number of symbols must equal to p
        makeException( p == symbolsCount );

        return table;
    };

    // Parse all DHT segments
    for( auto& d : dht )
    {
        makeException( d != nullptr );
        for( auto& table : d->tables )
        {
            uint8_t tc = ( table.tc_th >> 4 ) & 0x0F;
            uint8_t th = table.tc_th & 0x0F;
            auto key = std::make_pair( tc, th );

            // No duplicates expected
            makeException( huffmanTables.find( key ) == huffmanTables.end() );

            huffmanTables.emplace( key, buildTable( table ) );
        }
    }

    // Validate that every SOS selector used has a DHT entry
    for( auto s : sos )
    {
        makeException( s != nullptr );
        for( const auto& component : s->components )
        {
            uint8_t sel = component.huffmanSelectors;
            uint8_t dcid = ( sel >> 4 ) & 0x0F;
            uint8_t acid = sel & 0x0F;

            makeException( huffmanTables.find( {0, dcid} ) != huffmanTables.end() );
            makeException( huffmanTables.find( {1, acid} ) != huffmanTables.end() );
        }
    }

    // Verify reader bit-order is MSB-first
    {
        std::vector<uint8_t> sample{94, 15, 233, 85, 193, 17, 27, 6};

        Reader probe( sample.data(), sample.size(), 0 );

        for( size_t j = 0; j < sample.size(); ++j )
        {
            // Read 8 bits one bit at a time from sample and assemble into a byte
            uint8_t assembled = 0;
            for( int i = 0; i < 8; ++i )
            {
                BitList b = 0;
                makeException( probe.read( 1, b ) );

                // If reader is MSB-first, the first read bit is the MSB of the byte
                assembled = ( uint8_t )( ( assembled << 1 ) | ( uint8_t )b );
            }

            // Compare assembled to the first raw entropy byte
            makeException( assembled == sample[j] );
        }
    }

    auto contains = []( const HuffmanTable * table, unsigned code, unsigned count )
    {
        // Table does not exist
        if( !table )
            return false;

        // Invalid bit count
        if( count <= 0 || 16 < count )
            return false;

        unsigned mc = table->minCode[count];
        unsigned xc = table->maxCode[count];

        // Empty range
        if( mc > xc )
            return false;

        return mc <= code && code <= xc;
    };

    auto lookup = [&contains]( const HuffmanTable * table, unsigned code, unsigned count )
    {
        makeException( contains( table, code, count ) );

        unsigned index = table->valPtr[count] + code - table->minCode[count];
        makeException( index < table->symbols.size() );

        return table->symbols[index];
    };

    auto findTable = [&]( uint8_t tc, uint8_t th ) -> const HuffmanTable *
    {
        auto i = huffmanTables.find( std::make_pair( tc, th ) );
        if( i == huffmanTables.end() )
            return nullptr;

        return &i->second;
    };

    auto dcTable = [&]( auto & component )
    {
        auto sel = component.huffmanSelectors;
        uint8_t th = ( sel >> 4 ) & 0x0F; // DC id
        return findTable( 0, th );
    };

    auto acTable = [&]( auto & component )
    {
        auto sel = component.huffmanSelectors;
        uint8_t th = sel & 0x0F; // AC id
        return findTable( 1, th );
    };

    auto mcuGeometry = [&]()
    {
        uint8_t maxH = 0, maxV = 0;
        makeException( sof != nullptr );
        for( auto& c : sof->components )
        {
            uint8_t H = ( c.samplingFactors >> 4 ) & 0xF;
            uint8_t V = c.samplingFactors & 0x0F;
            maxH = std::max( maxH, H );
            maxV = std::max( maxV, V );
        }
        makeException( maxH > 0 && maxV > 0 );
        return std::pair{ maxH, maxV };
    };

    auto mcuCount = [&]()
    {
        auto [maxH, maxV] = mcuGeometry();

        uint16_t width  = sof->header.imageWidth;
        uint16_t height = sof->header.imageHeight;

        makeException( width > 0 && height > 0 );

        size_t mcuX = ( width  + ( 8 * maxH - 1 ) ) / ( 8 * maxH );
        size_t mcuY = ( height + ( 8 * maxV - 1 ) ) / ( 8 * maxV );

        return mcuX * mcuY;
    };

    // Used across slices and MCUs for progressive refinement
    // Persists across slices within same scan
    uint32_t EOBRUN = 0;

    auto prepare = [&]( const SegmentSOS * s )
    {
        makeException( s != nullptr );
        currentSegment = s;

        Ss = s->spectralStart;
        Se = s->spectralEnd;
        Ah = ( s->successiveApproximation >> 4 ) & 0x0F;
        Al = s->successiveApproximation & 0x0F;

        // Basic sanity checks on progression parameters
        makeException( 0 <= Ss && Ss <= 63 );
        makeException( Ss <= Se && Se <= 63 );
        makeException( 0 <= Ah && Ah <= 15 );
        makeException( 0 <= Al && Al <= 30 );

        EOBRUN = 0;
    };

    auto getSymbol = [&]( Reader & reader, unsigned & length, uint8_t & outSymbol, const HuffmanTable * table )
    {
        makeException( table );
        makeException( table->symbols.size() > 0 ); // Table must have symbols

        unsigned code = 0;
        length = 0;

        // Compute max code length available in this table (1..16), 0 means no codes
        auto maxLenAvailable = [&]()
        {
            unsigned mx = 0;
            for( unsigned l = 1; l <= 16; ++l )
            {
                // Treat only non-empty ranges as valid
                if( table->minCode[l] <= table->maxCode[l] )
                    mx = l;
            }
            return mx;
        };

        unsigned maxLen = maxLenAvailable();
        makeException( 0 < maxLen && maxLen <= 16 );

        do
        {
            BitList bit;
            makeException( reader.read( 1, bit ) );

            code = ( code << 1 ) | ( unsigned )bit;
            length++;

            makeException( length <= maxLen );
        }
        while( !contains( table, code, length ) );

        outSymbol = lookup( table, code, length );
    };

    auto getAmplitude = [&]( Reader & reader, BitList category, int64_t& amplitude )
    {
        // Categories beyond 31 are impossible
        makeException( category <= 31 );

        if( category == 0 )
        {
            amplitude = 0;
            return;
        }

        BitList a = 0;
        makeException( reader.read( category, a ) );

        // Extension as per JPEG HUFF_EXTEND semantics
        if( a < ( 1ULL << ( category - 1 ) ) )
            amplitude = a - ( ( 1ULL << category ) - 1 );
        else
            amplitude = a;

        // Apply successive approximation scaling (Al)
        if( amplitude < 0 )
            amplitude = -( ( int64_t )( ( uint64_t )( -amplitude ) << Al ) );
        else
            amplitude = ( int64_t )( ( uint64_t )( amplitude ) << Al );
    };

    // Helpers p1/m1 depend on Al

    auto p1_of = [&]( int Alv )
    {
        // Keep reasonable limits
        makeException( 0 <= Alv && Alv < 31 );

        return 1 << Alv;
    };

    auto m1_of = [&]( int Alv )
    {
        makeException( 0 <= Alv && Alv < 31 );

        return -( ( int32_t )1 << Alv );
    };

    auto addAC = [&]( Reader & r, Block & blk, auto & component )
    {
        // Choose starting coefficient index, if spectral start is 0 then AC starts from 1 (skip DC)
        int startK = ( Ss == 0 ) ? 1 : Ss;

        // Validate AC table presence for this component
        const HuffmanTable* acTbl = acTable( component );
        makeException( acTbl != nullptr );

        if( Ah == 0 )
        {
            // Initial scan (first pass)
            for( int k = startK; k <= Se; ++k )
            {
                unsigned count = 0;
                uint8_t symbol = 0;
                getSymbol( r, count, symbol, acTbl );

                uint8_t run = ( symbol >> 4 ) & 0xF;
                uint8_t length = symbol & 0xF;

                // Maximum allowed run is 15 (4 bits)
                makeException( run <= 15 );
                makeException( length <= 31 );

                if( run == 0 && length == 0 )
                    break; // EOB

                if( run > 0 )
                {
                    // Sanity check to prevent large overflow
                    makeException( k + run <= 10000 );

                    k += run;
                }

                makeException( 0 <= k && k < 64 );

                int64_t amplitude = 0;
                getAmplitude( r, length, amplitude );

                blk.coefficients[k] = ( int32_t )amplitude;
            }
        }
        else
        {
            // Refinement scan (Ah > 0)
            int p1 = p1_of( Al );
            int32_t m1 = m1_of( Al );

            int k = startK;

            // If there is an outstanding EOBRUN, apply correction bits to already-nonzero coefficients in band
            if( EOBRUN > 0 )
            {
                for( ; k <= Se; ++k )
                {
                    if( blk.coefficients[k] != 0 )
                    {
                        BitList b;
                        makeException( r.read( 1, b ) );
                        if( b )
                        {
                            if( ( blk.coefficients[k] & p1 ) == 0 )
                            {
                                if( blk.coefficients[k] >= 0 )
                                    blk.coefficients[k] += p1;
                                else
                                    blk.coefficients[k] += m1;
                            }
                        }
                    }
                }

                if( EOBRUN > 0 )
                    EOBRUN--;
                return;
            }

            const HuffmanTable* table = acTbl;
            makeException( table );

            while( k <= Se )
            {
                unsigned count = 0;
                uint8_t symbol = 0;
                getSymbol( r, count, symbol, table );

                int run = ( symbol >> 4 ) & 0xF;
                int s = symbol & 0xF;

                makeException( run >= 0 && run <= 15 );
                makeException( s >= 0 && s <= 255 );

                if( s != 0 )
                {
                    // New nonzero, read sign
                    BitList signbit;
                    makeException( r.read( 1, signbit ) );
                    int32_t newval = signbit ? p1 : m1;

                    // Advance and apply correction bits to already-nonzero coefficients while consuming runs
                    do
                    {
                        if( k > Se )
                            break;

                        if( blk.coefficients[k] != 0 )
                        {
                            BitList corr;
                            makeException( r.read( 1, corr ) );
                            if( corr )
                            {
                                if( ( blk.coefficients[k] & p1 ) == 0 )
                                {
                                    if( blk.coefficients[k] >= 0 )
                                        blk.coefficients[k] += p1;
                                    else
                                        blk.coefficients[k] += m1;
                                }
                            }
                        }
                        else
                        {
                            // One zero consumed from run
                            run--;
                            if( run < 0 )
                                break;
                        }

                        k++;
                    }
                    while( k <= Se );

                    if( k <= Se )
                    {
                        blk.coefficients[k] = newval;
                    }

                    k++;
                }
                else
                {
                    // If s == 0 either ZRL (run==15) or EOBRUN (run != 15)
                    if( run == 15 )
                    {
                        k += 15;
                        continue;
                    }
                    else
                    {
                        // Run in [0..14] here
                        makeException( 0 <= run && run <= 14 );

                        uint32_t e = 1u << run;
                        if( run )
                        {
                            BitList appended = 0;
                            makeException( r.read( run, appended ) );

                            // Appended must be less than (1<<run)
                            makeException( appended < ( ( 1ULL << run ) ) );

                            e += ( uint32_t ) appended;
                        }

                        // We just decode one band now
                        // EOBRUN counts remaining bands after this one
                        if( e > 0 )
                            e--; // We are processing one band occurrence now

                        EOBRUN = e;
                        break;
                    }
                }
            } // While
        }
    };

    // Keyed by SOF componentId
    std::unordered_map<uint8_t, int32_t> lastDC;

    auto addDC = [&]( Reader & r, Block & blk, auto & component )
    {
        const HuffmanTable* dcTbl = dcTable( component );
        makeException( dcTbl != nullptr );

        if( Ah == 0 )
        {
            // Initial DC pass: Huffman-coded category + amplitude
            unsigned count = 0;
            uint8_t symbol = 0;
            getSymbol( r, count, symbol, dcTbl );

            makeException( symbol <= 31 );

            int64_t amplitude = 0;
            getAmplitude( r, symbol, amplitude );

            // Amplitude is the difference from previous DC (per component)
            // Add to the predictor and store result as the actual DC coefficient:
            int32_t prev = lastDC[ blk.componentId ]; // Initializes to 0 if absent by default
            int64_t sum = int64_t( prev ) + amplitude;

            makeException( std::numeric_limits<int32_t>::lowest() <= sum && sum <= std::numeric_limits<int32_t>::max() );

            int32_t dc = ( int32_t )sum;

            blk.coefficients[0] = dc;

            // Update predictor
            lastDC[ blk.componentId ] = dc;
        }
        else
        {
            // DC refinement: single appended bit per block (binary decision)
            BitList bit;
            makeException( r.read( 1, bit ) );

            int p1 = p1_of( Al );
            if( bit )
            {
                blk.coefficients[0] |= p1;
            }
        }
    };

    acc.mcus.resize( mcuCount() );

    // Main loop
    for( auto segment : sos )
    {
        prepare( segment );

        for( auto& slice : segment->entropy )
        {
            if( slice.restartMarker.has_value() )
            {
                lastDC.clear();
                EOBRUN = 0;
            }

            Reader reader( slice.data.data(), slice.data.size(), 0 );

            for( auto& mcu : acc.mcus )
            {
                for( auto& sc : segment->components )
                {
                    // Locate SOF component for sampling factors
                    auto it = std::find_if( sof->components.begin(), sof->components.end(), [&]( const auto & c )
                    {
                        return c.componentId == sc.componentId;
                    } );
                    makeException( it != sof->components.end() );

                    uint8_t H = ( it->samplingFactors >> 4 ) & 0xF;
                    uint8_t V = it->samplingFactors & 0x0F;
                    makeException( H > 0 && V > 0 );
                    size_t blocks = H * V;

                    for( size_t b = 0; b < blocks; ++b )
                    {
                        auto& blk = mcu.blocks.emplace_back();
                        blk.componentId = sc.componentId;

                        // If spectral range includes DC, decode DC first
                        if( Ss == 0 )
                            addDC( reader, blk, sc );

                        // If spectral range includes AC coefficients, decode them
                        if( Se >= 1 )
                            addAC( reader, blk, sc );
                    }
                }
            }
        }
    }

    // Count total blocks
    uint32_t totalBlocks = 0;
    for( auto &mcu : acc.mcus )
        totalBlocks += mcu.blocks.size();

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( 4 + totalBlocks * ( 1 + 64 * 4 ), fmt, destination );

    Writer writer( destination.link, destination.bytes, fmt.offset );

    makeException( writer.write( sizeof( totalBlocks ), &totalBlocks ) );

    // For each MCU in scan order, append its blocks in the same order they were decoded
    for( auto &mcu : acc.mcus )
    {
        for( auto &blk : mcu.blocks )
        {
            makeException( writer.write( sizeof( blk.componentId ), &blk.componentId ) );

            for( size_t i = 0; i < 64; ++i )
                makeException( writer.write( sizeof( blk.coefficients[i] ), &blk.coefficients[i] ) );
        }
    }
}

bool Huffman::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

Arithmetic::Arithmetic( std::shared_ptr<JPEG> img, unsigned s, const PixelFormat &pfmt ) :
    Compression( s, pfmt ),
    image( std::move( img ) ),
    sof( image->findSingle<SegmentSOF>() ),
    dri( image->findSingle<SegmentDRI>() ),
    dac( image->find<SegmentDAC>() ),
    sos( image->find<SegmentSOS>() )
{
    makeException( sof && !dac.empty() && !sos.empty() );
}

void Arithmetic::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Arithmetic::decompress( Format &, const Reference &, Reference & ) const
{
    // Not implemented
    makeException( false );
}

bool Arithmetic::equals( const Compression & ) const
{
    // Not implemented
    makeException( false );
    return false;
}

Quantization::Quantization( std::shared_ptr<JPEG> img, unsigned s, const PixelFormat &pfmt ) :
    Compression( s, pfmt ),
    image( std::move( img ) ),
    sof( image->findSingle<SegmentSOF>() ),
    dqt( image->find<SegmentDQT>() )
{
    makeException( sof && !dqt.empty() );
}

void Quantization::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Quantization::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    // Build quantization table map
    // Key: tableId
    std::unordered_map<int, std::array<int, 64>> quantMap;

    static const uint8_t ZigZag[64] =
    {
        0,  1,  5,  6, 14, 15, 27, 28,
        2,  4,  7, 13, 16, 26, 29, 42,
        3,  8, 12, 17, 25, 30, 41, 43,
        9, 11, 18, 24, 31, 40, 44, 53,
        10, 19, 23, 32, 39, 45, 52, 54,
        20, 22, 33, 38, 46, 51, 55, 60,
        21, 34, 37, 47, 50, 56, 59, 61,
        35, 36, 48, 49, 57, 58, 62, 63
    };

    makeException( fmt.compression.front().get() == this );

    for( auto seg : dqt )
    {
        for( const auto & t : seg->tables )
        {
            std::visit( [&]( auto & table )
            {
                uint8_t pq_tq = table.pq_tq;
                int tq = pq_tq & 0x0F;

                std::array<int, 64> arr;
                for( int i = 0; i < 64; ++i )
                    arr[i] = table.values[i];

                quantMap[tq] = arr;
            }, t );
        }
    }

    // Read input blocks
    Reader reader( source.link, source.bytes, fmt.offset );

    uint32_t count = 0;
    makeException( reader.read( sizeof( count ), &count ) );

    // Preserve same serialized layout

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( 4 + count * ( 1 + 64 * 4 ), fmt, destination );

    Writer writer( destination.link, destination.bytes, fmt.offset );

    makeException( writer.write( sizeof( count ), &count ) );

    for( uint32_t bi = 0; bi < count; ++bi )
    {
        uint8_t compId = 0;
        makeException( reader.read( sizeof( compId ), &compId ) );

        // Find quantization table for this component from SOF
        int quantId = 0;
        bool found = false;
        for( auto &c : sof->components )
        {
            if( c.componentId == compId )
            {
                quantId = c.quantTableId;
                found = true;
                break;
            }
        }
        makeException( found );

        auto qit = quantMap.find( quantId );
        makeException( qit != quantMap.end() );
        const auto &q = qit->second;

        // Read coefficients, multiply by quant
        int32_t coeffs[64];
        for( int i = 0; i < 64; ++i )
        {
            int32_t v = 0;
            makeException( reader.read( sizeof( v ), &v ) );

            int64_t deq = int64_t( v ) * int64_t( q[i] );
            makeException( std::numeric_limits<int32_t>::lowest() <= deq && deq <= std::numeric_limits<int32_t>::max() );

            coeffs[i] = ( int32_t )deq;
        }

        // Write result removing zig-zag order
        makeException( writer.write( sizeof( compId ), &compId ) );
        for( int i = 0; i < 64; ++i )
        {
            auto k = coeffs[ZigZag[i]];
            makeException( writer.write( sizeof( k ), &k ) );
        }
    }
}

bool Quantization::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

DCT::DCT( std::shared_ptr<JPEG> img, unsigned s, const PixelFormat &pfmt ) :
    Compression( s, pfmt ),
    image( std::move( img ) ),
    sof( image->findSingle<SegmentSOF>() )
{
    makeException( sof );
}

void DCT::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void DCT::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader reader( source.link, source.bytes, fmt.offset );

    uint32_t count = 0;
    makeException( reader.read( sizeof( count ), &count ) );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( 4 + count * ( 1 + 64 * 4 ), fmt, destination );

    Writer writer( destination.link, destination.bytes, fmt.offset );

    makeException( writer.write( sizeof( count ), &count ) );

    int32_t in[64], out[64];
    for( uint32_t j = 0; j < count; ++j )
    {
        uint8_t compId = 0;
        makeException( reader.read( sizeof( compId ), &compId ) );

        for( int i = 0; i < 64; ++i )
            makeException( reader.read( sizeof( in[i] ), &in[i] ) );

        // Apply AAN integer IDCT (input in natural row-major order)
        // IDCT::verify( in );
        IDCT::idct8x8( in, out );

        // Output: keep int32_t spatial samples (before level shift)
        makeException( writer.write( sizeof( compId ), &compId ) );

        for( int i = 0; i < 64; ++i )
            makeException( writer.write( sizeof( out[i] ), &out[i] ) );
    }
}

bool DCT::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

BlockGrouping::BlockGrouping( std::shared_ptr<JPEG> img, unsigned s, const PixelFormat &pfmt ) :
    Compression( s, pfmt ),
    image( std::move( img ) ),
    sof( image->findSingle<SegmentSOF>() )
{
    makeException( sof );
}

void BlockGrouping::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void BlockGrouping::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader reader( source.link, source.bytes, fmt.offset );

    uint32_t count = 0;
    makeException( reader.read( sizeof( count ), &count ) );

    struct Block
    {
        uint8_t compId;
        std::array<int32_t, 64> v;
    };
    std::vector<Block> blocks;
    blocks.reserve( count );

    // Read all input blocks
    for( uint32_t i = 0; i < count; ++i )
    {
        Block block;
        makeException( reader.read( sizeof( block.compId ), &block.compId ) );

        for( int j = 0; j < 64; ++j )
            makeException( reader.read( sizeof( block.v[j] ), &block.v[j] ) );

        blocks.push_back( std::move( block ) );
    }

    // Compute MCU geometry
    uint8_t maxH = 1, maxV = 1;
    for( auto &c : sof->components )
    {
        uint8_t H = ( c.samplingFactors >> 4 ) & 0x0F;
        uint8_t V = c.samplingFactors & 0x0F;
        maxH = std::max( maxH, H );
        maxV = std::max( maxV, V );
    }

    uint16_t imageW = sof->header.imageWidth;
    uint16_t imageH = sof->header.imageHeight;

    size_t mcusX = ( imageW + ( 8 * maxH - 1 ) ) / ( 8 * maxH );
    size_t mcusY = ( imageH + ( 8 * maxV - 1 ) ) / ( 8 * maxV );

    // Per-component block grid sizes
    uint8_t componentCount = sof->header.numComponents;
    std::vector<size_t> compBlockW( componentCount ), compBlockH( componentCount );
    for( size_t i = 0; i < componentCount; ++i )
    {
        uint8_t H = ( sof->components[i].samplingFactors >> 4 ) & 0x0F;
        uint8_t V = sof->components[i].samplingFactors & 0x0F;
        compBlockW[i] = mcusX * H;
        compBlockH[i] = mcusY * V;
    }

    // Prepare per-component containers (blocks in raster order)

    std::vector<std::vector<int16_t>> compBlocks( componentCount );
    for( size_t i = 0; i < componentCount; ++i )
    {
        size_t numBlocks = compBlockW[i] * compBlockH[i];
        compBlocks[i].resize( numBlocks * 64, 0 );
    }

    size_t inIndex = 0;
    for( size_t my = 0; my < mcusY; ++my )
    {
        for( size_t mx = 0; mx < mcusX; ++mx )
        {
            // Per-component counters for this MCU
            std::vector<size_t> placed( componentCount, 0 );

            for( size_t sofci = 0; sofci < componentCount; ++sofci )
            {
                uint8_t Hs = ( sof->components[sofci].samplingFactors >> 4 ) & 0x0F;
                uint8_t Vs = sof->components[sofci].samplingFactors & 0x0F;

                for( uint8_t by = 0; by < Vs; ++by )
                {
                    for( uint8_t bx = 0; bx < Hs; ++bx )
                    {
                        makeException( inIndex < blocks.size() );

                        uint8_t blockCompId = blocks[inIndex].compId;

                        // Map blockCompId to targetC index
                        size_t targetCi = SIZE_MAX;
                        for( size_t k = 0; k < componentCount; ++k )
                        {
                            if( sof->components[k].componentId == blockCompId )
                            {
                                targetCi = k;
                                break;
                            }
                        }
                        makeException( targetCi != SIZE_MAX );

                        uint8_t Ht = ( sof->components[targetCi].samplingFactors >> 4 ) & 0x0F;
                        uint8_t Vt = sof->components[targetCi].samplingFactors & 0x0F;

                        size_t placedIdx = placed[targetCi]++;
                        size_t compBlocksPerRow = compBlockW[targetCi];

                        size_t blockX = ( mx * Ht ) + ( placedIdx % Ht );
                        size_t blockY = ( my * Vt ) + ( placedIdx / Ht );

                        size_t destBlockIndex = blockY * compBlocksPerRow + blockX;
                        makeException( destBlockIndex < compBlocks[targetCi].size() / 64 );

                        int16_t *dest = compBlocks[targetCi].data() + destBlockIndex * 64;
                        for( int k = 0; k < 64; ++k )
                        {
                            int32_t sval = blocks[inIndex].v[k];
                            makeException( std::numeric_limits<int16_t>::lowest() <= sval && sval <= std::numeric_limits<int16_t>::max() );

                            dest[k] = ( int16_t )sval;
                        }

                        ++inIndex;
                    }
                }
            } // SOF components
        }
    }

    // Now serialize output
    size_t bytes = 2 + 2 + 1;
    for( size_t i = 0; i < componentCount; ++i )
    {
        uint32_t numBlocks = uint32_t( compBlockW[i] * compBlockH[i] );
        bytes += 1 + 1 + 1 + 4 + 64 * 2 * numBlocks;
    }

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( bytes, fmt, destination );

    Writer writer( destination.link, destination.bytes, fmt.offset );

    uint16_t widthBlocks = uint16_t( mcusX * maxH );
    uint16_t heightBlocks = uint16_t( mcusY * maxV );

    makeException( writer.write( sizeof( widthBlocks ), &widthBlocks ) );
    makeException( writer.write( sizeof( heightBlocks ), &heightBlocks ) );
    makeException( writer.write( sizeof( componentCount ), &componentCount ) );

    for( size_t i = 0; i < componentCount; ++i )
    {
        uint8_t compId = sof->components[i].componentId;
        uint8_t sampling = sof->components[i].samplingFactors;
        uint8_t qId = sof->components[i].quantTableId;
        uint32_t numBlocks = uint32_t( compBlockW[i] * compBlockH[i] );

        makeException( writer.write( sizeof( compId ), &compId ) );
        makeException( writer.write( sizeof( sampling ), &sampling ) );
        makeException( writer.write( sizeof( qId ), &qId ) );
        makeException( writer.write( sizeof( numBlocks ), &numBlocks ) );

        // Write blocks in raster order
        for( size_t j = 0; j < numBlocks; ++j )
        {
            int16_t *src = compBlocks[i].data() + j * 64;
            for( int k = 0; k < 64; ++k )
                makeException( writer.write( sizeof( src[k] ), &src[k] ) );
        }
    }
}

bool BlockGrouping::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

Scale::Scale( std::shared_ptr<JPEG> img, unsigned s, const PixelFormat &pfmt ) :
    Compression( s, pfmt ),
    image( std::move( img ) ),
    sof( image->findSingle<SegmentSOF>() )
{
    makeException( sof );
}

void Scale::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Scale::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader reader( source.link, source.bytes, fmt.offset );

    uint16_t widthBlocks = 0, heightBlocks = 0;
    makeException( reader.read( sizeof( widthBlocks ), &widthBlocks ) );
    makeException( reader.read( sizeof( heightBlocks ), &heightBlocks ) );

    uint8_t componentCount = 0;
    makeException( reader.read( sizeof( componentCount ), &componentCount ) );

    // Read per-component block arrays
    struct Component
    {
        uint8_t compId;
        uint8_t sampling;
        uint8_t qId;
        uint32_t numBlocks;
        std::vector<int16_t> blocks;
    };

    std::vector<Component> components;
    components.reserve( componentCount );

    for( uint8_t i = 0; i < componentCount; ++i )
    {
        Component c;
        makeException( reader.read( sizeof( c.compId ), &c.compId ) );
        makeException( reader.read( sizeof( c.sampling ), &c.sampling ) );
        makeException( reader.read( sizeof( c.qId ), &c.qId ) );
        makeException( reader.read( sizeof( c.numBlocks ), &c.numBlocks ) );

        c.blocks.resize( size_t( c.numBlocks ) * 64 );
        for( size_t k = 0; k < c.numBlocks * 64; ++k )
            makeException( reader.read( sizeof( c.blocks[k] ), &c.blocks[k] ) );

        components.push_back( std::move( c ) );
    }

    // Reconstruct per-component pixel planes (in pixels)
    // Compute maxH/maxV from sof to compute mcusX / mcusY, but BlockGrouping provided widthBlocks / heightBlocks = mcusX * maxH etc
    uint8_t maxH = 1, maxV = 1;
    for( auto &c : sof->components )
    {
        uint8_t H = ( c.samplingFactors >> 4 ) & 0x0F;
        uint8_t V = c.samplingFactors & 0x0F;
        maxH = std::max( maxH, H );
        maxV = std::max( maxV, V );
    }

    uint16_t imageW = sof->header.imageWidth;
    uint16_t imageH = sof->header.imageHeight;

    // For each component, compBlockW = mcusX * H, but widthBlocks = mcusX * maxH => mcusX = widthBlocks / maxH
    size_t mcusX = widthBlocks / maxH;
    size_t mcusY = heightBlocks / maxV;

    struct Plane
    {
        uint8_t compId;
        int w;
        int h;
        std::vector<int16_t> pixels;
    };
    std::vector<Plane> planes;
    planes.reserve( components.size() );

    for( auto const &c : components )
    {
        // Find SOF sampling for this comp
        int sofIdx = -1;
        for( size_t k = 0; k < sof->components.size(); ++k )
        {
            if( sof->components[k].componentId == c.compId )
            {
                sofIdx = int( k );
                break;
            }
        }

        makeException( sofIdx >= 0 );
        uint8_t H = ( sof->components[sofIdx].samplingFactors >> 4 ) & 0x0F;
        uint8_t V = sof->components[sofIdx].samplingFactors & 0x0F;

        int compBlockW = int( mcusX * H );
        int compBlockH = int( mcusY * V );
        int srcW = compBlockW * 8;
        int srcH = compBlockH * 8;

        Plane plane;
        plane.compId = c.compId;
        plane.w = srcW;
        plane.h = srcH;
        plane.pixels.assign( size_t( srcW ) * size_t( srcH ), 0 );

        // Fill blocks (blocks are in raster order)
        size_t nb = c.numBlocks;
        makeException( nb == size_t( compBlockW ) * size_t( compBlockH ) );
        for( size_t by = 0; by < ( size_t )compBlockH; ++by )
        {
            for( size_t bx = 0; bx < ( size_t )compBlockW; ++bx )
            {
                size_t blockIndex = by * compBlockW + bx;
                const int16_t *blk = c.blocks.data() + blockIndex * 64;

                // Copy 8x8
                for( int ry = 0; ry < 8; ++ry )
                {
                    int dstY = int( by * 8 + ry );
                    for( int rx = 0; rx < 8; ++rx )
                    {
                        int dstX = int( bx * 8 + rx );
                        int idx = dstY * srcW + dstX;
                        plane.pixels[idx] = blk[ry * 8 + rx];
                    }
                }
            }
        }

        planes.push_back( std::move( plane ) );
    }

    // Upsample each component's plane from (srcW, srcH) to (imageW, imageH) using bilinear interpolation and output the result

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( 2 + 2 + 1 + planes.size() * ( 1 + 1 + 2 * imageW * imageH ), fmt, destination );

    Writer writer( destination.link, destination.bytes, fmt.offset );

    makeException( writer.write( sizeof( imageW ), &imageW ) );
    makeException( writer.write( sizeof( imageH ), &imageH ) );

    uint8_t planeCount = planes.size();
    makeException( writer.write( sizeof( planeCount ), &planeCount ) );

    for( auto &plane : planes )
    {
        makeException( writer.write( sizeof( plane.compId ), &plane.compId ) );

        uint8_t elementSize = sizeof( int16_t );
        makeException( writer.write( sizeof( elementSize ), &elementSize ) );

        // Resize destination buffer and fill
        // We'll sample source as floating coordinates: sx = (x + 0.5) * srcW / imageW - 0.5

        double sxFactor = double( plane.w ) / double( imageW );
        double syFactor = double( plane.h ) / double( imageH );

        for( uint16_t y = 0; y < imageH; ++y )
        {
            double sy = ( ( double )y + 0.5 ) * syFactor - 0.5;

            if( sy < 0 )
                sy = 0;

            if( sy > plane.h - 1 )
                sy = plane.h - 1;

            int y0 = int( floor( sy ) );
            int y1 = std::min( y0 + 1, plane.h - 1 );

            double wy = sy - y0;

            for( uint16_t x = 0; x < imageW; ++x )
            {
                double sx = ( ( double )x + 0.5 ) * sxFactor - 0.5;

                if( sx < 0 )
                    sx = 0;

                if( sx > plane.w - 1 )
                    sx = plane.w - 1;

                int x0 = int( floor( sx ) );
                int x1 = std::min( x0 + 1, plane.w - 1 );

                double wx = sx - x0;

                double v00 = plane.pixels[y0 * plane.w + x0];
                double v10 = plane.pixels[y0 * plane.w + x1];
                double v01 = plane.pixels[y1 * plane.w + x0];
                double v11 = plane.pixels[y1 * plane.w + x1];

                double value = ( 1.0 - wx ) * ( 1.0 - wy ) * v00 + wx * ( 1.0 - wy ) * v10 + ( 1.0 - wx ) * wy * v01 + wx * wy * v11;

                int32_t v32 = int32_t( std::lround( value ) );
                makeException( std::numeric_limits<int16_t>::lowest() <= v32 && v32 <= std::numeric_limits<int16_t>::max() );

                int16_t v16 = ( int16_t )v32;
                makeException( writer.write( sizeof( v16 ), &v16 ) );
            }
        }
    }
}

bool Scale::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

YCbCrK::YCbCrK( std::shared_ptr<JPEG> img, unsigned s, const PixelFormat &pfmt ) :
    Compression( s, pfmt ),
    image( std::move( img ) )
{}

void YCbCrK::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void YCbCrK::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader reader( source.link, source.bytes, fmt.offset );

    uint16_t width = 0, height = 0;
    makeException( reader.read( sizeof( width ), &width ) );
    makeException( reader.read( sizeof( height ), &height ) );

    uint8_t componentCount = 0;
    makeException( reader.read( sizeof( componentCount ), &componentCount ) );

    // Read components into map componentId to samples
    struct Component
    {
        uint8_t id;
        std::vector<int16_t> samples;
    };
    std::vector<Component> components;
    components.reserve( componentCount );

    for( uint8_t i = 0; i < componentCount; ++i )
    {
        uint8_t componentId = 0, elementSize = 0;
        makeException( reader.read( sizeof( componentId ), &componentId ) );
        makeException( reader.read( sizeof( elementSize ), &elementSize ) );

        // We only support 2 here
        makeException( elementSize == 2 );

        Component c;
        c.id = componentId;
        c.samples.resize( size_t( width ) * size_t( height ) );
        for( size_t k = 0; k < c.samples.size(); ++k )
            makeException( reader.read( sizeof( c.samples[k] ), &c.samples[k] ) );

        components.push_back( std::move( c ) );
    }

    // Build mapping from compId to index
    auto findComponent = [&]( uint8_t id ) -> std::optional<size_t>
    {
        for( size_t i = 0; i < components.size(); ++i )
        {
            if( components[i].id == id )
                return i;
        }
        return {};
    };

    // Conversion:
    // If 3 components, Y, Cb, Cr. Typical component IDs: Y = 1, Cb = 2, Cr = 3. Map by id position
    // If 4 components, Y, Cb, Cr, K. Convert YCbCr to RGB then multiply by 1 - K

    size_t pixelCount = size_t( width ) * size_t( height );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( pixelCount * 3, fmt, destination );

    Writer writer( destination.link, destination.bytes, fmt.offset );

    auto clamp8 = []( int v ) -> uint8_t
    {
        if( v < 0 )
            return 0;

        if( v > 255 )
            return 255;

        return ( uint8_t )v;
    };

    if( 3 <= componentCount && componentCount <= 4 )
    {
        // Find which components correspond to Y, Cb, Cr
        // Naive: assume first is Y, second Cb, third Cr (or search ids 1/2/3)
        auto yIdx = findComponent( 1 );
        auto cbIdx = findComponent( 2 );
        auto crIdx = findComponent( 3 );
        if( !yIdx || !cbIdx || !crIdx )
        {
            // Fallback: use order as given
            yIdx = 0;
            cbIdx = 1;
            crIdx = 2;
        }

        auto &Y = components[*yIdx].samples;
        auto &Cb = components[*cbIdx].samples;
        auto &Cr = components[*crIdx].samples;

        int16_t *K = nullptr;
        if( componentCount >= 4 )
        {
            auto kIdx = findComponent( 4 );
            if( !kIdx )
                kIdx = 3;

            K = components[*kIdx].samples.data();
        }

        // Convert: inputs are int16_t before level shift
        for( size_t i = 0; i < pixelCount; ++i )
        {
            double y = double( Y[i] ) + 128.0;
            double cb = double( Cb[i] );
            double cr = double( Cr[i] );
            double k = 1.0;

            if( K )
                k -= ( K[i] + 128.0 ) / 255.0;

            double r = y + 1.402   * cr;
            double g = y - 0.344136 * cb - 0.714136 * cr;
            double b = y + 1.772   * cb;

            auto v = clamp8( int( Round( k * r ) ) );
            makeException( writer.write( sizeof( v ), &v ) );

            v = clamp8( int( Round( k * g ) ) );
            makeException( writer.write( sizeof( v ), &v ) );

            v = clamp8( int( Round( k * b ) ) );
            makeException( writer.write( sizeof( v ), &v ) );
        }

        return;
    }

    // Unexpected component layout
    makeException( false );
}

bool YCbCrK::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

CMYK::CMYK( std::shared_ptr<JPEG> img, unsigned s, const PixelFormat &pfmt ) :
    Compression( s, pfmt ),
    image( std::move( img ) )
{}

void CMYK::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void CMYK::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader reader( source.link, source.bytes, fmt.offset );

    uint16_t width = 0, height = 0;
    makeException( reader.read( sizeof( width ), &width ) );
    makeException( reader.read( sizeof( height ), &height ) );

    uint8_t componentCount = 0;
    makeException( reader.read( sizeof( componentCount ), &componentCount ) );

    struct Component
    {
        uint8_t id;
        std::vector<int16_t> samples;
    };
    std::vector<Component> components;
    components.reserve( componentCount );

    for( uint8_t i = 0; i < componentCount; ++i )
    {
        uint8_t componentId = 0, elementSize = 0;
        makeException( reader.read( sizeof( componentId ), &componentId ) );
        makeException( reader.read( sizeof( elementSize ), &elementSize ) );

        makeException( elementSize == 2 );

        Component c;
        c.id = componentId;
        c.samples.resize( size_t( width ) * size_t( height ) );

        for( size_t k = 0; k < c.samples.size(); ++k )
            makeException( reader.read( sizeof( c.samples[k] ), &c.samples[k] ) );

        components.push_back( std::move( c ) );
    }

    // Find C, M, Y, K indices (try by id, fallback to order)
    auto findComponent = [&]( uint8_t id ) -> std::optional<size_t>
    {
        for( size_t i = 0; i < components.size(); ++i )
        {
            if( components[i].id == id )
                return i;
        }
        return {};
    };

    makeException( components.size() == 4 );

    auto cIdx = findComponent( 1 );
    auto mIdx = findComponent( 2 );
    auto yIdx = findComponent( 3 );
    auto kIdx = findComponent( 4 );
    if( !cIdx || !mIdx || !yIdx || !kIdx )
    {
        cIdx = 0;
        mIdx = 1;
        yIdx = 2;
        kIdx = 3;
    }

    size_t pixelCount = size_t( width ) * size_t( height );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( pixelCount * 3, fmt, destination );

    Writer writer( destination.link, destination.bytes, fmt.offset );

    auto &C = components[*cIdx].samples;
    auto &M = components[*mIdx].samples;
    auto &Y = components[*yIdx].samples;
    auto &K = components[*kIdx].samples;

    auto clamp8 = []( int v ) -> uint8_t
    {
        if( v < 0 )
            return 0;

        if( v > 255 )
            return 255;

        return ( uint8_t )v;
    };

    for( size_t i = 0; i < pixelCount; ++i )
    {
        double c = ( double( C[i] ) + 128.0 ) / 255.0;
        double m = ( double( M[i] ) + 128.0 ) / 255.0;
        double y = ( double( Y[i] ) + 128.0 ) / 255.0;
        double k = ( double( K[i] ) + 128.0 ) / 255.0;

        // Convert CMYK to RGB:
        double r = ( 1.0 - c ) * ( 1.0 - k ) * 255.0;
        double g = ( 1.0 - m ) * ( 1.0 - k ) * 255.0;
        double b = ( 1.0 - y ) * ( 1.0 - k ) * 255.0;

        auto v = clamp8( int( Round( r ) ) );
        makeException( writer.write( sizeof( v ), &v ) );

        v = clamp8( int( Round( g ) ) );
        makeException( writer.write( sizeof( v ), &v ) );

        v = clamp8( int( Round( b ) ) );
        makeException( writer.write( sizeof( v ), &v ) );
    }
}

bool CMYK::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

static int extractColorModel( size_t size, const JPEG& image )
{
    const SegmentAdobe *adobe = image.findSingle<SegmentAdobe>();
    switch( size )
    {
    case 1:
        return 0;
    case 3:
        if( adobe )
        {
            switch( adobe->hdr.colorTransform )
            {
            case 0:
                return 1;
            case 1:
                return 2;
            default:
                makeException( false );
            }
        }
        return 2;
    case 4:
        if( adobe )
        {
            switch( adobe->hdr.colorTransform )
            {
            case 0:
                return 3;
            case 2:
                return 4;
            default:
                makeException( false );
            }
        }
        return 3;
    default:
        makeException( false );
    }
    return 0;
}

static void extractJpg( Format &fmt, ReaderBase &r )
{
    auto img = std::make_shared<JPEG>();
    auto& image = *img;

    image.read( r );

    auto sof = image.findSingle<SegmentSOF>();
    makeException( sof );

    fmt.w = sof->header.imageWidth;
    fmt.h = sof->header.imageHeight;

    if( auto dnl = image.findSingle<SegmentDNL>() )
        fmt.h = dnl->numberOfLines;

    auto size = sof->components.size();
    auto bits = sof->header.samplePrecision;

    fmt.clear();
    fmt.pad = 1;
    fmt.offset = 0;
    fmt.channels.push_back( { 'R', bits } );
    fmt.channels.push_back( { 'G', bits } );
    fmt.channels.push_back( { 'B', bits } );
    fmt.calculateBits();

    switch( extractColorModel( size, image ) )
    {
    case 0:
        fmt.clear();
        fmt.channels.push_back( { 'G', bits } );
        fmt.calculateBits();
        break;
    case 1:
        break;
    case 2:
        fmt.compression.push_front( std::make_shared<YCbCrK>( img, 0, fmt ) );
        fmt.clear();
        fmt.channels.push_back( { 'Y', bits } );
        fmt.channels.push_back( { 'B', bits } );
        fmt.channels.push_back( { 'R', bits } );
        fmt.calculateBits();
        break;
    case 3:
        fmt.compression.push_front( std::make_shared<CMYK>( img, 0, fmt ) );
        fmt.clear();
        fmt.channels.push_back( { 'C', bits } );
        fmt.channels.push_back( { 'M', bits } );
        fmt.channels.push_back( { 'Y', bits } );
        fmt.channels.push_back( { 'K', bits } );
        fmt.calculateBits();
        break;
    case 4:
        fmt.compression.push_front( std::make_shared<YCbCrK>( img, 0, fmt ) );
        fmt.clear();
        fmt.channels.push_back( { 'Y', bits } );
        fmt.channels.push_back( { 'B', bits } );
        fmt.channels.push_back( { 'R', bits } );
        fmt.channels.push_back( { 'K', bits } );
        fmt.calculateBits();
        break;
    default:
        makeException( false );
    }

    // SegmentICC contains data needed for color management

    auto sof0 = image.findSingle<SegmentSOF0>();
    auto dri = image.findSingle<SegmentDRI>();
    auto dht = image.find<SegmentDHT>();
    auto dac = image.find<SegmentDAC>();
    auto dqt = image.find<SegmentDQT>();
    auto sos = image.find<SegmentSOS>();

    makeException( sof0 && !dri && !dht.empty() && !dqt.empty() && !sos.empty() );

    fmt.compression.push_front( std::make_shared<Scale>( img, 0, fmt ) );
    fmt.compression.push_front( std::make_shared<BlockGrouping>( img, 0, fmt ) );
    fmt.compression.push_front( std::make_shared<DCT>( img, 0, fmt ) );
    fmt.compression.push_front( std::make_shared<Quantization>( img, 0, fmt ) );
    fmt.compression.push_front( std::make_shared<Huffman>( img, 0, fmt ) );
}

void makeJpg( const Reference &ref, Format &format, HeaderWriter *write )
{
    format.w = ref.w;
    format.h = ref.h;

    if( !write )
    {
        SimpleReader r( ref.link, ref.bytes );
        extractJpg( format, r );
        return;
    }

    // Not implemented
    makeException( false );
}
}
