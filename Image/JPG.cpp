#include "Image/JPG.h"

#include <unordered_map>
#include <algorithm>
#include <array>
#include <map>

#include <fstream>

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

        // read and discard in small chunks to avoid temporary large buffers
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

// Helper LE readers/writers used in inter-stage buffers

static void write_u32_le( std::vector<uint8_t>& out, uint32_t v )
{
    out.push_back( uint8_t( v & 0xFF ) );
    out.push_back( uint8_t( ( v >> 8 ) & 0xFF ) );
    out.push_back( uint8_t( ( v >> 16 ) & 0xFF ) );
    out.push_back( uint8_t( ( v >> 24 ) & 0xFF ) );
}

static uint32_t read_u32_le( const uint8_t* p )
{
    return uint32_t( p[0] ) | ( uint32_t( p[1] ) << 8 ) | ( uint32_t( p[2] ) << 16 ) | ( uint32_t( p[3] ) << 24 );
}

static void write_u16_le( std::vector<uint8_t>& out, uint16_t v )
{
    out.push_back( uint8_t( v & 0xFF ) );
    out.push_back( uint8_t( ( v >> 8 ) & 0xFF ) );
}

static uint16_t read_u16_le( const uint8_t* p )
{
    return uint16_t( p[0] ) | ( uint16_t( p[1] ) << 8 );
}

static void write_i32_le( std::vector<uint8_t>& out, int32_t v )
{
    write_u32_le( out, ( uint32_t )v );
}

static int32_t read_i32_le( const uint8_t* p )
{
    return ( int32_t )read_u32_le( p );
}

static void write_i16_le( std::vector<uint8_t>& out, int16_t v )
{
    write_u16_le( out, ( uint16_t )v );
}

static int16_t read_i16_le( const uint8_t* p )
{
    return ( int16_t )read_u16_le( p );
}

// ----------------------------- AAN integer IDCT -----------------------------
// Implementation based on standard AAN scaled integer algorithm
// Produces int32_t results
struct AAN_IDCT
{
    // AAN scaled constants scaled by 2^13 (8192) to maintain integer math
    static constexpr int SCALE = 13;
    static constexpr int ONE = 1 << SCALE;

    // Precomputed fixed multipliers (scaled)
    static const int32_t aanscales[8];

    // Perform AAN 1-D inverse on an 8-element vector (in place)
    static void idct1d( int32_t *v )
    {
        // Based on AAN algorithm (integer-friendly)

        // Stage 1
        int32_t x0 = v[0] + v[4];
        int32_t x1 = v[0] - v[4];
        int32_t x2 = ( v[2] * aanscales[2] ) >> SCALE;
        int32_t x3 = ( v[6] * aanscales[6] ) >> SCALE;
        int32_t x4 = v[1] + v[7];
        int32_t x5 = v[3] + v[5];
        int32_t x6 = v[1] - v[7];
        int32_t x7 = v[3] - v[5];

        // Stage 2
        int32_t t0 = x0 + x3;
        int32_t t3 = x0 - x3;
        int32_t t1 = x1 + x2;
        int32_t t2 = x1 - x2;

        // Stage 3
        int32_t s0 = t0 + t1;
        int32_t s1 = t0 - t1;
        int32_t s2 = t3 + t2;
        int32_t s3 = t3 - t2;

        // Odd part
        int32_t p0 = ( x4 * aanscales[1] ) >> SCALE;
        int32_t p1 = ( x5 * aanscales[3] ) >> SCALE;
        int32_t p2 = ( x6 * aanscales[5] ) >> SCALE;
        int32_t p3 = ( x7 * aanscales[7] ) >> SCALE;

        int32_t o0 = p0 + p1;
        int32_t o1 = p0 - p1;
        int32_t o2 = p2 + p3;
        int32_t o3 = p2 - p3;

        v[0] = s0 + o0;
        v[7] = s0 - o0;
        v[1] = s2 + o2;
        v[6] = s2 - o2;
        v[2] = s3 + o3;
        v[5] = s3 - o3;
        v[3] = s1 + o1;
        v[4] = s1 - o1;
    }

    static void idct8x8( const int32_t *in, int32_t *out )
    {
        // Work in 32-bit integers, two-pass algorithm (rows then columns)

        int32_t tmp[64];

        for( int r = 0; r < 8; ++r )
        {
            int32_t row[8];

            for( int i = 0; i < 8; ++i )
                row[i] = in[r * 8 + i];

            idct1d( row );

            for( int i = 0; i < 8; ++i )
                tmp[r * 8 + i] = row[i];
        }

        for( int c = 0; c < 8; ++c )
        {
            int32_t col[8];
            for( int i = 0; i < 8; ++i )
                col[i] = tmp[i * 8 + c];

            idct1d( col );

            for( int i = 0; i < 8; ++i )
                out[i * 8 + c] = ( col[i] + ( 1 << ( SCALE - 1 ) ) ) >> SCALE; // Final scale down with rounding
        }
    }
};

const int32_t AAN_IDCT::aanscales[8] =
{
    ( int32_t )( 1 << AAN_IDCT::SCALE ), // 1.0
    ( int32_t )round( 1.387039845 * ( 1 << AAN_IDCT::SCALE ) ), // c1
    ( int32_t )round( 1.306562965 * ( 1 << AAN_IDCT::SCALE ) ), // c2
    ( int32_t )round( 1.175875602 * ( 1 << AAN_IDCT::SCALE ) ), // c3
    ( int32_t )round( 1.0 * ( 1 << AAN_IDCT::SCALE ) ), // c4
    ( int32_t )round( 0.785694958 * ( 1 << AAN_IDCT::SCALE ) ), // c5
    ( int32_t )round( 0.541196100 * ( 1 << AAN_IDCT::SCALE ) ), // c6
    ( int32_t )round( 0.275899379 * ( 1 << AAN_IDCT::SCALE ) ) // c7
};

