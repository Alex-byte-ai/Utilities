#include "Image/PNG.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <zlib.h>
#pragma GCC diagnostic pop

#include "Image/PixelIO.h"

namespace ImageConvert
{
#define PNG_GRAYSCALE 0
#define PNG_TRUECOLOR 2
#define PNG_INDEXED 3
#define PNG_GRAYSCALE_ALPHA 4
#define PNG_TRUECOLOR_ALPHA 6

#define PNG_NONE 0
#define PNG_SUB 1
#define PNG_UP 2
#define PNG_AVERAGE 3
#define PNG_PAETH 4

// Ensure structures are packed without padding
#pragma pack(push, 1)

// A generic PNG chunk header
// Every chunk starts with a 4-byte length and a 4-byte type
struct PNGChunkHeader
{
    uint32_t length; // Length of the chunk's data in bytes (big-endian)
    char type[4];    // Chunk type (e.g., "IHDR")

    bool is( const char *string ) const;
    void set( const char *string );
};

// 13 bytes the IHDR chunk data
struct PNGIHDRData
{
    uint32_t width;            // Image width in pixels (big-endian)
    uint32_t height;           // Image height in pixels (big-endian)
    uint8_t bitDepth;          // Bit depth per channel
    uint8_t colorType;         // Color type (e.g., PNG_GRAYSCALE, PNG_TRUECOLOR, etc.)
    uint8_t compressionMethod; // Compression method (PNG always uses 0)
    uint8_t filterMethod;      // Filter method (PNG always uses 0)
    uint8_t interlaceMethod;   // Interlace method (0 for no interlace, 1 for Adam7 interlacing)
};

#pragma pack(pop)

struct PNGChunk
{
    PNGChunkHeader meta;
    std::vector<uint8_t> data;
    uint32_t crc;

    bool read( ReaderBase &r, const std::function<bool( const PNGChunkHeader & )> &include = nullptr );
    bool write( WriterBase &w ) const;

    void updateCrc();

    uint32_t calculateCrc() const;
    unsigned size() const;
};

// Verifies a PNG signature
bool PNGSignature::read( ReaderBase &r )
{
    uint8_t sgntr[8];
    if( !r.read( sizeof( sgntr ), sgntr ) )
        return false;
    return compare( sgntr, signature, sizeof( signature ) );
}

// Writes a PNG signature
bool PNGSignature::write( WriterBase &w ) const
{
    return w.write( sizeof( signature ), signature );
}

bool PNGChunkHeader::is( const char *string ) const
{
    return stringLength( string ) == sizeof( type ) && compare( type, string, sizeof( type ) );
}

void PNGChunkHeader::set( const char *string )
{
    makeException( stringLength( string ) == sizeof( type ) );
    copy( type, string, sizeof( type ) );
}

// Skips content, if and only if, include returns false
bool PNGChunk::read( ReaderBase &r, const std::function<bool( const PNGChunkHeader & )> &include )
{
    if( !r.read( sizeof( meta.length ), &meta.length ) )
        return false;
    meta.length = swapBe32( meta.length );

    if( !r.read( sizeof( meta.type ), meta.type ) )
        return false;

    uint8_t *pointer = nullptr;
    if( !include || include( meta ) )
    {
        data.resize( meta.length );
        pointer = data.data();
    }

    makeException( r.read( meta.length, pointer ) ); // Chunk is incorrect, if it claims to occupy more space, than allocated

    if( !r.read( sizeof( crc ), &crc ) )
        return false;
    crc = swapBe32( crc );

    makeException( !pointer || crc == calculateCrc() );
    return true;
}

bool PNGChunk::write( WriterBase &w ) const
{
    auto t = swapBe32( meta.length );
    if( !w.write( sizeof( t ), &t ) )
        return false;

    if( !w.write( sizeof( meta.type ), meta.type ) )
        return false;

    makeException( w.write( meta.length, data.data() ) ); // Chunk is incorrect, if it claims to occupy more space, than allocated

    t = swapBe32( crc );
    if( !w.write( sizeof( t ), &t ) )
        return false;

    return true;
}

void PNGChunk::updateCrc()
{
    crc = calculateCrc();
}

uint32_t PNGChunk::calculateCrc() const
{
    // Start with an initial CRC value of 0
    uint32_t result = crc32( 0L, nullptr, 0 );

    // Update CRC with the chunk type
    result = crc32( result, ( const Bytef * )meta.type, sizeof( meta.type ) );

    // Update CRC with the chunk data
    result = crc32( result, ( const Bytef * )data.data(), meta.length );
    return result;
}

unsigned PNGChunk::size() const
{
    return sizeof( meta ) + meta.length + sizeof( crc );
}

FracturePng::FracturePng( unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt )
{}

void FracturePng::compress( Format &fmt, const Reference &source, Reference &destination )
{
    makeException( fmt.compression.front().get() == this );

    Reader r( source.link, source.bytes, fmt.offset );

    copy( fmt );
    fmt.offset = 0;
    size = fmt.bufferSize( this );
    fmt.clear();

    constexpr unsigned maxChunkSize = 64 * 1024;
    size += ( ( size + maxChunkSize - 1 ) / maxChunkSize + 1 ) * ( sizeof( PNGChunkHeader ) + sizeof( PNGChunk::crc ) );

    sync( fmt, destination );

    Writer w( destination.link, destination.bytes, fmt.offset );

    PNGChunk chunk;
    unsigned chunkSize;

    chunk.meta.set( "IDAT" );
    while( ( chunkSize = r.bytesLeft( maxChunkSize ) ) > 0 )
    {
        chunk.meta.length = chunkSize;
        chunk.data.resize( chunkSize );
        r.read( chunkSize, chunk.data.data() );
        chunk.updateCrc();
        makeException( chunk.write( w ) );
    }

    chunk.meta.set( "IEND" );
    chunk.meta.length = 0;
    chunk.data.resize( 0 );
    chunk.updateCrc();
    makeException( chunk.write( w ) );
}

void FracturePng::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader r( source.link, source.bytes, fmt.offset );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( fmt, destination );

