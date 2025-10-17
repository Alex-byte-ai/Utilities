#include "Scanner.h"

#include "Exception.h"
#include "Basic.h"

void Scanner::getSymbol()
{
    if( buffer.empty() )
        fillBuffer();

    if( bufferPos >= buffer.size() )
    {
        symbol = 0;
        return;
    }

    if( buffer.size() - bufferPos < 4 )
        fillBuffer();

    size_t p = bufferPos;
    if( String::DecodeUtf8( symbol, buffer, p ) )
    {
        bufferPos = p;
    }
    else
    {
        // Skip one byte
        symbol = buffer[bufferPos++];
    }

    updatePosition( symbol );
}

bool Scanner::digit() const
{
    return '0' <= symbol && symbol <= '9';
}

bool Scanner::letter() const
{
    return ( 'a' <= symbol && symbol <= 'z' ) || ( 'A' <= symbol && symbol <= 'Z' ) || symbol == '_' || symbol == '.';
}

void Scanner::fillBuffer()
{
    if( buffer.empty() )
    {
        bufferPos = 0;
        bufferEnd = bufferSize;
        buffer.resize( bufferSize );
        data.read( ( char * )buffer.data(), bufferSize );
        buffer.resize( data.gcount() );
    }

    // Slide any leftover bytes down to front
    size_t leftover = bufferEnd - bufferPos;
    if( leftover > 0 && bufferPos > 0 )
        move( buffer.data(), buffer.data() + bufferPos, leftover );

    bufferPos = 0;
    bufferEnd = leftover;

    leftover = bufferSize - bufferEnd;
    if( leftover > 0 )
    {
        data.read( ( char * )buffer.data() + bufferEnd, leftover );
        bufferEnd += data.gcount();
        buffer.resize( bufferEnd );
    }
}

void Scanner::updatePosition( uint32_t sym )
{
    // LF, NEL, LS, PS
    if( sym == 0x0A || sym == 0x85 || sym == 0x2028 || sym == 0x2029 )
    {
        token.place = 0;
        ++token.line;
    }
    // CR
    else if( sym != 0x0D )
    {
        ++token.place;
    }
}

Scanner::Token::Token( Scanner &scanner_ ): scanner( scanner_ )
{
    t = Nil;

    x = 0;
    n = 0;

    place = 0;
    line = 0;
}

String Scanner::Token::name() const
{
    String result;
    if( t == Nil )
        result << L"Nil";
    else if( t == Scanner::Name )
        result << L"Name(" << s << L")";
    else if( t == Int )
        result << L"Int(" << s << L")";
    else if( t == Real )
        result << L"Real(" << s << L")";
    else if( t == Text )
        result << L"Text(" << s << L")";
    else if( t == Slash )
        result << L"Slash";
    else if( t == Colon )
        result << L"Colon";
    else if( t == Comma )
        result << L"Comma";
    else if( t == BraceO )
        result << L"BraceO";
    else if( t == BraceC )
        result << L"BraceC";
    else if( t == BracketO )
        result << L"BracketO";
    else if( t == BracketC )
        result << L"BracketC";
    else if( t == Plus )
        result << L"Plus";
    else if( t == Minus )
        result << L"Minus";
    else if( t == Line )
        result << L"Line(" << s << L")";
    else if( t == NoFile )
        result << L"NoFile";
    else
        result << L"Bad";
    return result;
}

String Scanner::Token::description( TokenType t )
{
    String result;
    if( t == Nil )
        result << L"end of file";
    else if( t == Scanner::Name )
        result << L"name";
    else if( t == Int )
        result << L"integer";
    else if( t == Real )
        result << L"real number";
    else if( t == Text )
        result << L"text";
    else if( t == Text )
        result << L"text";
    else if( t == Slash )
        result << L"slash";
    else if( t == Colon )
        result << L"colon";
    else if( t == Comma )
        result << L"comma";
    else if( t == BraceO )
        result << L"opening brace";
    else if( t == BraceC )
        result << L"closing brace";
    else if( t == BracketO )
        result << L"opening bracket";
    else if( t == BracketC )
        result << L"closing bracket";
    else if( t == Plus )
        result << L"plus";
    else if( t == Minus )
        result << L"minus";
    else if( t == Line )
        result << L"line";
    else if( t == NoFile )
        result << L"data source is missing";
    else
        result << L"unknown symbol";
    return result;
}

void Scanner::Token::header( String &e ) const
{
    e = L"\n";
    e << L"In file " << scanner.fileName << L"\n";
    e << L"On line " << line << L", position " << place << L"\n";
    e << L"Caused by token: " << name() << L"\n";
}

void Scanner::Token::error() const
{
    String e;
    auto make = [&]()
    {
        throw Exception( ( std::wstring )e );
    };

    if( t == NoFile )
    {
        e << scanner.fileName << L" doesn't exist.";
        make();
    }

    if( t == Bad )
    {
        header( e );
        e << L"Unknown symbol.";
        make();
    }
}

void Scanner::Token::error( TokenType expected ) const
{
    error();

    String e;
    auto make = [&]()
    {
        throw Exception( ( std::wstring )e );
    };

    if( expected == Real )
    {
        if( ( t != Real ) && ( t != Int ) )
        {
            header( e );
            e << "Real or integer number was expected, but " << description( t ) << " was found.";
            make();
        }
        return;
    }

    if( t != expected )
    {
        header( e );
        e << description( expected ) << " was expected, but " << description( t ) << " was found.";
        make();
    }
}

