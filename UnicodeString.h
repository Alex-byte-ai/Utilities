#pragma once

#include <optional>
#include <cstdint>
#include <vector>
#include <string>

class String
{
private:
    [[nodiscard]] static bool DecodeAscii( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] static bool DecodeUtf8( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] static bool DecodeUtf16be( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] static bool DecodeUtf16le( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] static bool DecodeUtf32be( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] static bool DecodeUtf32le( uint32_t &unicode, const uint8_t *data, const size_t size, size_t &pos );

    [[nodiscard]] bool DecodeAscii( const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] bool DecodeUtf8( const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] bool DecodeUtf16be( const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] bool DecodeUtf16le( const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] bool DecodeUtf32be( const uint8_t *data, const size_t size, size_t &pos );
    [[nodiscard]] bool DecodeUtf32le( const uint8_t *data, const size_t size, size_t &pos );
public:
    [[nodiscard]] static bool Endianness(); // Return true, if it's little endian

    enum WideCharClassification
    {
        le16,
        be16,
        le32,
        be32,
    };
    [[nodiscard]] static WideCharClassification WideCharType();

    [[nodiscard]] static bool EncodeAscii( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos );
    [[nodiscard]] static bool DecodeAscii( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] static bool EncodeUtf8( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos );
    [[nodiscard]] static bool DecodeUtf8( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] static bool EncodeUtf16be( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos );
    [[nodiscard]] static bool DecodeUtf16be( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] static bool EncodeUtf16le( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos );
    [[nodiscard]] static bool DecodeUtf16le( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] static bool EncodeUtf32be( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos );
    [[nodiscard]] static bool DecodeUtf32be( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] static bool EncodeUtf32le( uint32_t unicode, std::vector<uint8_t> &data, size_t &pos );
    [[nodiscard]] static bool DecodeUtf32le( uint32_t &unicode, const std::vector<uint8_t> &data, size_t &pos );

    // Encode functions might encode Unicode encryption type as well (BOM)
    // Decode functions just decode block of data
    [[nodiscard]] bool EncodeAscii( std::vector<uint8_t> &data, size_t &pos, bool bom = false ) const;
    [[nodiscard]] bool DecodeAscii( const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] bool EncodeUtf8( std::vector<uint8_t> &data, size_t &pos, bool bom = true ) const;
    [[nodiscard]] bool DecodeUtf8( const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] bool EncodeUtf16be( std::vector<uint8_t> &data, size_t &pos, bool bom = true ) const;
    [[nodiscard]] bool DecodeUtf16be( const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] bool EncodeUtf16le( std::vector<uint8_t> &data, size_t &pos, bool bom = true ) const;
    [[nodiscard]] bool DecodeUtf16le( const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] bool EncodeUtf32be( std::vector<uint8_t> &data, size_t &pos, bool bom = true ) const;
    [[nodiscard]] bool DecodeUtf32be( const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] bool EncodeUtf32le( std::vector<uint8_t> &data, size_t &pos, bool bom = true ) const;
    [[nodiscard]] bool DecodeUtf32le( const std::vector<uint8_t> &data, size_t &pos );

    [[nodiscard]] bool EncodeW( std::wstring &data ) const;
    [[nodiscard]] bool EncodeA( std::string &data ) const;

    // This function detects which Unicode encryption type is used
    [[nodiscard]] bool Decode( const std::vector<uint8_t> &data, size_t &pos );

    String();
    String( const std::wstring &data );
    String( const std::string &data );
    String( const wchar_t *data );
    String( const char *data );
    ~String();

    inline bool operator==( const String &other ) const
    {
        return text == other.text;
    }

    inline bool operator!=( const String &other ) const
    {
        return text != other.text;
    }

    inline size_t Length() const
    {
        return text.size();
    }

    inline size_t Empty() const
    {
        return text.empty();
    }

    // Only clears text.
    void Clear();

    void GetLines( std::vector<String> &lines ) const;
    void SubString( unsigned first, unsigned last, String &result ) const;

    [[nodiscard]] explicit operator const std::wstring() const;

    template<typename T>
    String &operator<<( const T *const data )
    {
        auto old = base;
        base = 16;
        auto &result = *this << "0x" << ( long long unsigned )data;
        base = old;
        return result;
    }

    String &operator<<( const String &data );

    String &operator<<( const std::wstring &data );
    String &operator<<( const std::string &data );

    String &operator<<( const wchar_t *data );
    String &operator<<( const char *data );

    String &operator<<( bool data );

    String &operator<<( char data );

    // These types are output as numbers
    String &operator<<( signed char data );
    String &operator<<( unsigned char data );

    String &operator<<( wchar_t data );

    String &operator<<( short int data );
    String &operator<<( unsigned short int data );

    String &operator<<( int data );
    String &operator<<( unsigned int data );

    String &operator<<( long int data );
    String &operator<<( unsigned long int data );

    String &operator<<( long long int data );
    String &operator<<( unsigned long long int data );

    String &operator<<( float data );
    String &operator<<( double data );
    String &operator<<( long double data );

    String &Add( uint32_t symbol );

    bool showPlus, fixedPoint;
    short int pointPrecision;

    void numericBase( short int value );
    short int numericBase();

    void showBase( std::optional<short int> value );
    std::optional<short int> showBase();
private:
    short int base;
    std::optional<short int> baseBase;

    std::vector<uint32_t> text;
};
