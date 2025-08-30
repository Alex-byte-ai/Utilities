#include "UnicodeString.h"

#include <string>
#include <limits>

#include "Exception.h"
#include "Lambda.h"
#include "Basic.h"

const std::vector<std::wstring> digits =
{
    L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9",
    L"A", L"B", L"C", L"D", L"E", L"F",
    L"G", L"H", L"I", L"J", L"K", L"L",
    L"M", L"N", L"O", L"P", L"Q", L"R",
    L"S", L"T", L"U", L"V", L"W", L"X",
    L"Y", L"Z"
};

const std::vector<std::wstring> digitsSubscript =
{
    L"₀", L"₁", L"₂", L"₃", L"₄", L"₅", L"₆", L"₇", L"₈", L"₉",
};

const std::vector<std::wstring> digitsSuperscript =
{
    L"⁰", L"¹", L"²", L"³", L"⁴", L"⁵", L"⁶", L"⁷", L"⁸", L"⁹"
};

template<typename T>
const T &choose( int level, const T &a, const T &b, const T &c )
{
    return level < 0 ? a : ( level > 0 ? c : b );
}

template<typename T>
void convertInteger( T number, std::wstring &output, short int base, bool showPlus, int level )
{
    static_assert( std::is_integral_v<T> );

    auto &d = choose( level, digitsSubscript, digits, digitsSuperscript );

    auto digitToChar = [&]( unsigned digit ) -> const std::wstring &
    {
        makeException( digit < d.size() );
        return d[digit];
    };

    std::wstring sign;
    if( base > 0 ) // Negative base systems don't require signs.
    {
        // Choosing 0 to be "positive"
        if( showPlus && number >= 0 )
        {
            sign = choose( level, L"₊", L"+", L"⁺" );
        }
        else if( number < 0 )
        {
            sign = choose( level, L"₋", L"-", L"⁻" );
        }
    }

    std::wstring result;

    if( base > 0 )
    {
        do
        {
            result = digitToChar( Abs( number % base ) ) + result;
            number = number / base;
        }
        while( number != 0 );
    }
    else
    {
        do
        {
            result = digitToChar( Mod( number, base ) ) + result;
            number = Div( number, base );
        }
        while( number != 0 );
    }

    output += sign + result;
}

// Returns true, if it's needed to show base
template<typename T>
bool convertFloatingPoint( T number, std::wstring &output, short int base, bool showPlus,
                           bool fixedPoint, short int pointPrecision )
{
    static_assert( std::is_floating_point_v<T> );

    auto digitToChar = [&]( unsigned digit ) -> const std::wstring &
    {
        makeException( digit < digits.size() );
        return digits[digit];
    };

    if( !isNumber( number ) )
    {
        output = L"🚫";
        return false;
    }

    if( isInfinite( number ) )
    {
        output = ( number > 0 ) ? L"∞" : L"-∞";
        return false;
    }

    output.clear();

    if( base > 0 )
    {
        if( number < 0 )
        {
            output += L'-';
            number = -number;
        }
        else if( showPlus )
        {
            output += L'+';
        }
    }

    short exponent = 0;
    if( !fixedPoint )
    {
        if( Abs( number ) < 1 )
        {
            do
            {
                number *= base;
                --exponent;
            }
            while( Abs( number ) < 1 );
        }
        else if( Abs( number ) >= Abs( base ) )
        {
            do
            {
                number /= base;
                ++exponent;
            }
            while( Abs( number ) >= Abs( base ) );
        }
    }

    T intPart = RoundDown( number );
    T fracPart = number - intPart;

    std::wstring intStr;
    do
    {
        int digit = Mod( intPart, base );
        intStr = digitToChar( digit ) + intStr;
        intPart = RoundDown( intPart / base );
    }
    while( intPart > 0 );

    output += intStr;

    std::wstring fracStr;
    for( int i = 0; i < pointPrecision; ++i )
    {
        fracPart *= base;
        int digit = RoundDown( fracPart );
        fracStr += digitToChar( digit );
        fracPart -= digit;

        if( fracPart <= 0 )
            break;
    }

    if( !fracStr.empty() )
    {
        output += L".";
        output += fracStr;
    }

    if( exponent != 0 )
    {
        output += L"×10";
        convertInteger( exponent, output, base, showPlus, 1 );
    }

    return true;
}