void Scanner::Token::error( const String &msg ) const
{
    error();

    String e;
    header( e );
    e << msg;
    throw Exception( ( std::wstring )e );
}

Scanner::Scanner( std::istream &d, const String &f ): data( d ), fileName( f ), token( *this )
{
    if( !data )
    {
        token.t = NoFile;
        return;
    }

    getSymbol();
    if( symbol == 0x200b )
    {
        getSymbol();
        getToken();
    }
    else
    {
        getToken();
    }
}

Scanner::~Scanner()
{}

void Scanner::getToken()
{
    if( token.t == NoFile )
        return;

    // Ignoring white space
    while( ( symbol != '\0' ) && ( symbol <= ' ' ) )
        getSymbol();

    // Ignoring comments
    while( symbol == '#' )
    {
        getSymbol();

        while( symbol != '\0' && symbol != '\r' && symbol != '\n' )
            getSymbol();

        while( ( symbol != '\0' ) && ( symbol <= ' ' ) )
            getSymbol();
    }

    if( symbol == '\0' )
    {
        token.t = Nil;
        return;
    }

    if( symbol == '/' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Slash;
        return;
    }

    if( symbol == ':' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Colon;
        return;
    }

    if( symbol == '{' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = BraceO;
        return;
    }

    if( symbol == '}' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = BraceC;
        return;
    }

    if( symbol == '[' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = BracketO;
        return;
    }

    if( symbol == ']' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = BracketC;
        return;
    }

    if( symbol == ',' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Comma;
        return;
    }

    if( letter() )
    {
        token.s.Clear();
        token.t = Name;
        do
        {
            token.s.Add( symbol );
            getSymbol();
        }
        while( letter() || digit() );

        return;
    }

    if( digit() || symbol == '-' || symbol == '+' )
    {
        int e10 = 0, d;
        bool neg;

        token.n = 0;
        token.x = 0;
        token.s = L"";

        if( symbol == '-' )
        {
            neg = true;
            token.s.Add( symbol );
            getSymbol();
            token.t = Minus;
        }
        else if( symbol == '+' )
        {
            neg = false;
            token.s.Add( symbol );
            getSymbol();
            token.t = Plus;
        }
        else
        {
            neg = false;
        }

        if( !digit() )
            return;

        token.t = Int;

        while( digit() )
        {
            d = symbol - '0';
            token.n = 10 * token.n + d;
            token.x = 10 * token.x + d;
            token.s.Add( symbol );
            getSymbol();
        }

        if( symbol == '.' )
        {
            token.t = Real;
            token.s.Add( symbol );
            getSymbol();
            while( digit() )
            {
                token.x = 10 * token.x + ( symbol - '0' );
                token.s.Add( symbol );
                getSymbol();
                --e10;
            }
        }

        if( symbol == 'e' || symbol == 'E' )
        {
            int e = 0;
            bool nex;

            token.t = Real;

            token.s.Add( symbol );
            getSymbol();

            if( symbol == '-' )
            {
                nex = true;
                token.s.Add( symbol );
                getSymbol();
            }
            else if( symbol == '+' )
            {
                nex = false;
                token.s.Add( symbol );
                getSymbol();
            }
            else
            {
                nex = false;
            }

            while( digit() )
            {
                e = 10 * e + ( symbol - '0' );
                token.s.Add( symbol );
                getSymbol();
            }

            if( nex )
            {
                e10 -= e;
            }
            else
            {
                e10 += e;
            }
        }

        if( token.t == Real )
        {
            token.x *= Pow( 10.0, e10 );
        }
        else if( token.s.Length() >= 19 )
        {
            String bigNumber;
            bigNumber << token.n;
            if( token.s != bigNumber )
                token.t = Real;
        }

        if( neg )
        {
            token.n = -token.n;
            token.x = -token.x;
        }

        return;
    }

    if( symbol == '"' || symbol == '\'' )
    {
        auto closing = symbol;
        token.s.Clear();

        while( true )
        {
            getSymbol();
            if( symbol < ' ' )
            {
                token.t = Bad;
                break;
            }
            if( symbol == closing )
            {
                token.t = Text;
                getSymbol();
                break;
            }
            if( symbol == '\\' )
            {
                getSymbol();
                if( symbol == '\\' || symbol == closing )
                {
                    token.s.Add( symbol );
                    continue;
                }
                if( symbol == 't' )
                {
                    token.s.Add( '\t' );
                    continue;
                }
                if( symbol == 'n' )
                {
                    token.s.Add( '\n' );
                    continue;
                }
                token.t = Bad;
                break;
            }
            token.s.Add( symbol );
        }

        return;
    }

    token.s.Clear();
    token.s.Add( symbol );
    getSymbol();
    token.t = Bad;
}

void Scanner::getLine()
{
    if( token.t == NoFile )
        return;

    token.s = L"";
    token.t = Line;

    getSymbol();
    while( symbol != '\0' && symbol != '\n' && symbol != '\r' )
    {
        token.s.Add( symbol );
        getSymbol();
    }

    if( symbol == '\n' && symbol == '\r' )
        getSymbol();
}

String Scanner::trace()
{
    String output;
    while( token.t != Nil && token.t != Bad && token.t != NoFile )
    {
        output << token.name() << "\n";
        getToken();
    }
    output << token.name() << "\n";
    return output;
}