Huffman::Huffman( const SegmentSOF* sof_, const SegmentDRI* dri_, std::vector<const SegmentDHT*> dht_, std::vector<const SegmentSOS*> sos_, unsigned s, const PixelFormat &pfmt )
    : Compression( s, pfmt ), sof( sof_ ), dri( dri_ ), dht( std::move( dht_ ) ), sos( std::move( sos_ ) )
{
    makeException( sof && !dht.empty() && !sos.empty() );
}

void Huffman::decompress( const std::vector<uint8_t>&, std::vector<uint8_t>& output ) const
{
    struct Block
    {
        int32_t coefficients[64] = {0};
        uint8_t componentId = 0; // SOF componentId
        bool written = false; // Was this block written at least once
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
        size_t index = 0;
    };

    const SegmentSOS* currentSegment = nullptr;
    int Ss = 0, Se = 0, Ah = 0, Al = 0;
    Accumulator acc;

    // Key { tableClass ( 0 = DC, 1 = AC ), tableID ( 0..3 ) }
    std::map<std::pair<uint8_t, uint8_t>, HuffmanTable> huffmanTables;

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

    auto buildTable = []( const SegmentDHT::Table & t )
    {
        HuffmanTable table;
        table.symbols = t.symbols;

        // List of code lengths in symbol order
        std::vector<unsigned> huffsize;
        huffsize.reserve( 256 );
        for( unsigned l = 1; l <= 16; ++l )
        {
            uint8_t count = t.counts[l - 1];
            for( uint8_t i = 0; i < count; ++i )
                huffsize.push_back( l );
        }
        huffsize.push_back( 0 ); // Terminator

        // Validate symbol count
        size_t symbolsCount = huffsize.size() - 1;
        makeException( symbolsCount == table.symbols.size() );

        // List of codes in symbol order
        std::vector<unsigned> huffcode;
        huffcode.resize( symbolsCount );

        unsigned code = 0, p = 0;
        size_t size = huffsize[0];

        // Produce codes in increasing code length order
        while( huffsize[p] != 0 )
        {
            while( huffsize[p] == size )
            {
                huffcode[p] = code;
                ++p;
                ++code;

                // Code must fit in 'size' bits
                makeException( size == 0 || code <= ( 1u << size ) );
            }

            // After finishing codes of length 'size', shift left for next length
            if( huffsize[p] != 0 ) // Only shift, if there are more codes
            {
                code <<= 1;
                ++size;
                makeException( size <= 16 );
            }
        }

        p = 0;
        for( unsigned l = 1; l <= 16; ++l )
        {
            uint8_t count = t.counts[l - 1];
            if( count )
            {
                table.valPtr[l]  = p;
                table.minCode[l] = huffcode[p];

                p += count;
                table.maxCode[l] = huffcode[p - 1];
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

            bool okDC = huffmanTables.find( {0, dcid} ) != huffmanTables.end();
            bool okAC = huffmanTables.find( {1, acid} ) != huffmanTables.end();
            makeException( okDC && okAC );
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

    auto contains = []( const HuffmanTable * table, unsigned code, unsigned bits )
    {
        // Table does not exist
        if( !table )
            return false;

        // Invalid length
        if( bits <= 0 || 16 < bits )
            return false;

        unsigned mc = table->minCode[bits];
        unsigned xc = table->maxCode[bits];

        // Empty range
        if( mc > xc )
            return false;

        return mc <= code && code <= xc;
    };

    auto lookup = [&contains]( const HuffmanTable * table, unsigned code, unsigned bits )
    {
        makeException( contains( table, code, bits ) );

        unsigned minC = table->minCode[bits];
        unsigned maxC = table->maxCode[bits];

        unsigned index = table->valPtr[bits] + ( code - minC );
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

    auto getSymbol = [&]( Reader & reader, unsigned & bits, uint8_t & outSymbol, const HuffmanTable * table )
    {
        makeException( table );
        makeException( table->symbols.size() > 0 ); // Table must have symbols

        unsigned code = 0;
        bits = 0;

        // Compute max code length available in this table (1..16), 0 means no codes
        auto maxLenAvailable = [&]() -> int
        {
            int mx = 0;
            for( int l = 1; l <= 16; ++l )
            {
                // Treat only non-empty ranges as valid
                if( table->minCode[l] <= table->maxCode[l] )
                    mx = l;
            }
            return mx;
        };

        int maxLen = maxLenAvailable();
        makeException( maxLen > 0 ); // There must be at least one code in the table

        do
        {
            BitList bit;
            makeException( reader.read( 1, bit ) );

            code = ( code << 1 ) | ( unsigned )bit;
            bits++;

            // Huffman codes' maximum length is 16 bits
            makeException( maxLen <= 16 );
            makeException( bits <= maxLen );
        }
        while( !contains( table, code, bits ) );

        outSymbol = lookup( table, code, bits );
    };

    auto getAmplitude = [&]( Reader & reader, BitList category, int64_t& amplitude )
    {
        unsigned size = ( unsigned ) category;

        // Categories beyond 31 are impossible
        makeException( size <= 31 );

        if( size == 0 )
        {
            amplitude = 0;
            return;
        }

        BitList bits = 0;
        makeException( reader.read( size, bits ) );

        // Extension as per JPEG HUFF_EXTEND semantics
        if( bits < ( 1ULL << ( size - 1 ) ) )
            amplitude = bits - ( ( 1ULL << size ) - 1 );
        else
            amplitude = bits;

        // Apply successive approximation scaling (Al)
        if( amplitude < 0 )
            amplitude = -( ( int64_t )( ( uint64_t )( -amplitude ) << Al ) );
        else
            amplitude = ( int64_t )( ( uint64_t )( amplitude ) << Al );
    };

    // Helpers p1/m1 depend on Al (they will be recomputed when Al changes in prepare)
    auto p1_of = [&]( int Alv )
    {
        // keep reasonable limits
        makeException( Alv >= 0 && Alv < 31 );
        return ( 1 << Alv );
    };

    auto m1_of = [&]( int Alv )
    {
        makeException( Alv >= 0 && Alv < 31 );
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
                unsigned bits = 0;
                uint8_t symbol = 0;
                getSymbol( r, bits, symbol, acTbl );

                uint8_t run = ( symbol >> 4 ) & 0xF;
                uint8_t size = symbol & 0xF;

                // Maximum allowed run is 15 (4 bits)
                makeException( run <= 15 );
                makeException( size <= 31 );

                if( run == 0 && size == 0 )
                    break; // EOB

                if( run > 0 )
                {
                    // Sanity check to prevent large overflow
                    makeException( k + run <= 10000 );

                    k += run;
                }

                makeException( 0 <= k && k < 64 );

                int64_t amplitude = 0;
                getAmplitude( r, size, amplitude );

                int zz = ZigZag[k];
                makeException( ( unsigned )zz < 64 );

                blk.coefficients[zz] = ( int32_t )amplitude;
                blk.written = true;
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
                    int zz = ZigZag[k];
                    makeException( ( unsigned )zz < 64 );
                    if( blk.coefficients[zz] != 0 )
                    {
                        BitList b;
                        makeException( r.read( 1, b ) );
                        if( b )
                        {
                            if( ( blk.coefficients[zz] & p1 ) == 0 )
                            {
                                if( blk.coefficients[zz] >= 0 )
                                    blk.coefficients[zz] += p1;
                                else
                                    blk.coefficients[zz] += m1;
                                blk.written = true;
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
                unsigned bits = 0;
                uint8_t symbol = 0;
                getSymbol( r, bits, symbol, table );

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

                        int zz = ZigZag[k];
                        makeException( ( unsigned )zz < 64 );

                        if( blk.coefficients[zz] != 0 )
                        {
                            BitList corr;
                            makeException( r.read( 1, corr ) );
                            if( corr )
                            {
                                if( ( blk.coefficients[zz] & p1 ) == 0 )
                                {
                                    if( blk.coefficients[zz] >= 0 )
                                        blk.coefficients[zz] += p1;
                                    else
                                        blk.coefficients[zz] += m1;
                                    blk.written = true;
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
                        int zz = ZigZag[k];
                        makeException( ( unsigned )zz < 64 );
                        blk.coefficients[zz] = newval;
                        blk.written = true;
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

    auto addDC = [&]( Reader & r, Block & blk, auto & component )
    {
        const HuffmanTable* dcTbl = dcTable( component );
        makeException( dcTbl != nullptr );

        if( Ah == 0 )
        {
            // Initial DC pass: Huffman-coded category + amplitude
            unsigned bits = 0;
            uint8_t symbol = 0;
            getSymbol( r, bits, symbol, dcTbl );

            // DC categories are small (<= 31)
            makeException( symbol <= 31 );

            int64_t amplitude = 0;
            getAmplitude( r, symbol, amplitude );

            blk.coefficients[0] = amplitude;
            blk.written = true;
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
                blk.written = true;
            }
        }
    };

    acc.mcus.resize( mcuCount() );

    // Main loop
    for( auto segment : sos )
    {
        prepare( segment );

        // Compute expected blocks-per-mcu for this scan (sum H*V for components present in SOS)
        size_t expectedBlocksPerMcu = 0;
        for( auto &sc : segment->components )
        {
            // Find corresponding SOF component to read sampling
            auto it = std::find_if( sof->components.begin(), sof->components.end(), [&]( const auto & c )
            {
                return c.componentId == sc.componentId;
            } );
            makeException( it != sof->components.end() );

            uint8_t H = ( it->samplingFactors >> 4 ) & 0xF;
            uint8_t V = it->samplingFactors & 0x0F;
            makeException( H > 0 && V > 0 );
            expectedBlocksPerMcu += ( size_t ) H * ( size_t ) V;
        }
        makeException( expectedBlocksPerMcu > 0 );

        for( auto& slice : segment->entropy )
        {
            Reader reader( slice.data.data(), slice.data.size(), 0 );

            for( auto& mcu : acc.mcus )
            {
                // Ensure we don't overflow MCU block count unexpectedly
                makeException( mcu.blocks.size() <= expectedBlocksPerMcu );

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
                        // Sanity guard: cannot exceed expected element count per MCU
                        makeException( mcu.blocks.size() < expectedBlocksPerMcu );

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

    output.clear();

    // Count total blocks
    size_t totalBlocks = 0;
    for( auto &mcu : acc.mcus )
        totalBlocks += mcu.blocks.size();

    write_u32_le( output, ( uint32_t )totalBlocks );

    // For each MCU in scan order, append its blocks in the same order they were decoded
    for( auto &mcu : acc.mcus )
    {
        for( auto &blk : mcu.blocks )
        {
            output.push_back( blk.componentId );

            // Inverse zigzag
            int32_t natural[64] = {0};
            for( size_t i = 0; i < 64; ++i )
            {
                size_t zz = ZigZag[i];
                makeException( zz < 64 );
                natural[ zz ] = blk.coefficients[i];
            }

            for( size_t i = 0; i < 64; ++i )
                write_i32_le( output, natural[i] );
        }
    }
}

void Huffman::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Huffman::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    // Implement:
    makeException( false );
}

bool Huffman::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

Arithmetic::Arithmetic( const SegmentSOF* sof_, const SegmentDRI* dri_, std::vector<const SegmentDAC*> dac_, std::vector<const SegmentSOS*> sos_, unsigned s, const PixelFormat &pfmt )
    : Compression( s, pfmt ), sof( sof_ ), dri( dri_ ), dac( std::move( dac_ ) ), sos( std::move( sos_ ) )
{}

void Arithmetic::decompress( const std::vector<uint8_t>&, std::vector<uint8_t>& ) const
{
    // Not implemented
    makeException( false );
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

Quantization::Quantization( const SegmentSOF* sof_, std::vector<const SegmentDQT*> dqt_, unsigned s, const PixelFormat &pfmt )
    : Compression( s, pfmt ), sof( sof_ ), dqt( std::move( dqt_ ) )
{
    makeException( sof && !dqt.empty() );
}

void Quantization::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    // Build quantization table map: tableId to 64 integers
    std::array<int, 64> empty64;
    std::unordered_map<int, std::array<int, 64>> quantMap;

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
    const uint8_t* p = input.data();
    size_t remain = input.size();
    makeException( remain >= 4 );

    uint32_t count = read_u32_le( p );
    p += 4;
    remain -= 4;

    // Preserve same serialized layout: u32 count + entries (u8 compId + 64*i32)
    output.clear();

    write_u32_le( output, count );

    for( uint32_t bi = 0; bi < count; ++bi )
    {
        // Truncated
        makeException( remain >= 1 + 64 * 4 );

        uint8_t compId = *p++;
        remain -= 1;

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

        // Read 64 i32 coefficients, multiply by quant (int)
        int32_t coeffs[64];
        for( int i = 0; i < 64; ++i )
        {
            int32_t v = read_i32_le( p );
            p += 4;
            remain -= 4;

            int64_t deq = int64_t( v ) * int64_t( q[i] );

            // Clamp into int32 (shouldn't overflow under reasonable JPEG ranges)
            if( deq > INT32_MAX ) deq = INT32_MAX;
            if( deq < INT32_MIN ) deq = INT32_MIN;
            coeffs[i] = ( int32_t )deq;
        }

        // Write to output
        output.push_back( compId );
        for( int i = 0; i < 64; ++i )
            write_i32_le( output, coeffs[i] );
    }
}

void Quantization::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Quantization::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    // Implement:
    makeException( false );
}

bool Quantization::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

DCT::DCT( const SegmentSOF* sof_, unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt ), sof( sof_ )
{
    makeException( sof );
}

void DCT::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    const uint8_t* p = input.data();
    size_t remain = input.size();
    makeException( remain >= 4 );

    uint32_t count = read_u32_le( p );
    p += 4;
    remain -= 4;

    output.clear();

    write_u32_le( output, count );

    int32_t in[64], outblk[64];
    for( uint32_t bi = 0; bi < count; ++bi )
    {
        makeException( remain >= 1 + 64 * 4 );

        uint8_t compId = *p++;
        remain -= 1;

        for( int i = 0; i < 64; ++i )
        {
            in[i] = read_i32_le( p );
            p += 4;
            remain -= 4;
        }

        // Apply AAN integer IDCT (input in natural row-major order)
        AAN_IDCT::idct8x8( in, outblk );

        // Output: keep i32 spatial samples (before level shift)
        output.push_back( compId );
        for( int i = 0; i < 64; ++i )
            write_i32_le( output, outblk[i] );
    }
}

void DCT::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void DCT::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    // Implement:
    makeException( false );
}

bool DCT::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

BlockGrouping::BlockGrouping( const SegmentSOF* sof_, unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt ), sof( sof_ )
{
    makeException( sof );
}

void BlockGrouping::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    const uint8_t* p = input.data();
    size_t remain = input.size();
    makeException( remain >= 4 );

    uint32_t count = read_u32_le( p );
    p += 4;
    remain -= 4;

    struct InBlock
    {
        uint8_t compId;
        std::array<int32_t, 64> v;
    };
    std::vector<InBlock> blocks;
    blocks.reserve( count );

    // Read all input blocks
    for( uint32_t i = 0; i < count; ++i )
    {
        makeException( remain >= 1 + 64 * 4 );

        InBlock ib;
        ib.compId = *p++;
        remain -= 1;

        for( int j = 0; j < 64; ++j )
        {
            ib.v[j] = read_i32_le( p );
            p += 4;
            remain -= 4;
        }

        blocks.push_back( std::move( ib ) );
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
    size_t compCount = sof->header.numComponents;
    std::vector<size_t> compBlockW( compCount ), compBlockH( compCount );
    for( size_t ci = 0; ci < compCount; ++ci )
    {
        uint8_t H = ( sof->components[ci].samplingFactors >> 4 ) & 0x0F;
        uint8_t V = sof->components[ci].samplingFactors & 0x0F;
        compBlockW[ci] = mcusX * H;
        compBlockH[ci] = mcusY * V;
    }

    // Prepare per-component containers (blocks in raster order)
    // Use i16 store (clamp)

    std::vector<std::vector<int16_t>> compBlocks( compCount );
    for( size_t ci = 0; ci < compCount; ++ci )
    {
        size_t numBlocks = compBlockW[ci] * compBlockH[ci];
        compBlocks[ci].assign( numBlocks * 64, 0 );
    }

    // Rebuild clean per-component block grids
    for( size_t ci = 0; ci < compCount; ++ci )
        std::fill( compBlocks[ci].begin(), compBlocks[ci].end(), int16_t( 0 ) );

    size_t inIndex = 0;
    for( size_t my = 0; my < mcusY; ++my )
    {
        for( size_t mx = 0; mx < mcusX; ++mx )
        {
            // Per-component counters for this MCU
            std::vector<size_t> placed( compCount, 0 );

            for( size_t sofci = 0; sofci < compCount; ++sofci )
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
                        for( size_t k = 0; k < compCount; ++k )
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
                            if( sval > INT16_MAX ) sval = INT16_MAX;
                            if( sval < INT16_MIN ) sval = INT16_MIN;
                            dest[k] = ( int16_t )sval;
                        }

                        ++inIndex;
                    }
                }
            } // SOF components
        }
    }

    // Now serialize BlockGrouping output
    output.clear();

    uint16_t widthBlocks = uint16_t( mcusX * maxH );
    uint16_t heightBlocks = uint16_t( mcusY * maxV );
    write_u16_le( output, widthBlocks );
    write_u16_le( output, heightBlocks );

    // Components: number of SOF components
    output.push_back( ( uint8_t ) compCount );

    for( size_t ci = 0; ci < compCount; ++ci )
    {
        uint8_t compId = sof->components[ci].componentId;
        uint8_t sampling = sof->components[ci].samplingFactors;
        uint8_t qId = sof->components[ci].quantTableId;
        uint32_t numBlocks = uint32_t( compBlockW[ci] * compBlockH[ci] );

        output.push_back( compId );
        output.push_back( sampling );
        output.push_back( qId );
        write_u32_le( output, numBlocks );

        // Write blocks in raster order
        for( size_t bi = 0; bi < numBlocks; ++bi )
        {
            int16_t *src = compBlocks[ci].data() + bi * 64;
            for( int k = 0; k < 64; ++k )
                write_i16_le( output, src[k] );
        }
    }
}

void BlockGrouping::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void BlockGrouping::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    // Implement:
    makeException( false );
}

bool BlockGrouping::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

Scale::Scale( const SegmentSOF* sof_, unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt ), sof( sof_ )
{
    makeException( sof );
}

// Input: BlockGrouping output (blocks by component)
// Output: u16 width, u16 height, u8 components; for each component: u8 compId, u8 elemSize(2) then width*height i16 LE samples
void Scale::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    const uint8_t* p = input.data();
    size_t remain = input.size();

    makeException( remain >= 2 + 2 + 1 );

    uint16_t widthBlocks = read_u16_le( p );
    p += 2;
    remain -= 2;

    uint16_t heightBlocks = read_u16_le( p );
    p += 2;
    remain -= 2;

    uint8_t components = *p++;
    remain -= 1;

    // Read per-component block arrays
    struct CompIn
    {
        uint8_t compId;
        uint8_t sampling;
        uint8_t qId;
        uint32_t numBlocks;
        std::vector<int16_t> blocks; // numBlocks * 64
    };
    std::vector<CompIn> comps;
    comps.reserve( components );

    for( uint8_t ci = 0; ci < components; ++ci )
    {
        makeException( remain >= 1 + 1 + 1 + 4 );

        CompIn c;
        c.compId = *p++;
        c.sampling = *p++;
        c.qId = *p++;
        c.numBlocks = read_u32_le( p );
        p += 4;
        remain -= 1 + 1 + 1 + 4;

        size_t need = size_t( c.numBlocks ) * 64 * 2;
        makeException( remain >= need );

        c.blocks.resize( size_t( c.numBlocks ) * 64 );
        for( size_t k = 0; k < c.numBlocks * 64; ++k )
        {
            c.blocks[k] = read_i16_le( p );
            p += 2;
        }
        remain -= need;
        comps.push_back( std::move( c ) );
    }

    // Reconstruct per-component pixel planes (in pixels)
    // Compute maxH/maxV from sof to compute mcusX/mcusY, but BlockGrouping provided widthBlocks/heightBlocks = mcusX*maxH etc.
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

    struct CompPixel
    {
        uint8_t compId;
        int W;
        int H;
        std::vector<int16_t> pixels;
    };
    std::vector<CompPixel> compPixels;
    compPixels.reserve( comps.size() );

    for( auto const &ci : comps )
    {
        // Find SOF sampling for this comp
        int sofIdx = -1;
        for( size_t k = 0; k < sof->components.size(); ++k )
        {
            if( sof->components[k].componentId == ci.compId )
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

        CompPixel cp;
        cp.compId = ci.compId;
        cp.W = srcW;
        cp.H = srcH;
        cp.pixels.assign( size_t( srcW ) * size_t( srcH ), 0 );

        // fill blocks (blocks are in raster order)
        size_t nb = ci.numBlocks;
        makeException( nb == size_t( compBlockW ) * size_t( compBlockH ) );
        for( size_t by = 0; by < ( size_t )compBlockH; ++by )
        {
            for( size_t bx = 0; bx < ( size_t )compBlockW; ++bx )
            {
                size_t blockIndex = by * compBlockW + bx;
                const int16_t *blk = ci.blocks.data() + blockIndex * 64;
                // Copy 8x8
                for( int ry = 0; ry < 8; ++ry )
                {
                    int dstY = int( by * 8 + ry );
                    for( int rx = 0; rx < 8; ++rx )
                    {
                        int dstX = int( bx * 8 + rx );
                        int idx = dstY * srcW + dstX;
                        cp.pixels[idx] = blk[ry * 8 + rx];
                    }
                }
            }
        }

        compPixels.push_back( std::move( cp ) );
    }

    // Upsample each component from (srcW, srcH) to (imageW, imageH) using bilinear interpolation
    output.clear();

    write_u16_le( output, imageW );
    write_u16_le( output, imageH );
    output.push_back( ( uint8_t ) compPixels.size() );

    for( auto &cp : compPixels )
    {
        // Element size = 2 (i16)
        output.push_back( cp.compId );
        output.push_back( 2u );

        // Resize destination buffer and fill
        // We'll sample source as floating coordinates: sx = (x + 0.5) * srcW / imageW - 0.5
        double sxFactor = double( cp.W ) / double( imageW );
        double syFactor = double( cp.H ) / double( imageH );

        for( uint16_t y = 0; y < imageH; ++y )
        {
            double sy = ( ( double )y + 0.5 ) * syFactor - 0.5;
            if( sy < 0 ) sy = 0;
            if( sy > cp.H - 1 ) sy = cp.H - 1;
            int y0 = int( floor( sy ) );
            int y1 = std::min( y0 + 1, cp.H - 1 );
            double wy = sy - y0;

            for( uint16_t x = 0; x < imageW; ++x )
            {
                double sx = ( ( double )x + 0.5 ) * sxFactor - 0.5;
                if( sx < 0 ) sx = 0;
                if( sx > cp.W - 1 ) sx = cp.W - 1;
                int x0 = int( floor( sx ) );
                int x1 = std::min( x0 + 1, cp.W - 1 );
                double wx = sx - x0;

                double v00 = cp.pixels[y0 * cp.W + x0];
                double v10 = cp.pixels[y0 * cp.W + x1];
                double v01 = cp.pixels[y1 * cp.W + x0];
                double v11 = cp.pixels[y1 * cp.W + x1];

                double val = ( 1.0 - wx ) * ( 1.0 - wy ) * v00 + wx * ( 1.0 - wy ) * v10 + ( 1.0 - wx ) * wy * v01 + wx * wy * v11;

                // Clamp to int16
                int32_t iv = int32_t( std::lround( val ) );
                if( iv > INT16_MAX ) iv = INT16_MAX;
                if( iv < INT16_MIN ) iv = INT16_MIN;
                write_i16_le( output, ( int16_t )iv );
            }
        }
    }
}

void Scale::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void Scale::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    // Implement:
    makeException( false );
}

bool Scale::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

YCbCrK::YCbCrK( unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt )
{}

// Input: Scale output (width,height,components, for each: compId, elemSize(2), width*height i16 samples)
// Output: width,height, channels(3), bitDepth(8), interleaved RGB8
void YCbCrK::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    const uint8_t* p = input.data();
    size_t remain = input.size();

    makeException( remain >= 2 + 2 + 1 );

    uint16_t width = read_u16_le( p );
    p += 2;
    remain -= 2;

    uint16_t height = read_u16_le( p );
    p += 2;
    remain -= 2;

    uint8_t components = *p++;
    remain -= 1;

    // Read components into map compId -> samples
    struct Comp
    {
        uint8_t id;
        std::vector<int16_t> samples;
    };
    std::vector<Comp> comps;
    comps.reserve( components );

    for( uint8_t ci = 0; ci < components; ++ci )
    {
        makeException( remain >= 2 );

        uint8_t compId = *p++;
        uint8_t elemSize = *p++;
        remain -= 2;

        // We only support 2 here
        makeException( elemSize == 2 );

        size_t need = size_t( width ) * size_t( height ) * 2;
        makeException( remain >= need );

        Comp c;
        c.id = compId;
        c.samples.resize( size_t( width ) * size_t( height ) );
        for( size_t k = 0; k < c.samples.size(); ++k )
        {
            c.samples[k] = read_i16_le( p );
            p += 2;
        }
        remain -= need;
        comps.push_back( std::move( c ) );
    }

    // Build mapping from compId to index
    auto findComp = [&]( uint8_t id ) -> int
    {
        for( size_t i = 0; i < comps.size(); ++i )
        {
            if( comps[i].id == id )
                return int( i );
        }
        return -1;
    };

    // Decide conversion:
    // If 1 component, treat as grayscale
    // If 3 components, treat as Y,Cb,Cr (component IDs typical: 1=Y,2=Cb,3=Cr). We'll map by id position.
    // If 4 components, treat as Y,Cb,Cr,K -> convert YCbCr->RGB then apply K as multiplication.

    output.clear();

    // Write header width,height, channels, bitDepth
    write_u16_le( output, width );
    write_u16_le( output, height );
    uint8_t outChannels = 3;
    uint8_t outBitDepth = 8;
    output.push_back( outChannels );
    output.push_back( outBitDepth );

    // Helper to clamp
    auto clamp8 = []( int v ) -> uint8_t
    {
        if( v < 0 )
            return 0;

        if( v > 255 )
            return 255;

        return ( uint8_t )v;
    };

    size_t pixCount = size_t( width ) * size_t( height );

    if( components == 1 )
    {
        // Grayscale, replicate to RGB
        auto &s = comps[0].samples;
        for( size_t i = 0; i < pixCount; ++i )
        {
            int v = int( s[i] ) + 128; // level shift
            output.push_back( clamp8( v ) );
            output.push_back( clamp8( v ) );
            output.push_back( clamp8( v ) );
        }
        return;
    }

    if( components >= 3 )
    {
        // Find which components correspond to Y, Cb, Cr
        // Naive: assume first is Y, second Cb, third Cr (or search ids 1/2/3)
        int yIdx = findComp( 1 );
        int cbIdx = findComp( 2 );
        int crIdx = findComp( 3 );
        if( yIdx < 0 || cbIdx < 0 || crIdx < 0 )
        {
            // Fallback: use order as given
            yIdx = 0;
            cbIdx = 1;
            crIdx = 2;
        }

        auto &Y = comps[yIdx].samples;
        auto &Cb = comps[cbIdx].samples;
        auto &Cr = comps[crIdx].samples;

        // Convert: inputs are i16 possibly before level shift -> add 128
        for( size_t i = 0; i < pixCount; ++i )
        {
            double y = double( Y[i] ) + 128.0;
            double cb = double( Cb[i] ) + 128.0;
            double cr = double( Cr[i] ) + 128.0;

            double r = y + 1.402   * ( cr - 128.0 );
            double g = y - 0.344136 * ( cb - 128.0 ) - 0.714136 * ( cr - 128.0 );
            double b = y + 1.772   * ( cb - 128.0 );

            output.push_back( clamp8( int( std::lround( r ) ) ) );
            output.push_back( clamp8( int( std::lround( g ) ) ) );
            output.push_back( clamp8( int( std::lround( b ) ) ) );
        }

        // If there's a 4th component (K), apply it as multiply (optional)
        if( components >= 4 )
        {
            int kIdx = findComp( 4 );
            if( kIdx >= 0 )
            {
                // Apply K: current output is in RGB interleaved, modify in-place
                auto &K = comps[kIdx].samples;
                for( size_t i = 0; i < pixCount; ++i )
                {
                    uint8_t kr = output[i * 3 + 0];
                    uint8_t kg = output[i * 3 + 1];
                    uint8_t kb = output[i * 3 + 2];

                    double kf = ( double( K[i] ) + 128.0 ) / 255.0;

                    uint8_t rr = clamp8( int( std::lround( ( 1.0 - kf ) * kr ) ) );
                    uint8_t gg = clamp8( int( std::lround( ( 1.0 - kf ) * kg ) ) );
                    uint8_t bb = clamp8( int( std::lround( ( 1.0 - kf ) * kb ) ) );
                    output[i * 3 + 0] = rr;
                    output[i * 3 + 1] = gg;
                    output[i * 3 + 2] = bb;
                }
            }
        }

        return;
    }

    // Fallback: unexpected components, produce black
    for( size_t i = 0; i < pixCount; ++i )
    {
        output.push_back( 0 );
        output.push_back( 0 );
        output.push_back( 0 );
    }
}

void YCbCrK::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void YCbCrK::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    // Implement:
    makeException( false );
}

bool YCbCrK::equals( const Compression &other ) const
{
    // Implement:
    makeException( false );
    return false;
}

CMYK::CMYK( unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt ) {}

// Input: Scale output (components), expects 4 components C,M,Y,K. Output same layout as YCbCrK: RGB interleaved 8-bit
void CMYK::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    const uint8_t* p = input.data();
    size_t remain = input.size();
    makeException( remain >= 2 + 2 + 1 );

    uint16_t width = read_u16_le( p );
    p += 2;
    remain -= 2;

    uint16_t height = read_u16_le( p );
    p += 2;
    remain -= 2;

    uint8_t components = *p++;
    remain -= 1;

    struct Comp
    {
        uint8_t id;
        std::vector<int16_t> samples;
    };
    std::vector<Comp> comps;
    comps.reserve( components );

    for( uint8_t ci = 0; ci < components; ++ci )
    {
        makeException( remain >= 2 );

        uint8_t compId = *p++;
        uint8_t elemSize = *p++;
        remain -= 2;

        makeException( elemSize == 2 );

        size_t need = size_t( width ) * size_t( height ) * 2;
        makeException( remain >= need );

        Comp c;
        c.id = compId;
        c.samples.resize( size_t( width ) * size_t( height ) );
        for( size_t k = 0; k < c.samples.size(); ++k )
        {
            c.samples[k] = read_i16_le( p );
            p += 2;
        }
        remain -= need;
        comps.push_back( std::move( c ) );
    }

    // Find C, M, Y, K indices (try by id, fallback to order)
    auto findComp = [&]( uint8_t id ) -> int
    {
        for( size_t i = 0; i < comps.size(); ++i )
        {
            if( comps[i].id == id )
                return int( i );
        };
        return -1;
    };

    int cIdx = findComp( 1 );
    int mIdx = findComp( 2 );
    int yIdx = findComp( 3 );
    int kIdx = findComp( 4 );
    if( cIdx < 0 || mIdx < 0 || yIdx < 0 || kIdx < 0 )
    {
        makeException( comps.size() >= 4 );

        // Fallback to first four in order
        cIdx = 0;
        mIdx = 1;
        yIdx = 2;
        kIdx = 3;
    }

    output.clear();

    size_t pixCount = size_t( width ) * size_t( height );

    write_u16_le( output, width );
    write_u16_le( output, height );
    output.push_back( 3 ); // Channels
    output.push_back( 8 ); // Bit depth

    auto &C = comps[cIdx].samples;
    auto &M = comps[mIdx].samples;
    auto &Y = comps[yIdx].samples;
    auto &K = comps[kIdx].samples;

    auto clamp8 = []( int v ) -> uint8_t
    {
        if( v < 0 )
            return 0;

        if( v > 255 )
            return 255;

        return ( uint8_t )v;
    };

    for( size_t i = 0; i < pixCount; ++i )
    {
        double c = ( double( C[i] ) + 128.0 ) / 255.0;
        double m = ( double( M[i] ) + 128.0 ) / 255.0;
        double y = ( double( Y[i] ) + 128.0 ) / 255.0;
        double k = ( double( K[i] ) + 128.0 ) / 255.0;

        // Convert CMYK to RGB:
        double r = ( 1.0 - c ) * ( 1.0 - k ) * 255.0;
        double g = ( 1.0 - m ) * ( 1.0 - k ) * 255.0;
        double b = ( 1.0 - y ) * ( 1.0 - k ) * 255.0;

        output.push_back( clamp8( int( std::lround( r ) ) ) );
        output.push_back( clamp8( int( std::lround( g ) ) ) );
        output.push_back( clamp8( int( std::lround( b ) ) ) );
    }
}

