#include "Image/JPG.h"

#include <algorithm>
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

    // Now read entropy-coded data:

    entropy.clear();
    nextMarker.reset();

    auto slice = &entropy.emplace_back();

    while( true )
    {
        uint8_t b;
        if( !readByte( b ) )
            return true; // Accepting partial entropyData

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
            if( c >= 0xD0 && c <= 0xD7 )
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

static const int ZigZag[64] =
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
// Produces i32 results
// We'll clamp to i16 for storage when bit depth <= 8
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
        // stage 1
        int32_t x0 = v[0] + v[4];
        int32_t x1 = v[0] - v[4];
        int32_t x2 = ( v[2] * aanscales[2] ) >> SCALE;
        int32_t x3 = ( v[6] * aanscales[6] ) >> SCALE;
        int32_t x4 = v[1] + v[7];
        int32_t x5 = v[3] + v[5];
        int32_t x6 = v[1] - v[7];
        int32_t x7 = v[3] - v[5];

        // stage 2
        int32_t t0 = x0 + x3;
        int32_t t3 = x0 - x3;
        int32_t t1 = x1 + x2;
        int32_t t2 = x1 - x2;

        // stage 3
        int32_t s0 = t0 + t1;
        int32_t s1 = t0 - t1;
        int32_t s2 = t3 + t2;
        int32_t s3 = t3 - t2;

        // odd part
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

    static void idct8x8( const int32_t in[64], int32_t out[64] )
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
        int minCode[17];
        int maxCode[17];
        int valPtr[17];
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

    auto buildTable = []( const SegmentDHT::Table & t ) -> HuffmanTable
    {
        HuffmanTable ht;
        ht.symbols = t.symbols;

        int code = 0;
        int k = 0;

        for( int i = 1; i <= 16; i++ )
        {
            uint8_t count = t.counts[i - 1];
            if( count == 0 )
            {
                ht.minCode[i] = -1;
                ht.maxCode[i] = -1;
            }
            else
            {
                ht.valPtr[i]  = k;
                ht.minCode[i] = code;
                code += count;
                ht.maxCode[i] = code - 1;
                code <<= 1; // Shift left for next bit length
                k += count;
            }
        }

        return ht;
    };

    // Parse all DHT segments
    for( auto& d : dht )
    {
        for( auto& table : d->tables )
        {
            uint8_t tc = ( table.tc_th >> 4 ) & 0x0F;
            uint8_t th = table.tc_th & 0x0F;
            huffmanTables[ {tc, th}] = buildTable( table );
        }
    }

    auto contains = [this]( auto table, unsigned code, unsigned bits ) -> bool
    {
        // Table does not exist
        if( !table )
            return false;

        // Invalid length
        if( bits == 0 || bits > 16 )
            return false;

        int mc = table->minCode[bits];
        int xc = table->maxCode[bits];

        if( mc < 0 || xc < 0 )
            return false;

        if( ( unsigned )mc <= code && code <= ( unsigned )xc )
            return true;

        return false;
    };

    auto lookup = [this]( auto table, unsigned code, unsigned bits )
    {
        // Return the symbol from table assuming contains(...) already true

        makeException( table );
        makeException( bits > 0 && bits <= 16 );

        int minC = table->minCode[bits];
        makeException( minC >= 0 );

        int index = table->valPtr[bits] + ( int )( code - ( unsigned ) minC );
        makeException( index >= 0 && index < ( int ) table->symbols.size() );

        return table->symbols[index];
    };

    auto findTable = [&]( uint8_t tc, uint8_t th ) -> const HuffmanTable *
    {
        auto key = std::make_pair( tc, th );

        auto i = huffmanTables.find( key );
        if( i == huffmanTables.end() )
            return nullptr;

        return &i->second;
    };

    auto dcTable = [&]( int componentIndex )
    {
        auto sel = currentSegment->components[componentIndex].huffmanSelectors;
        uint8_t th = ( sel >> 4 ) & 0x0F; // DC id
        return findTable( 0, th );
    };

    auto acTable = [&]( int componentIndex )
    {
        auto sel = currentSegment->components[componentIndex].huffmanSelectors;
        uint8_t th = sel & 0x0F; // AC id
        return findTable( 1, th );
    };

    auto mcuGeometry = [&]()
    {
        uint8_t maxH = 0, maxV = 0;
        for( auto& c : sof->components )
        {
            uint8_t H = ( c.samplingFactors >> 4 ) & 0xF;
            uint8_t V = c.samplingFactors & 0x0F;
            maxH = std::max( maxH, H );
            maxV = std::max( maxV, V );
        }
        return std::pair{ maxH, maxV };
    };

    auto mcuCount = [&]()
    {
        auto [maxH, maxV] = mcuGeometry();
        uint16_t width  = sof->header.imageWidth;
        uint16_t height = sof->header.imageHeight;

        size_t mcuX = ( width  + ( 8 * maxH - 1 ) ) / ( 8 * maxH );
        size_t mcuY = ( height + ( 8 * maxV - 1 ) ) / ( 8 * maxV );
        return mcuX * mcuY;
    };

    auto prepare = [&]( const SegmentSOS * s )
    {
        currentSegment = s;
        Ss = s->spectralStart;
        Se = s->spectralEnd;
        Ah = ( s->successiveApproximation >> 4 ) & 0x0F;
        Al = s->successiveApproximation & 0x0F;

        if( acc.mcus.empty() )
        {
            acc.mcus.resize( mcuCount() );
            auto [maxH, maxV] = mcuGeometry();

            for( auto& m : acc.mcus )
            {
                for( auto& sc : s->components )
                {
                    // Find corresponding SOF component to read sampling
                    auto it = std::find_if( sof->components.begin(), sof->components.end(), [&]( const auto & c )
                    {
                        return c.componentId == sc.componentId;
                    } );
                    makeException( it != sof->components.end() );

                    uint8_t H = ( it->samplingFactors >> 4 ) & 0xF;
                    uint8_t V = it->samplingFactors & 0x0F;

                    size_t blocks = H * V;
                    for( size_t i = 0; i < blocks; ++i )
                        m.blocks.emplace_back();
                }
            }
        }
    };

    auto getSymbol = [&]( Reader & reader, unsigned & bits, uint8_t & outSymbol, auto table )
    {
        unsigned code = 0;
        bits = 0;
        do
        {
            BitList bit;
            reader.read( 1, bit );
            code = ( code << 1 ) | bit;
            bits++;
            makeException( bits <= 16 ); // Huffman codes max 16 bits
        }
        while( !contains( table, code, bits ) );

        outSymbol = lookup( table, code, bits );
    };

    auto getAmplitude = [&]( Reader & reader, BitList category, int64_t& amplitude )
    {
        unsigned size = ( unsigned ) category;
        if( size == 0 )
        {
            amplitude = 0;
            return;
        }

        BitList bits;
        reader.read( size, bits );
        if( bits < ( 1ULL << ( size - 1 ) ) )
            amplitude = bits - ( ( 1ULL << size ) - 1 );
        else
            amplitude = bits;
    };

    auto addDC = [&]( Reader & r, Block & blk, int component )
    {
        unsigned bits = 0;
        uint8_t symbol = 0;
        getSymbol( r, bits, symbol, dcTable( component ) );

        int64_t amplitude = 0;
        getAmplitude( r, symbol, amplitude );

        blk.coefficients[0] = ( int16_t )( amplitude << Al );
    };

    auto addAC = [&]( Reader & r, Block & blk, int component )
    {
        for( int k = Ss; k <= Se; ++k )
        {
            unsigned bits = 0;
            uint8_t symbol = 0;
            getSymbol( r, bits, symbol, acTable( component ) );

            uint8_t run = ( symbol >> 4 ) & 0xF;
            uint8_t size = symbol & 0xF;

            if( run == 0 && size == 0 )
                break; // EOB

            int64_t amplitude = 0;
            getAmplitude( r, size, amplitude );
            blk.coefficients[k] = ( int16_t )( amplitude << Al );
        }
    };

    // Main loop
    for( auto segment : sos )
    {
        prepare( segment );

        for( auto& slice : segment->entropy )
        {
            Reader reader( slice.data.data(), slice.data.size(), 0 );

            for( auto& mcu : acc.mcus )
            {
                size_t blkIndex = 0;
                for( size_t ci = 0; ci < segment->components.size(); ++ci )
                {
                    // Count blocks for this component

                    auto it = std::find_if( sof->components.begin(), sof->components.end(), [&]( const auto & c )
                    {
                        return c.componentId == segment->components[ci].componentId;
                    } );

                    uint8_t H = ( it->samplingFactors >> 4 ) & 0xF;
                    uint8_t V = it->samplingFactors & 0x0F;
                    size_t blocks = H * V;

                    for( size_t b = 0; b < blocks; ++b )
                    {
                        Block& blk = mcu.blocks[blkIndex++];

                        if( Ss == 0 )
                            addDC( reader, blk, ( int )ci );
                        else
                            addAC( reader, blk, ( int )ci );
                    }
                }
            }
        }
    }

    // TODO: serialize mcus into `output`
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
    // Implement:
    makeException( false );
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
    // Implement:
    makeException( false );
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
    // Implement:
    makeException( false );
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

void Scale::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    // Implement:
    makeException( false );
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

void YCbCrK::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    // Implement:
    makeException( false );
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

void CMYK::decompress( const std::vector<uint8_t>& input, std::vector<uint8_t>& output ) const
{
    // Implement:
    makeException( false );
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
    for( uint16_t i = 0; i < area; ++i )
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
