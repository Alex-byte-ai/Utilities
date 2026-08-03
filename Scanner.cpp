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
    if( Unicode::String::DecodeUtf8( symbol, buffer, p ) )
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
    return ( 'a' <= symbol && symbol <= 'z' ) || ( 'A' <= symbol && symbol <= 'Z' ) || symbol == '_';
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

Scanner::Token::Token( Scanner &scn ): scanner( scn )
{
    t = Nil;

    x = 0;
    n = 0;

    place = 0;
    line = 0;
}

Scanner::Token::Token( const Token& other ) : scanner( other.scanner ), t( other.t ), n( other.n ), x( other.x ), s( other.s ), place( other.place ), line( other.line )
{}

bool Scanner::Token::isSign() const
{
    return t == Slash ||
           t == SlashEquals ||
           t == BackSlash ||
           t == Colon ||
           t == Semicolon ||
           t == Comma ||
           t == Dot ||
           t == Exclamation ||
           t == ExclamationEquals ||
           t == Question ||
           t == Circumflex ||
           t == CircumflexEquals ||
           t == NumberSign ||
           t == Percent ||
           t == PercentEquals ||
           t == AtSign ||
           t == Ampersand ||
           t == AmpersandEquals ||
           t == VerticalBar ||
           t == VerticalBarEquals ||
           t == Dollar ||
           t == Tilde ||
           t == Equals ||
           t == EqualsEquals ||
           t == Smaller ||
           t == SmallerSmaller ||
           t == SmallerSmallerEquals ||
           t == Greater ||
           t == GreaterGreater ||
           t == GreaterGreaterEquals ||
           t == SmallerEquals ||
           t == GreaterEquals ||
           t == Minus ||
           t == MinusMinus ||
           t == MinusMinusPost ||
           t == MinusMinusPre ||
           t == MinusEquals ||
           t == Plus ||
           t == PlusPlus ||
           t == PlusPlusPost ||
           t == PlusPlusPre ||
           t == PlusEquals ||
           t == Star ||
           t == StarEquals ||
           t == BraceO ||
           t == BraceC ||
           t == BracketO ||
           t == BracketC ||
           t == ParenthesisO ||
           t == ParenthesisC;
}

Unicode::String Scanner::Token::name() const
{
    Unicode::String result;
    if( t == NoFile )
        result << L"NoFile";
    else if( t == Nil )
        result << L"Nil";
    else if( t == Name )
        result << L"Name(" << s << L")";
    else if( t == Int )
        result << L"Int(" << s << L")";
    else if( t == Real )
        result << L"Real(" << s << L")";
    else if( t == Text )
        result << L"Text('" << s << L"')";
    else if( isSign() )
        result << L"Sign('" << s << L"')";
    else if( t == Line )
        result << L"Line('" << s << L"')";
    else
        result << L"Bad";
    return result;
}

Unicode::String Scanner::Token::description( TokenType t )
{
    Unicode::String result;
    if( t == NoFile )
        result << L"data source is missing";
    else if( t == Nil )
        result << L"end of data";
    else if( t == Name )
        result << L"name";
    else if( t == Int )
        result << L"integer";
    else if( t == Real )
        result << L"real number";
    else if( t == Text )
        result << L"text";
    else if( t == Slash )
        result << L"slash";
    else if( t == SlashEquals )
        result << L"divide assignment";
    else if( t == BackSlash )
        result << L"back slash";
    else if( t == Colon )
        result << L"colon";
    else if( t == Semicolon )
        result << L"semicolon";
    else if( t == Comma )
        result << L"comma";
    else if( t == Dot )
        result << L"dot";
    else if( t == Exclamation )
        result << L"not";
    else if( t == ExclamationEquals )
        result << L"not assignment";
    else if( t == Question )
        result << L"question sign";
    else if( t == Circumflex )
        result << L"exclusive or";
    else if( t == CircumflexEquals )
        result << L"exclusive or assignment";
    else if( t == NumberSign )
        result << L"number sign";
    else if( t == Percent )
        result << L"remainder";
    else if( t == PercentEquals )
        result << L"remainder assignment";
    else if( t == AtSign )
        result << L"at sign";
    else if( t == Ampersand )
        result << L"and";
    else if( t == AmpersandEquals )
        result << L"and assignment";
    else if( t == VerticalBar )
        result << L"or";
    else if( t == VerticalBarEquals )
        result << L"or assignment";
    else if( t == Dollar )
        result << L"dollar sign";
    else if( t == Tilde )
        result << L"tilde";
    else if( t == Equals )
        result << L"assignment";
    else if( t == EqualsEquals )
        result << L"equals";
    else if( t == Smaller )
        result << L"smaller";
    else if( t == SmallerSmaller )
        result << L"left shift";
    else if( t == SmallerSmallerEquals )
        result << L"left shift assignment";
    else if( t == SmallerEquals )
        result << L"smaller or equals";
    else if( t == Greater )
        result << L"greater";
    else if( t == GreaterGreater )
        result << L"right shift";
    else if( t == GreaterGreaterEquals )
        result << L"right shift assignment";
    else if( t == GreaterEquals )
        result << L"greater or equals";
    else if( t == Minus )
        result << L"minus";
    else if( t == MinusMinus )
        result << L"decrement";
    else if( t == MinusMinusPost )
        result << L"postfix decrement";
    else if( t == MinusMinusPre )
        result << L"prefix decrement";
    else if( t == MinusEquals )
        result << L"minus assignment";
    else if( t == Plus )
        result << L"plus";
    else if( t == PlusPlus )
        result << L"increment";
    else if( t == PlusPlusPost )
        result << L"postfix increment";
    else if( t == PlusPlusPre )
        result << L"prefix increment";
    else if( t == PlusEquals )
        result << L"plus assignment";
    else if( t == Star )
        result << L"multiplication";
    else if( t == StarEquals )
        result << L"multiplication assignment";
    else if( t == BraceO )
        result << L"opening brace";
    else if( t == BraceC )
        result << L"closing brace";
    else if( t == BracketO )
        result << L"opening bracket";
    else if( t == BracketC )
        result << L"closing bracket";
    else if( t == ParenthesisO )
        result << L"opening parenthesis";
    else if( t == ParenthesisC )
        result << L"closing parenthesis";
    else if( t == Line )
        result << L"line";
    else
        result << L"unknown symbol";
    return result;
}