void CMYK::compress( Format &, const Reference &, Reference & )
{
    // Not implemented
    makeException( false );
}

void CMYK::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    // Implement:
    makeException( false );
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
    JPEG image;
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
    fmt.channels.reserve( size );

    // SegmentICC contains data needed for color management

    auto sof0 = image.findSingle<SegmentSOF0>();
    auto dri = image.findSingle<SegmentDRI>();
    auto dht = image.find<SegmentDHT>();
    auto dac = image.find<SegmentDAC>();
    auto dqt = image.find<SegmentDQT>();
    auto sos = image.find<SegmentSOS>();

    makeException( sof0 && !dht.empty() && !dqt.empty() && !sos.empty() );

    fmt.clear();
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
        fmt.compression.push_front( std::make_shared<YCbCrK>( fmt.bufferSize(), fmt ) );
        fmt.clear();
        fmt.channels.push_back( { 'Y', bits } );
        fmt.channels.push_back( { 'B', bits } );
        fmt.channels.push_back( { 'R', bits } );
        fmt.calculateBits();
        break;
    case 3:
        fmt.compression.push_front( std::make_shared<CMYK>( fmt.bufferSize(), fmt ) );
        fmt.clear();
        fmt.channels.push_back( { 'C', bits } );
        fmt.channels.push_back( { 'M', bits } );
        fmt.channels.push_back( { 'Y', bits } );
        fmt.channels.push_back( { 'K', bits } );
        fmt.calculateBits();
        break;
    case 4:
        fmt.compression.push_front( std::make_shared<YCbCrK>( fmt.bufferSize(), fmt ) );
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

    std::vector<uint8_t> input, output;

    Huffman h( sof0, dri, dht, sos, fmt.bufferSize(), fmt );
    h.decompress( input, output );
    input = std::move( output );

    Quantization q( sof0, dqt, fmt.bufferSize(), fmt );
    q.decompress( input, output );
    input = std::move( output );

    DCT dct( sof0, fmt.bufferSize(), fmt );
    dct.decompress( input, output );
    input = std::move( output );

    BlockGrouping bg( sof0, fmt.bufferSize(), fmt );
    bg.decompress( input, output );
    input = std::move( output );

    Scale sc( sof0, fmt.bufferSize(), fmt );
    sc.decompress( input, output );
    input = std::move( output );

    YCbCrK cc( fmt.bufferSize(), fmt );
    cc.decompress( input, output );

    uint16_t width = read_u16_le( output.data() );
    uint16_t height = read_u16_le( output.data() + 2 );
    uint8_t channels = output[4];
    uint8_t bitDepth = output[5];

    makeException( channels == 3 && bitDepth == 8 );

    FILE* out = fopen( "output/out.bmp", "wb" );
    makeException( out );