bool String::DecodeAscii( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos )
{
    if( pos >= size )
        return false;

    uint8_t byte = data[pos++];
    if( byte > 0x7F )
        return false;

    unicode = byte;
    return true;
}

bool String::DecodeUtf8( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos )
{
    if( pos >= size )
        return false;

    if( ( data[pos] & 0b10000000 ) == 0 )
    {
        unicode = data[pos++];
        return true;
    }

    if( ( data[pos] & 0b11100000 ) == 0b11000000 )
    {
        if( pos + 1 >= size )
            return false;
        unicode = ( data[pos++] & 0b00011111 ) << 6 ;
        unicode = unicode | ( data[pos++] & 0b00111111 );
        return true;
    }

    if( ( data[pos] & 0b11110000 ) == 0b11100000 )
    {
        if( pos + 2 >= size )
            return false;
        unicode = ( data[pos++] & 0b00001111 ) << 12;
        unicode = unicode | ( ( data[pos++] & 0b00111111 ) << 6 );
        unicode = unicode | ( data[pos++] & 0b00111111 );
        return true;
    }

    if( ( data[pos] & 0b11111000 ) == 0b11110000 )
    {
        if( pos + 3 >= size )
            return false;
        unicode = ( data[pos++] & 0b00000111 ) << 18;
        unicode = unicode | ( ( data[pos++] & 0b00111111 ) << 12 );
        unicode = unicode | ( ( data[pos++] & 0b00111111 ) << 6 );
        unicode = unicode | ( data[pos++] & 0b00111111 );
        return true;
    }

    return false;
}

bool String::DecodeUtf16be( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos )
{
    if( pos + 1 >= size )
        return false;

    uint16_t unit1 = ( data[pos] << 8 ) | data[pos + 1];
    pos += 2;

    if( unit1 >= 0xD800 && unit1 <= 0xDBFF )
    {
        if( pos + 1 >= size )
            return false;

        uint16_t unit2 = ( data[pos] << 8 ) | data[pos + 1];
        pos += 2;

        if( unit2 < 0xDC00 || unit2 > 0xDFFF )
            return false;

        unicode = 0x10000 + ( ( unit1 - 0xD800 ) << 10 ) + ( unit2 - 0xDC00 );
        return true;
    }

    unicode = unit1;
    return true;
}

bool String::DecodeUtf16le( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos )
{
    if( pos + 1 >= size )
        return false;

    uint16_t unit1 = data[pos] | ( data[pos + 1] << 8 );
    pos += 2;

    if( unit1 >= 0xD800 && unit1 <= 0xDBFF )
    {
        if( pos + 1 >= size )
            return false;

        uint16_t unit2 = data[pos] | ( data[pos + 1] << 8 );
        pos += 2;

        if( unit2 < 0xDC00 || unit2 > 0xDFFF )
            return false;

        unicode = 0x10000 + ( ( unit1 - 0xD800 ) << 10 ) + ( unit2 - 0xDC00 );
        return true;
    }

    unicode = unit1;
    return true;
}

bool String::DecodeUtf32be( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos )
{
    if( pos + 3 >= size )
        return false;

    unicode = ( ( uint32_t )data[pos++] ) << 24;
    unicode |= ( ( uint32_t )data[pos++] ) << 16;
    unicode |= ( ( uint32_t )data[pos++] ) << 8;
    unicode |= ( uint32_t )data[pos++];

    return true;
}

bool String::DecodeUtf32le( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos )
{
    if( pos + 3 >= size )
        return false;

    unicode = data[pos++];
    unicode |= ( ( uint32_t )data[pos++] ) << 8;
    unicode |= ( ( uint32_t )data[pos++] ) << 16;
    unicode |= ( ( uint32_t )data[pos++] ) << 24;

    return true;
}

bool String::DecodeAscii( const uint8_t *data, const size_t size, size_t &pos )
{
    uint32_t unicode;
    while( DecodeAscii( unicode, data, size, pos ) )
        text.emplace_back( unicode );
    return pos == size;
}