    Writer w( destination.link, destination.bytes, fmt.offset );

    PNGChunk chunk;
    while( chunk.read( r ) )
    {
        if( chunk.meta.is( "IDAT" ) )
        {
            makeException( w.write( chunk.meta.length, chunk.data.data() ) );
        }
        else if( chunk.meta.is( "IEND" ) )
        {
            break;
        }
    }
}

bool FracturePng::equals( const Compression &other ) const
{
    if( dynamic_cast<const FracturePng *>( &other ) )
        return this->Compression::operator==( other );
    return false;
};

ZlibPng::ZlibPng( unsigned s, const PixelFormat &pfmt ) : Compression( s, pfmt )
{}

void ZlibPng::compress( Format &fmt, const Reference &source, Reference &destination )
{
    makeException( fmt.compression.front().get() == this );

    auto srcSize = source.bytes - fmt.offset;
    auto srcData = ( uint8_t * )source.link + fmt.offset;

    copy( fmt );
    fmt.offset = 0;
    fmt.clear();

    std::vector<uint8_t> outBuffer( srcSize + 1000 ); // ???

    z_stream strm = {};
    makeException( deflateInit( &strm, Z_BEST_COMPRESSION ) == Z_OK );

    strm.avail_in = srcSize;
    strm.next_in = srcData;
    strm.avail_out = outBuffer.size();
    strm.next_out = outBuffer.data();
    makeException( deflate( &strm, Z_FINISH ) == Z_STREAM_END );
    deflateEnd( &strm );

    outBuffer.resize( outBuffer.size() - strm.avail_out );
    size = outBuffer.size();
    sync( fmt, destination );

    ::copy( destination.link, outBuffer.data(), size );
}

void ZlibPng::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    z_stream strm = {};
    strm.avail_in = source.bytes - fmt.offset;
    strm.next_in = ( Bytef * )( ( const uint8_t * )source.link + fmt.offset );
    makeException( inflateInit( &strm ) == Z_OK );

    fmt.offset = 0;
    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( fmt, destination );

    std::vector<uint8_t> outBuffer( fmt.bufferSize() + 1000 ); // ???
    strm.avail_out = outBuffer.size();
    strm.next_out = outBuffer.data();

    auto result = inflate( &strm, Z_FINISH );
    makeException( result == Z_STREAM_END );
    inflateEnd( &strm );

    outBuffer.resize( outBuffer.size() - strm.avail_out );
    ::copy( destination.link, outBuffer.data(), destination.bytes );
}

bool ZlibPng::equals( const Compression &other ) const
{
    if( dynamic_cast<const ZlibPng *>( &other ) )
        return this->Compression::operator==( other );
    return false;
};