#pragma pack(push, 1)

    struct BITMAPFILEHEADER
    {
        uint16_t bfType;
        uint32_t bfSize;
        uint16_t bfReserved1;
        uint16_t bfReserved2;
        uint32_t bfOffBits;
    };

    struct BITMAPINFOHEADER
    {
        uint32_t biSize;
        int32_t  biWidth;
        int32_t  biHeight;
        uint16_t biPlanes;
        uint16_t biBitCount;
        uint32_t biCompression;
        uint32_t biSizeImage;
        int32_t  biXPelsPerMeter;
        int32_t  biYPelsPerMeter;
        uint32_t biClrUsed;
        uint32_t biClrImportant;
    };

    struct CIEXYZ
    {
        int32_t ciexyzX;
        int32_t ciexyzY;
        int32_t ciexyzZ;
    };

    struct CIEXYZTRIPLE
    {
        CIEXYZ ciexyzRed;
        CIEXYZ ciexyzGreen;
        CIEXYZ ciexyzBlue;
    };

    struct BITMAPV4HEADER
    {
        BITMAPINFOHEADER info;
        uint32_t bV4RedMask;
        uint32_t bV4GreenMask;
        uint32_t bV4BlueMask;
        uint32_t bV4AlphaMask;
        uint32_t bV4CSType;
        CIEXYZTRIPLE bV4Endpoints;
        uint32_t bV4GammaRed;
        uint32_t bV4GammaGreen;
        uint32_t bV4GammaBlue;
    };