bool String::DecodeUtf8( const uint8_t *data, const size_t size, size_t &pos )
{
    uint32_t unicode;
    while( DecodeUtf8( unicode, data, size, pos ) )
        text.emplace_back( unicode );
    return pos == size;
}

bool String::DecodeUtf16be( const uint8_t *data, const size_t size, size_t &pos )
{
    uint32_t unicode;
    while( DecodeUtf16be( unicode, data, size, pos ) )
        text.emplace_back( unicode );
    return pos == size;
}

bool String::DecodeUtf16le( const uint8_t *data, const size_t size, size_t &pos )
{
    uint32_t unicode;
    while( DecodeUtf16le( unicode, data, size, pos ) )
        text.emplace_back( unicode );
    return pos == size;
}

bool String::DecodeUtf32be( const uint8_t *data, const size_t size, size_t &pos )
{
    uint32_t unicode;
    while( DecodeUtf32be( unicode, data, size, pos ) )
        text.emplace_back( unicode );
    return pos == size;
}

bool String::DecodeUtf32le( const uint8_t *data, const size_t size, size_t &pos )
{
    uint32_t unicode;
    while( DecodeUtf32le( unicode, data, size, pos ) )
        text.emplace_back( unicode );
    return pos == size;
}

//Encoded L'0'
//UTF-16 BE 00 30
//UTF-16 LE 30 00
//UTF-32 BE 00 00 00 30
//UTF-32 LE 30 00 00 00
bool String::Endianness()
{
    const wchar_t s = L'0';
    return reinterpret_cast<const uint8_t *>( &s )[0] == 0x30;
}

String::WideCharClassification String::WideCharType()
{
    if constexpr( sizeof( wchar_t ) == 2 )
    {
        if( Endianness() )
            return le16;
        return be16;
    }
    else if constexpr( sizeof( wchar_t ) == 4 )
    {
        if( Endianness() )
            return le32;
        return be32;
    }
    makeException( false );
    return le16;
}

bool String::EncodeAscii( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos )
{
    if( unicode > 0x7F )
        return false;

    if( !Expander( data )( pos + 1 ) )
        return false;

    data[pos++] = unicode;
    return true;
}

bool String::DecodeAscii( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeAscii( unicode, data.data(), data.size(), pos );
}

bool String::EncodeUtf8( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos )
{
    if( unicode <= 0x7F )
    {
        if( !Expander( data )( pos + 1 ) )
            return false;

        data[pos++] = unicode;
        return true;
    }

    if( unicode <= 0x7FF )
    {
        if( !Expander( data )( pos + 2 ) )
            return false;

        data[pos++] = 0b11000000 | ( unicode >> 6 );
        data[pos++] = 0b10000000 | ( unicode & 0b00111111 );
        return true;
    }

    if( unicode <= 0xFFFF )
    {
        if( !Expander( data )( pos + 3 ) )
            return false;

        data[pos++] = 0b11100000 | ( unicode >> 12 );
        data[pos++] = 0b10000000 | ( ( unicode >> 6 ) & 0b00111111 );
        data[pos++] = 0b10000000 | ( unicode & 0b00111111 );
        return true;
    }

    if( unicode <= 0x10FFFF )
    {
        if( !Expander( data )( pos + 4 ) )
            return false;

        data[pos++] = 0b11110000 | ( unicode >> 18 );
        data[pos++] = 0b10000000 | ( ( unicode >> 12 ) & 0b00111111 );
        data[pos++] = 0b10000000 | ( ( unicode >> 6 ) & 0b00111111 );
        data[pos++] = 0b10000000 | ( unicode & 0b00111111 );
        return true;
    }

    return false;
}

bool String::DecodeUtf8( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf8( unicode, data.data(), data.size(), pos );
}

bool String::EncodeUtf16be( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos )
{
    if( !Expander( data )( pos + 1 ) )
        return false;

    if( unicode <= 0xFFFF )
    {
        if( !Expander( data )( pos + 2 ) )
            return false;
        data[pos++] = unicode >> 8;
        data[pos++] = unicode & 0xFF;
        return true;
    }

    if( unicode <= 0x10FFFF )
    {
        if( !Expander( data )( pos + 4 ) )
            return false;

        unicode -= 0x10000;
        uint16_t highSurrogate = 0xD800 | ( unicode >> 10 );
        uint16_t lowSurrogate = 0xDC00 | ( unicode & 0x3FF );

        data[pos++] = highSurrogate >> 8;
        data[pos++] = highSurrogate & 0xFF;
        data[pos++] = lowSurrogate >> 8;
        data[pos++] = lowSurrogate & 0xFF;
        return true;
    }

    return false;
}