FilterAndInterlacePng::Step::Step( unsigned pass )
{
    startX = passStart[pass][0];
    startY = passStart[pass][1];
    incX   = passInc[pass][0];
    incY   = passInc[pass][1];
}

unsigned FilterAndInterlacePng::Step::x( unsigned origX ) const
{
    return startX + incX * origX;
}

unsigned FilterAndInterlacePng::Step::y( unsigned origY ) const
{
    return startY + incY * origY;
}

FilterAndInterlacePng::Size::Size( unsigned w, unsigned h )
{
    scanline = w;
    number = h;
}

FilterAndInterlacePng::Size::Size( const Step &step, unsigned w, unsigned h )
{
    scanline = ( w > step.startX ) ? ( ( w - step.startX + step.incX - 1 ) / step.incX ) : 0; // Scanline size in pixels
    number = ( h > step.startY ) ? ( ( h - step.startY + step.incY - 1 ) / step.incY ) : 0;
}

unsigned FilterAndInterlacePng::Size::lineBytes( unsigned bits ) const
{
    return 1 + ( scanline * bits + 7 ) / 8;
}

unsigned FilterAndInterlacePng::Size::bytes( unsigned bits ) const
{
    return number * lineBytes( bits );
}

bool FilterAndInterlacePng::Size::empty() const
{
    return scanline <= 0 || number <= 0;
}

int FilterAndInterlacePng::paethPredictor( int a, int b, int c )
{
    int p = a + b - c;
    int pa = Abs( p - a );
    int pb = Abs( p - b );
    int pc = Abs( p - c );
    if( pa <= pb && pa <= pc )
        return a;
    if( pb <= pc )
        return b;
    return c;
}

unsigned FilterAndInterlacePng::scoreCandidate( const std::vector<BitList> &candidate )
{
    unsigned score = 0;
    for( auto v : candidate )
    {
        int8_t diff = ( uint8_t )v;
        score += Abs( diff );
    }
    return score;
}

// Use empty 'previous', if it's a first line
std::vector<BitList> FilterAndInterlacePng::applyFilter(
    const std::vector<BitList> &line, const std::vector<BitList> &previous, unsigned filterType, bool apply ) const
{
    size_t width = line.size();
    std::vector<BitList> result( width );

    const auto &orig = apply ? line : result;

    auto pixelBytes = ( bits + 7 ) / 8;

    auto sub = [&apply]( BitList a, BitList b )
    {
        makeException( a <= 255 );
        makeException( b <= 255 );
        if( apply )
            b = 0x100 - b;
        return ( a + b ) & 0xff;
    };

    switch( filterType )
    {
    case PNG_NONE:
        result = line;
        break;
    case PNG_SUB:
        for( size_t i = 0; i < width; ++i )
        {
            BitList left = i >= pixelBytes ? orig[i - pixelBytes] : 0;
            result[i] = sub( line[i], left );
        }
        break;
    case PNG_UP:
        for( size_t i = 0; i < width; ++i )
        {
            BitList up = !previous.empty() ? previous[i] : 0;
            result[i] = sub( line[i], up );
        }
        break;
    case PNG_AVERAGE:
        for( size_t i = 0; i < width; ++i )
        {
            int left = i >= pixelBytes ? orig[i - pixelBytes] : 0;
            int up = !previous.empty() ? previous[i] : 0;
            auto avg = ( left + up ) / 2;
            result[i] = sub( line[i], avg );
        }
        break;
    case PNG_PAETH:
        for( size_t i = 0; i < width; ++i )
        {
            int left = i >= pixelBytes ? orig[i - pixelBytes] : 0;
            int up = !previous.empty() ? previous[i] : 0;
            int upLeft = i >= pixelBytes && !previous.empty() ? previous[i - pixelBytes] : 0;
            auto paeth = paethPredictor( left, up, upLeft );
            result[i] = sub( line[i], paeth );
        }
        break;
    default:
        makeException( false );
    }
    return result;
}

FilterAndInterlacePng::FilterAndInterlacePng( bool i, int width, int height, const PixelFormat &pfmt )
    : Compression( 0, pfmt ), interlaced( i ), w( width ), h( height )
{
    calculateSize();
}

FilterAndInterlacePng::FilterAndInterlacePng( const FilterAndInterlacePng &other )
    : Compression( other.size, other ), interlaced( other.interlaced ), w( other.w ), h( other.h )
{}