#pragma pack(pop)

    BITMAPFILEHEADER fh;
    BITMAPV4HEADER v4;

    fh.bfType = 0x4D42;
    fh.bfSize = sizeof( fh ) + sizeof( v4 ) + 4 * fmt.w * fmt.h;
    fh.bfReserved1 = 0;
    fh.bfReserved2 = 0;
    fh.bfOffBits = sizeof( fh ) + sizeof( v4 );
    fwrite( &fh, 1, sizeof( fh ), out );

    clear( &v4, sizeof( v4 ) );
    v4.info.biSize = sizeof( v4 );
    v4.info.biWidth = fmt.w;
    v4.info.biHeight = fmt.h;
    v4.info.biPlanes = 1;
    v4.info.biBitCount = 32;
    v4.info.biCompression = 3;
    v4.info.biSizeImage = 0;
    v4.info.biClrUsed = 0;
    v4.info.biClrImportant = 0;
    v4.bV4RedMask   = 0x00ff0000;
    v4.bV4GreenMask = 0x0000ff00;
    v4.bV4BlueMask  = 0x000000ff;
    v4.bV4AlphaMask = 0xff000000;
    v4.bV4CSType = 0x73524742;
    fwrite( &v4, 1, sizeof( v4 ), out );

    uint8_t alpha = 255;
    auto area = width * height;
    for( uint32_t i = 0; i < area; ++i )
    {
        fwrite( output.data() + 6 + i * 3, 1, 3, out );
        fwrite( &alpha, 1, sizeof( alpha ), out );
    }

    fclose( out );

    makeException( false );
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