bool String::DecodeUtf16be( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf16be( unicode, data.data(), data.size(), pos );
}

bool String::EncodeUtf16le( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos )
{
    if( !Expander( data )( pos + 1 ) )
        return false;

    if( unicode <= 0xFFFF )
    {
        if( !Expander( data )( pos + 2 ) )
            return false;
        data[pos++] = unicode & 0xFF;
        data[pos++] = unicode >> 8;
        return true;
    }

    if( unicode <= 0x10FFFF )
    {
        if( !Expander( data )( pos + 4 ) )
            return false;

        unicode -= 0x10000;
        uint16_t highSurrogate = 0xD800 | ( unicode >> 10 );
        uint16_t lowSurrogate = 0xDC00 | ( unicode & 0x3FF );

        data[pos++] = highSurrogate & 0xFF;
        data[pos++] = highSurrogate >> 8;
        data[pos++] = lowSurrogate & 0xFF;
        data[pos++] = lowSurrogate >> 8;
        return true;
    }

    return false;
}

bool String::DecodeUtf16le( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf16le( unicode, data.data(), data.size(), pos );
}

bool String::EncodeUtf32be( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos )
{
    if( !Expander( data )( pos + 4 ) )
        return false;

    data[pos++] = ( unicode >> 24 ) & 0xFF;
    data[pos++] = ( unicode >> 16 ) & 0xFF;
    data[pos++] = ( unicode >> 8 ) & 0xFF;
    data[pos++] = unicode & 0xFF;
    return true;
}

bool String::DecodeUtf32be( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf32be( unicode, data.data(), data.size(), pos );
}

bool String::EncodeUtf32le( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos )
{
    if( !Expander( data )( pos + 4 ) )
        return false;

    data[pos++] = unicode & 0xFF;
    data[pos++] = ( unicode >> 8 ) & 0xFF;
    data[pos++] = ( unicode >> 16 ) & 0xFF;
    data[pos++] = ( unicode >> 24 ) & 0xFF;
    return true;
}

bool String::DecodeUtf32le( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf32le( unicode, data.data(), data.size(), pos );
}

bool String::EncodeAscii( std::vector<uint8_t> &data, size_t &pos, bool bom ) const
{
    if( bom && !EncodeAscii( 0xFEFF, data, pos ) )
        return false;
    for( auto &unicode : text )
    {
        if( !EncodeAscii( unicode, data, pos ) )
            return false;
    }
    return true;
}

bool String::DecodeAscii( const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeAscii( data.data(), data.size(), pos );
}

bool String::EncodeUtf8( std::vector<uint8_t> &data, size_t &pos, bool bom ) const
{
    if( bom && !EncodeUtf8( 0xFEFF, data, pos ) )
        return false;
    for( auto &unicode : text )
    {
        if( !EncodeUtf8( unicode, data, pos ) )
            return false;
    }
    return true;
}

bool String::DecodeUtf8( const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf8( data.data(), data.size(), pos );
}

bool String::EncodeUtf16be( std::vector<uint8_t> &data, size_t &pos, bool bom ) const
{
    if( bom && !EncodeUtf16be( 0xFEFF, data, pos ) )
        return false;
    for( auto &unicode : text )
    {
        if( !EncodeUtf16be( unicode, data, pos ) )
            return false;
    }
    return true;
}

bool String::DecodeUtf16be( const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf16be( data.data(), data.size(), pos );
}

bool String::EncodeUtf16le( std::vector<uint8_t> &data, size_t &pos, bool bom ) const
{
    if( bom && !EncodeUtf16le( 0xFEFF, data, pos ) )
        return false;
    for( auto &unicode : text )
    {
        if( !EncodeUtf16le( unicode, data, pos ) )
            return false;
    }
    return true;
}

bool String::DecodeUtf16le( const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf16le( data.data(), data.size(), pos );
}

