#pragma once

#include <iostream>

#include "UnicodeString.h"

class Scanner
{
private:
    std::istream &data;
    Unicode::String fileName;

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
        SlashEquals,
        BackSlash,
        Colon,
        Semicolon,
        Comma,
        Dot,
        Exclamation,
        ExclamationEquals,
        Question,
        Circumflex,
        CircumflexEquals,
        NumberSign,
        Percent,
        PercentEquals,
        AtSign,
        Ampersand,
        AmpersandEquals,
        VerticalBar,
        VerticalBarEquals,
        Dollar,
        Tilde,
        Equals,
        EqualsEquals,
        Smaller,
        SmallerSmaller,
        SmallerSmallerEquals,
        SmallerEquals,
        Greater,
        GreaterGreater,
        GreaterGreaterEquals,
        GreaterEquals,
        Minus,
        MinusMinus,
        MinusMinusPost,
        MinusMinusPre,
        MinusEquals,
        Plus,
        PlusPlus,
        PlusPlusPost,
        PlusPlusPre,
        PlusEquals,
        Star,
        StarEquals,
        BraceO,
        BraceC,
        BracketO,
        BracketC,
        ParenthesisO,
        ParenthesisC,
        Line,
    };

    class Token
    {
    private:
    public:
        Scanner &scanner;
    public:
        TokenType t;

        long long int n;
        double x;
        Unicode::String s;

        unsigned place, line;

        Token( Scanner &scanner );
        Token( const Token& other );

        bool isSign() const;
        Unicode::String name() const;
        static Unicode::String description( TokenType type );

        void header( Unicode::String &e ) const;

        void error() const;
        void error( TokenType expected ) const;
        void error( const Unicode::String &message ) const;
    };

    Token token;

    Scanner( std::istream &data, const Unicode::String &fileName );
    ~Scanner();

    void getToken();
    void getLine();

    Unicode::String trace();
};