void FilterAndInterlacePng::calculateSize()
{
    size = 0;
    if( interlaced )
    {
        // Iterate over the 7 Adam7 passes
        for( unsigned pass = 0; pass < 7; ++pass )
        {
            Step passStep( pass );
            Size passSize( passStep, w, h );

            if( passSize.empty() )
                continue;

            size += passSize.bytes( bits );
        }
    }
    else
    {
        Size passSize( w, h );
        size += passSize.bytes( bits );
    }
}

void FilterAndInterlacePng::compress( Format &fmt, const Reference &source, Reference &destination )
{
    makeException( fmt.compression.front().get() == this );

    auto fmtSrc = fmt;
    PixelReader sourcePixelReader( fmtSrc, source );
    fmtSrc.offset = 0;

    unsigned width = Abs( fmt.w );
    unsigned height = Abs( fmt.h );
    w = width;
    h = height;
    calculateSize();

    copy( fmt );
    fmt.offset = 0;
    fmt.clear();

    sync( fmtSrc, destination );

    PixelWriter destinationPixelWriter( fmtSrc, destination );
    Reader destinationReader( destination.link, destination.bytes, fmt.offset );
    Writer destinationWriter( destination.link, destination.bytes, fmt.offset );

    auto read = [&]( BitList & value )
    {
        makeException( destinationReader.read( 8, value ) );
    };

    auto write = [&]( BitList value )
    {
        makeException( destinationWriter.write( 8, value ) );
    };

    std::vector<std::vector<Pixel>> image( height, std::vector<Pixel>( width ) );
    for( unsigned y = 0; y < height; ++y )
    {
        for( unsigned x = 0; x < width; ++x )
        {
            makeException( sourcePixelReader.getPixelLn( image[y][x] ) );
        }
    }

    auto putPass = [&]( const Size & passSize, const std::function<bool( unsigned &x, unsigned &y )> &position )
    {
        auto bytes = passSize.lineBytes( bits ) - 1;
        auto number = passSize.number;

        auto padding = 8 * bytes - bits * passSize.scanline;
        for( unsigned py = 0; py < passSize.number; ++py )
        {
            makeException( destinationPixelWriter.write( 8, ( BitList )0 ) );
            for( unsigned px = 0; px < passSize.scanline; ++px )
            {
                auto x = px;
                auto y = py;
                if( position( x, y ) )
                    makeException( destinationPixelWriter.putPixel( image[y][x] ) );
            }
            makeException( destinationPixelWriter.write( padding, ( BitList )0 ) );
        }

        std::vector<BitList> previous, line( bytes );

        while( number > 0 )
        {
            BitList bestFilter;
            read( bestFilter );
            for( unsigned i = 0; i < bytes; ++i )
                read( line[i] );

            // Compute candidate filtered lines
            const unsigned numFilters = 5;
            std::vector<std::vector<BitList>> candidates( numFilters );
            std::vector<unsigned> scores( numFilters, 0 );

            for( unsigned filter = 0; filter < numFilters; ++filter )
            {
                candidates[filter] = applyFilter( line, previous, filter, true );
                scores[filter] = scoreCandidate( candidates[filter] );
            }

            // Choose the filter with the lowest score
            bestFilter = 0;
            unsigned bestScore = scores[0];
            for( unsigned f = 1; f < numFilters; ++f )
            {
                if( scores[f] < bestScore )
                {
                    bestScore = scores[f];
                    bestFilter = f;
                }
            }

            write( bestFilter );
            for( unsigned i = 0; i < bytes; ++i )
                write( candidates[bestFilter][i] );

            previous = line;
            --number;
        }
    };

    if( interlaced )
    {
        for( unsigned pass = 0; pass < 7; ++pass )
        {
            Step passStep( pass );
            Size passSize( passStep, width, height );

            if( passSize.empty() )
                continue;

            putPass( passSize, [&]( unsigned & x, unsigned & y )
            {
                x = passStep.x( x );
                y = passStep.y( y );
                return x < width && y < height;
            } );
        }
    }
    else
    {
        Size passSize( width, height );
        putPass( passSize, [&]( unsigned &, unsigned & )
        {
            return true;
        } );
    }
}