bool String::EncodeUtf32be( std::vector<uint8_t> &data, size_t &pos, bool bom ) const
{
    if( bom && !EncodeUtf8( 0xFEFF, data, pos ) )
        return false;
    for( auto &unicode : text )
    {
        if( !EncodeUtf32be( unicode, data, pos ) )
            return false;
    }
    return true;
}

bool String::DecodeUtf32be( const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf32be( data.data(), data.size(), pos );
}

bool String::EncodeUtf32le( std::vector<uint8_t> &data, size_t &pos, bool bom ) const
{
    if( bom && !EncodeUtf8( 0xFEFF, data, pos ) )
        return false;
    for( auto &unicode : text )
    {
        if( !EncodeUtf32le( unicode, data, pos ) )
            return false;
    }
    return true;
}

bool String::DecodeUtf32le( const std::vector<uint8_t> &data, size_t &pos )
{
    return DecodeUtf32le( data.data(), data.size(), pos );
}

bool String::EncodeW( std::wstring &data ) const
{
    size_t pos = 0;
    std::vector<uint8_t> bytes;

    switch( WideCharType() )
    {
    case le16:
        if( !EncodeUtf16le( bytes, pos, false ) )
            return false;
        break;
    case be16:
        if( !EncodeUtf16be( bytes, pos, false ) )
            return false;
        break;
    case le32:
        if( !EncodeUtf32le( bytes, pos, false ) )
            return false;
        break;
    case be32:
        if( !EncodeUtf32be( bytes, pos, false ) )
            return false;
        break;
    default:
        break;
    }

    bytes.push_back( 0 );
    bytes.push_back( 0 );
    data = ( const wchar_t * )bytes.data();
    return true;
}

bool String::EncodeA( std::string &data ) const
{
    size_t pos = 0;
    std::vector<uint8_t> bytes;

    if( !EncodeAscii( bytes, pos, false ) )
        return false;

    bytes.push_back( 0 );
    data = ( const char * )bytes.data();
    return true;
}

// Detecting which UTF encryption type is used might not be bullet-proof???
bool String::Decode( const std::vector<uint8_t> &data, size_t &pos )
{
    Clear();

    if( data.size() < pos + 2 )
        return DecodeAscii( data, pos );

    if( data[pos] == 0xEF )
    {
        if( data.size() <= pos + 3 )
            return DecodeAscii( data, pos );

        if( ( data[pos + 1] == 0xBB ) && ( data[pos + 2] == 0xBF ) )
        {
            pos += 3;
            return DecodeUtf8( data, pos );
        }

        return DecodeAscii( data, pos );
    }

    if( data[pos] == 0xFE )
    {
        if( data.size() <= pos + 2 )
            return DecodeAscii( data, pos );

        if( data[pos + 1] == 0xFF )
        {
            pos += 2;
            return DecodeUtf16be( data, pos );
        }

        return DecodeAscii( data, pos );
    }

    if( data[pos] == 0x00 )
    {
        if( data.size() < pos + 4 )
            return DecodeAscii( data, pos );

        if( ( data[pos + 1] == 0x00 ) && ( data[pos + 2] == 0xFE ) && ( data[pos + 3] == 0xFF ) )
        {
            pos += 4;
            return DecodeUtf32be( data, pos );
        }

        return DecodeAscii( data, pos );
    }

    if( data[pos] == 0xFF )
    {
        if( data[pos + 1] != 0xFE )
            return DecodeAscii( data, pos );

        if( ( data.size() >= pos + 4 ) && ( data[pos + 2] == 0x00 ) && ( data[pos + 3] == 0x00 ) )
        {
            pos += 4;
            return DecodeUtf32le( data, pos );
        }

        pos += 2;
        return DecodeUtf16le( data, pos );
    }

    return DecodeAscii( data, pos );
}

String::String()
    : showPlus( false ), fixedPoint( true ), pointPrecision( 8 ), base( 10 )
{}

String::String( const std::wstring &data ) : String()
{
    *this << data;
}

String::String( const std::string &data ) : String()
{
    *this << data;
}

String::String( const wchar_t *data ) : String()
{
    *this << data;
}

String::String( const char *data ) : String()
{
    *this << data;
}

String::~String()
{}