void Scanner::Token::header( Unicode::String &e ) const
{
    e = L"\n";
    e << L"In file " << scanner.fileName << L"\n";
    e << L"On line " << line << L", position " << place << L"\n";
    e << L"Caused by token: " << name() << L"\n";
}

void Scanner::Token::error() const
{
    Unicode::String e;
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

    Unicode::String e;
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

void Scanner::Token::error( const Unicode::String &msg ) const
{
    error();

    Unicode::String e;
    header( e );
    e << msg;
    throw Exception( ( std::wstring )e );
}

Scanner::Scanner( std::istream &d, const Unicode::String &f ): data( d ), fileName( f ), token( *this )
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
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = SlashEquals;
            return;
        }
        token.t = Slash;
        return;
    }

    if( symbol == '\\' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = BackSlash;
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

    if( symbol == ';' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Semicolon;
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

    if( symbol == '.' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Dot;
        return;
    }

    if( symbol == '!' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = ExclamationEquals;
            return;
        }
        token.t = Exclamation;
        return;
    }

    if( symbol == '?' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Question;
        return;
    }

    if( symbol == '^' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = CircumflexEquals;
            return;
        }
        token.t = Circumflex;
        return;
    }

    if( symbol == '#' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        token.t = NumberSign;
        return;
    }

    if( symbol == '%' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = PercentEquals;
            return;
        }
        token.t = Percent;
        return;
    }

    if( symbol == '@' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = AtSign;
        return;
    }

    if( symbol == '&' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = AmpersandEquals;
            return;
        }
        token.t = Ampersand;
        return;
    }

    if( symbol == '|' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = VerticalBarEquals;
            return;
        }
        token.t = VerticalBar;
        return;
    }

    if( symbol == '$' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Dollar;
        return;
    }

    if( symbol == '~' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Tilde;
        return;
    }

    if( symbol == '~' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = Tilde;
        return;
    }

    if( symbol == '=' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = EqualsEquals;
            return;
        }
        token.t = Equals;
        return;
    }

    if( symbol == '<' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = SmallerEquals;
            return;
        }
        if( symbol == '<' )
        {
            token.s.Add( symbol );
            getSymbol();
            if( symbol == '=' )
            {
                token.s.Add( symbol );
                getSymbol();
                token.t = SmallerSmallerEquals;
                return;
            }
            token.t = SmallerSmaller;
            return;
        }
        token.t = Smaller;
        return;
    }

    if( symbol == '>' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = GreaterEquals;
            return;
        }
        if( symbol == '>' )
        {
            token.s.Add( symbol );
            getSymbol();
            if( symbol == '=' )
            {
                token.s.Add( symbol );
                getSymbol();
                token.t = GreaterGreaterEquals;
                return;
            }
            token.t = GreaterGreater;
            return;
        }
        token.t = Greater;
        return;
    }

    if( symbol == '*' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        if( symbol == '=' )
        {
            token.s.Add( symbol );
            getSymbol();
            token.t = StarEquals;
            return;
        }
        token.t = Star;
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

    if( symbol == '(' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = ParenthesisO;
        return;
    }

    if( symbol == ')' )
    {
        token.s.Clear();
        token.s.Add( symbol );
        getSymbol();
        token.t = ParenthesisC;
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
            if( symbol == '=' )
            {
                token.s.Add( symbol );
                getSymbol();
                token.t = MinusEquals;
                return;
            }
            if( symbol == '-' )
            {
                token.s.Add( symbol );
                getSymbol();
                token.t = MinusMinus;
                return;
            }
            token.t = Minus;
        }
        else if( symbol == '+' )
        {
            neg = false;
            token.s.Add( symbol );
            getSymbol();
            if( symbol == '=' )
            {
                token.s.Add( symbol );
                getSymbol();
                token.t = PlusEquals;
                return;
            }
            if( symbol == '+' )
            {
                token.s.Add( symbol );
                getSymbol();
                token.t = PlusPlus;
                return;
            }
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
            Unicode::String bigNumber;
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

Unicode::String Scanner::trace()
{
    Unicode::String output;
    auto out = [&]()
    {
        output << token.name() << ": \"" << Token::description( token.t ) << "\"\n";
    };

    while( token.t != Nil && token.t != Bad && token.t != NoFile )
    {
        out();
        getToken();
    }
    out();
    return output;
}