void FilterAndInterlacePng::decompress( Format &fmt, const Reference &source, Reference &destination ) const
{
    makeException( fmt.compression.front().get() == this );

    Reader sourceReader( source.link, source.bytes, fmt.offset );

    fmt.offset = 0;

    Reference unfilterSource;
    unfilterSource.fill();
    sync( fmt, unfilterSource );
    Writer unfilterSourceWriter( unfilterSource.link, unfilterSource.bytes, fmt.offset );

    fmt.compression.pop_front();
    fmt.copy( *this );
    sync( fmt, destination );

    PixelReader unfilterSourcePixelReader( fmt, unfilterSource );
    PixelWriter destinationPixelWriter( fmt, destination );

    auto read = [&]( BitList & value )
    {
        makeException( sourceReader.read( 8, value ) );
    };

    auto write = [&]( BitList value )
    {
        makeException( unfilterSourceWriter.write( 8, value ) );
    };

    unsigned width = Abs( fmt.w );
    unsigned height = Abs( fmt.h );

    std::vector<std::vector<Pixel>> image( height, std::vector<Pixel>( width ) );

    auto getPass = [&]( const Size & passSize, const std::function<bool( unsigned &x, unsigned &y )> &position )
    {
        auto bytes = passSize.lineBytes( bits ) - 1;
        auto number = passSize.number;

        std::vector<BitList> previous, line( bytes );

        while( number > 0 )
        {
            BitList filter;
            read( filter );
            for( unsigned i = 0; i < bytes; ++i )
                read( line[i] );

            line = applyFilter( line, previous, filter, false );

            write( 0 );
            for( unsigned i = 0; i < bytes; ++i )
                write( line[i] );

            previous = line;
            --number;
        }

        auto padding = 8 * bytes - bits * passSize.scanline;
        for( unsigned py = 0; py < passSize.number; ++py )
        {
            BitList filter, remainder;
            makeException( unfilterSourcePixelReader.read( 8, filter ) );
            for( unsigned px = 0; px < passSize.scanline; ++px )
            {
                auto x = px;
                auto y = py;
                if( position( x, y ) )
                    makeException( unfilterSourcePixelReader.getPixel( image[y][x] ) );
            }
            makeException( unfilterSourcePixelReader.read( padding, remainder ) );
        }
    };

    if( interlaced )
    {
        for( unsigned pass = 0; pass < 7; ++pass )
        {
            Step passStep( pass );
            Size passSize( passStep, width, height );

            if( passSize.empty() )
                continue;

            getPass( passSize, [&]( unsigned & x, unsigned & y )
            {
                x = passStep.x( x );
                y = passStep.y( y );
                return x < width && y < height;
            } );
        }
    }
    else
    {
        Size passSize( width, height );
        getPass( passSize, [&]( unsigned &, unsigned & )
        {
            return true;
        } );
    }

    for( unsigned y = 0; y < height; ++y )
    {
        for( unsigned x = 0; x < width; ++x )
        {
            makeException( destinationPixelWriter.putPixelLn( image[y][x] ) );
        }
    }
}

bool FilterAndInterlacePng::equals( const Compression &other ) const
{
    if( auto fip = dynamic_cast<const FilterAndInterlacePng *>( &other ) )
        return this->Compression::operator==( other ) &&
               interlaced == fip->interlaced &&
               w == fip->w &&
               h == fip->h;
    return false;
};