void String::Clear()
{
    text.clear();
}

void String::GetLines( std::vector<String> &lines ) const
{
    String line;
    lines.clear();
    bool r = false;
    for( auto symbol : text )
    {
        if( symbol == '\r' )
        {
            r = true;
            continue;
        }

        if( symbol == '\n' || r )
        {
            r = false;
            lines.emplace_back( line );
            line.text.clear();
            continue;
        }

        line.text.emplace_back( symbol );
    }
    lines.emplace_back( line );
}

void String::SubString( unsigned first, unsigned last, String &result ) const
{
    last = Min( last, ( unsigned )text.size() );
    if( last <= first )
    {
        result.text.clear();
        return;
    }

    result.text.resize( last - first );
    copy( result.text.data(), text.data() + first, result.text.size() * sizeof( result.text[0] ) );
}

String::operator const std::wstring() const
{
    std::wstring result;
    makeException( EncodeW( result ) );
    return result;
}

String &String::operator<<( const String &data )
{
    text.insert( text.end(), data.text.begin(), data.text.end() );
    return *this;
}

String &String::operator<<( const std::wstring &data )
{
    return *this << data.c_str();
}

String &String::operator<<( const wchar_t *data )
{
    makeException( data != nullptr );

    size_t pos = 0;
    switch( WideCharType() )
    {
    case le16:
        makeException( DecodeUtf16le( ( const uint8_t * )data, std::wcslen( data ) * sizeof( wchar_t ), pos ) );
        break;
    case be16:
        makeException( DecodeUtf16be( ( const uint8_t * )data, std::wcslen( data ) * sizeof( wchar_t ), pos ) );
        break;
    case le32:
        makeException( DecodeUtf32le( ( const uint8_t * )data, std::wcslen( data ) * sizeof( wchar_t ), pos ) );
        break;
    case be32:
        makeException( DecodeUtf32be( ( const uint8_t * )data, std::wcslen( data ) * sizeof( wchar_t ), pos ) );
        break;
    default:
        makeException( false );
        break;
    }

    return *this;
}

String &String::operator<<( const std::string &data )
{
    size_t pos = 0;
    makeException( DecodeAscii( ( const uint8_t * )data.c_str(), data.length() * sizeof( char ), pos ) );
    return *this;
}

String &String::operator<<( const char *data )
{
    makeException( data != nullptr );

    size_t pos = 0;
    makeException( DecodeAscii( ( const uint8_t * )data, stringLength( data ) * sizeof( char ), pos ) );
    return *this;
}

String &String::operator<<( bool data )
{
    if( data )
        return *this << "true";
    return *this << "false";
}

String &String::operator<<( char data )
{
    char str[2];
    str[0] = data;
    str[1] = '\0';
    return *this << str;
}

String &String::operator<<( signed char data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( unsigned char data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( wchar_t data )
{
    wchar_t str[2];
    str[0] = data;
    str[1] = L'\0';
    return *this << str;
}

String &String::operator<<( short int data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( unsigned short int data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( int data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( unsigned int data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( long int data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( unsigned long int data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( long long int data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( unsigned long long int data )
{
    std::wstring output;
    convertInteger( data, output, base, showPlus, 0 );
    if( baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( float data )
{
    std::wstring output;
    if( convertFloatingPoint( data, output, base, showPlus, fixedPoint, pointPrecision ) && baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( double data )
{
    std::wstring output;
    if( convertFloatingPoint( data, output, base, showPlus, fixedPoint, pointPrecision ) && baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::operator<<( long double data )
{
    std::wstring output;
    if( convertFloatingPoint( data, output, base, showPlus, fixedPoint, pointPrecision ) && baseBase )
        convertInteger( base, output, *baseBase, showPlus, -1 );
    return *this << output;
}

String &String::Add( uint32_t symbol )
{
    text.push_back( symbol );
    return *this;
}

void String::numericBase( short int value )
{
    makeException( base < -1 || 1 < base );
    base = value;
}

short int String::numericBase()
{
    return base;
}

void String::showBase( std::optional<short int> value )
{
    makeException( !value || *value < -1 || 1 < *value );
    baseBase = std::move( value );
}

std::optional<short int> String::showBase()
{
    return baseBase;
}
