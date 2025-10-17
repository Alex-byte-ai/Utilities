#pragma once

#include <iostream>

#include "UnicodeString.h"

class Scanner
{
private:
    std::istream &data;
    String fileName;

    uint32_t symbol;

    void getSymbol();

    bool digit() const;
    bool letter() const;

    static constexpr size_t bufferSize = 4096;
    std::vector<uint8_t> buffer;
    size_t bufferPos = 0, bufferEnd = 0;

    void fillBuffer();
    void updatePosition( uint32_t sym );
public:
    enum TokenType
    {
        NoFile,
        Bad,
        Nil,
        Name,
        Int,
        Real,
        Text,
        Slash,
        Colon,
        Comma,
        BraceO,
        BraceC,
        BracketO,
        BracketC,
        Plus,
        Minus,
        Line,
    };

    class Token
    {
    private:
        Scanner &scanner;
    public:
        TokenType t;

        long long int n;
        double x;
        String s;

        unsigned place, line;

        Token( Scanner &scanner );

        String name() const;
        static String description( TokenType type );

        void header( String &e ) const;

        void error() const;
        void error( TokenType expected ) const;
        void error( const String &message ) const;
    };

    Token token;

    Scanner( std::istream &data, const String &fileName );
    ~Scanner();

    void getToken();
    void getLine();

    String trace();
};