static void extractPng( Format &fmt, ReaderBase &r )
{
    PNGSignature sgntr;
    makeException( sgntr.read( r ) );

    // Read IHDR chunk.
    PNGChunk ihdrChunk;
    makeException( ihdrChunk.read( r ) );

    PNGIHDRData ihdr;
    makeException( ihdrChunk.meta.length == sizeof( ihdr ) && ihdrChunk.meta.is( "IHDR" ) );
    copy( &ihdr, ihdrChunk.data.data(), sizeof( ihdr ) );

    fmt.w = swapBe32( ihdr.width );
    fmt.h = swapBe32( ihdr.height );

    auto include = []( const PNGChunkHeader & meta )
    {
        return meta.is( "PLTE" ) || meta.is( "tRNS" );
    };

    // Extracting meta data from other chunks and calculating data size
    PNGChunk chunk;
    unsigned volume = 0, chunks = 0;
    std::optional<PNGChunk> plte, trns;
    while( chunk.read( r, include ) )
    {
        chunks += chunk.size();
        if( chunk.meta.is( "IDAT" ) )
        {
            volume += chunk.meta.length;
        }
        else if( chunk.meta.is( "PLTE" ) )
        {
            makeException( !plte );
            plte = std::move( chunk );
        }
        else if( chunk.meta.is( "tRNS" ) )
        {
            makeException( !trns );
            trns = std::move( chunk );
        }
    }

    if( ihdr.colorType == PNG_TRUECOLOR || ihdr.colorType == PNG_TRUECOLOR_ALPHA )
        plte.reset();

    makeException( plte.has_value() == ( ihdr.colorType == PNG_INDEXED ) );
    makeException( !trns.has_value() || ( ihdr.colorType != PNG_GRAYSCALE_ALPHA && ihdr.colorType != PNG_TRUECOLOR_ALPHA ) );

    fmt.clear();
    fmt.pad = 1;
    fmt.bits = ihdr.bitDepth;
    fmt.offset = sizeof( sgntr ) + ihdrChunk.size();

    switch( ihdr.colorType )
    {
    case PNG_GRAYSCALE:
        makeException( fmt.bits == 1 || fmt.bits == 2 || fmt.bits == 4 || fmt.bits == 8 || fmt.bits == 16 );
        fmt.channels.push_back( { 'G', fmt.bits } );
        if( trns )
            fmt.channels.push_back( { 'A', fmt.bits } );
        break;
    case PNG_TRUECOLOR:
        makeException( fmt.bits == 8 || fmt.bits == 16 );
        fmt.channels.push_back( { 'R', fmt.bits } );
        fmt.channels.push_back( { 'G', fmt.bits } );
        fmt.channels.push_back( { 'B', fmt.bits } );
        if( trns )
            fmt.channels.push_back( { 'A', fmt.bits } );
        break;
    case PNG_INDEXED:
        makeException( fmt.bits == 1 || fmt.bits == 2 || fmt.bits == 4 || fmt.bits == 8 );
        fmt.channels.push_back( { 'R', 8 } );
        fmt.channels.push_back( { 'G', 8 } );
        fmt.channels.push_back( { 'B', 8 } );
        if( trns )
            fmt.channels.push_back( { 'A', 8 } );
        break;
    case PNG_GRAYSCALE_ALPHA:
        makeException( fmt.bits == 8 || fmt.bits == 16 );
        fmt.channels.push_back( { 'G', fmt.bits } );
        fmt.channels.push_back( { 'A', fmt.bits } );
        break;
    case PNG_TRUECOLOR_ALPHA:
        makeException( fmt.bits == 8 || fmt.bits == 16 );
        fmt.channels.push_back( { 'R', fmt.bits } );
        fmt.channels.push_back( { 'G', fmt.bits } );
        fmt.channels.push_back( { 'B', fmt.bits } );
        fmt.channels.push_back( { 'A', fmt.bits } );
        break;
    default:
        makeException( false );
    }

    fmt.calculateBits();

    if( plte )
    {
        unsigned alphaBytes = trns ? 1 : 0;
        unsigned alphaNumber = trns ? trns->meta.length / alphaBytes : 0;

        makeException( fmt.bits > alphaBytes * 8 );

        unsigned colorBytes = fmt.bits / 8 - alphaBytes;
        unsigned colorNumber = plte->meta.length / colorBytes;
        makeException( plte->meta.length % colorBytes == 0 );

        auto palette = std::make_shared<Palette>( 0, fmt );

        fmt.clear();
        fmt.channels.push_back( { '#', ihdr.bitDepth } );
        fmt.calculateBits();
        palette->size = fmt.bufferSize();

        auto paletteChannel = plte->data.data();
        auto alphaChannel = trns ? trns->data.data() : nullptr;
        for( unsigned i = 0; i < colorNumber; ++i )
        {
            Pixel pixel;
            uint8_t channel;
            for( unsigned j = 0; j < colorBytes; ++j )
            {
                copy( &channel, paletteChannel++, sizeof( channel ) );
                pixel.push_back( channel );
            }
            if( trns )
            {
                if( alphaNumber > 0 )
                {
                    --alphaNumber;
                    copy( &channel, alphaChannel++, sizeof( channel ) );
                    pixel.push_back( channel );
                }
                else
                {
                    pixel.push_back( 255 );
                }
            }
            palette->samples.push_back( pixel );
        }
        makeException( alphaNumber == 0 );

        fmt.compression.push_front( palette );

        std::optional<Pixel> p;
        fmt.compression.push_front( std::make_shared<Misc>( fmt.bufferSize(), false, false, p, fmt ) );
    }
    else
    {
        if( trns )
        {
            Pixel pixel;

            if( ihdr.colorType == PNG_GRAYSCALE )
            {
                makeException( trns->data.size() * 8 == ihdr.bitDepth );
                if( ihdr.bitDepth == 8 )
                {
                    uint8_t p;
                    copy( &p, trns->data.data(), sizeof( p ) );
                    pixel.push_back( p );
                }
                else if( ihdr.bitDepth == 16 )
                {
                    uint16_t p;
                    copy( &p, trns->data.data(), sizeof( p ) );
                    pixel.push_back( p );
                }
                else
                {
                    makeException( false );
                }
            }
            else if( ihdr.colorType == PNG_TRUECOLOR )
            {
                makeException( trns->data.size() * 8 == ihdr.bitDepth * 3 );
                if( ihdr.bitDepth == 8 )
                {
                    uint8_t p[3];
                    copy( p, trns->data.data(), sizeof( p ) );
                    for( auto &c : p )
                        pixel.push_back( c );
                }
                else if( ihdr.bitDepth == 16 )
                {
                    uint16_t p[3];
                    copy( p, trns->data.data(), sizeof( p ) );
                    for( auto &c : p )
                        pixel.push_back( c );
                }
                else
                {
                    makeException( false );
                }
            }
            else
            {
                makeException( false );
            }

            fmt.compression.push_front( std::make_shared<Misc>( fmt.bufferSize(), false, false, pixel, fmt ) );
            fmt.channels.pop_back();
            fmt.calculateBits();
        }
        else
        {
            std::optional<Pixel> p;
            fmt.compression.push_front( std::make_shared<Misc>( fmt.bufferSize(), false, false, p, fmt ) );
        }
    }

    fmt.compression.push_front( std::make_shared<FilterAndInterlacePng>( ihdr.interlaceMethod == 1, Abs( fmt.w ), Abs( fmt.h ), fmt ) );
    fmt.clear();

    fmt.compression.push_front( std::make_shared<ZlibPng>( volume, fmt ) );
    fmt.clear();

    fmt.compression.push_front( std::make_shared<FracturePng>( chunks, fmt ) );
    fmt.clear();
}

void makePng( const Reference &ref, Format &format, HeaderWriter *write )
{
    format.w = ref.w;
    format.h = ref.h;

    if( !write )
    {
        SimpleReader r( ref.link, ref.bytes );
        extractPng( format, r );
        return;
    }

    format.offset += sizeof( PNGSignature ) + sizeof( PNGChunkHeader ) + sizeof( PNGIHDRData ) + sizeof( PNGChunk::crc );
    format.channels.push_back( { 'R', 8 } );
    format.channels.push_back( { 'G', 8 } );
    format.channels.push_back( { 'B', 8 } );
    format.channels.push_back( { 'A', 8 } );
    format.calculateBits();

    std::optional<Pixel> p;
    format.compression.push_front( std::make_shared<Misc>( format.bufferSize(), false, false, p, format ) );

    format.compression.push_front( std::make_shared<FilterAndInterlacePng>( true, Abs( format.w ), Abs( format.h ), format ) );
    format.clear();

    format.compression.push_front( std::make_shared<ZlibPng>( 0, format ) );
    format.clear();

    format.compression.push_front( std::make_shared<FracturePng>( 0, format ) );
    format.clear();

    *write = []( const Format & fmt, Reference & dst )
    {
        SimpleWriter w( dst.link, dst.bytes );

        PNGSignature ps;
        makeException( ps.write( w ) );

        PNGIHDRData ihdr;
        ihdr.width = swapBe32( fmt.w );
        ihdr.height = swapBe32( fmt.h );
        ihdr.bitDepth = 8;
        ihdr.colorType = PNG_TRUECOLOR_ALPHA;
        ihdr.compressionMethod = 0;
        ihdr.filterMethod = 0;
        ihdr.interlaceMethod = 1;

        PNGChunk ihdrChunk;
        ihdrChunk.meta.set( "IHDR" );
        ihdrChunk.meta.length = sizeof( ihdr );
        ihdrChunk.data.resize( sizeof( ihdr ) );
        copy( ihdrChunk.data.data(), &ihdr, sizeof( ihdr ) );
        ihdrChunk.updateCrc();

        makeException( ihdrChunk.write( w ) );
    };
}
}
