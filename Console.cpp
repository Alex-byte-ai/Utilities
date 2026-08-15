#include "Console.h"

#include <fstream>
#include <vector>
#include <map>
#include <set>

#include "UnicodeString.h"
#include "Exception.h"
#include "GetTime.h"
#include "Window.h"
#include "Lambda.h"
#include "Thread.h"
#include "Basic.h"

#include "resource.h"

const std::wstring uniqueId = L"n45hJ24ihgUiojK30qhjIh45M2cV3.123";

class Message : public Connect::Message
{
public:
    enum class Type
    {
        text,
        tabRight,
        tabLeft,
        color,
        configure,
        configureEmptyOpt,
        save,
        saveEmptyOpt,
        command,
        clear,
    };

    Type type;
    std::wstring data;
    wchar_t color;

    Message()
    {
        type = Type::text;
    }

    size_t size() const
    {
        if( type != Type::color )
            return data.size() * sizeof( data[0] );
        if( type == Type::color )
            return sizeof( color );
        return 0;
    }

    void *contents( size_t size )
    {
        if( type != Type::color )
        {
            if( size % sizeof( wchar_t ) != 0 )
                return nullptr;

            size /= sizeof( wchar_t );
            if( size == data.size() )
                return data.data();

            data.resize( size );
            return data.data();
        }
        if( type == Type::color )
        {
            if( size != sizeof( color ) )
                return nullptr;
            return &color;
        }
        if( size != 0 )
            return nullptr;
        return data.data();
    }

    const void *contents() const
    {
        if( type != Type::color )
            return data.data();
        if( type == Type::color )
            return &color;
        return nullptr;
    }

    bool input( const std::vector<uint8_t> &vector )
    {
        auto sizeLeft = vector.size();
        auto pointer = vector.data();

        if( sizeLeft < sizeof( type ) )
            return false;

        sizeLeft -= sizeof( type );
        copy( &type, pointer, sizeof( type ) );
        pointer += sizeof( type );

        auto contentsPointer = contents( sizeLeft );
        if( !contentsPointer )
            return false;
        copy( contentsPointer, pointer, sizeLeft );
        return true;
    }

    bool output( std::vector<uint8_t> &vector ) const
    {
        auto dataLength = size();
        vector.resize( sizeof( type ) + dataLength );
        auto pointer = vector.data();

        copy( pointer, &type, sizeof( type ) );
        pointer += sizeof( type );

        auto contentsPointer = contents();
        if( !contentsPointer )
            return false;

        copy( pointer, contents(), dataLength );
        return true;
    }
};

class Console::ServerInformation : public Connect::Message
{
public:
    bool running, focused, delay;

    ServerInformation( bool r, bool f ) : running( r ), focused( f ), delay( false )
    {}

    bool input( const std::vector<uint8_t> &vector )
    {
        long long unsigned time, outputTime;
        if( vector.size() != sizeof( time ) + sizeof( focused ) + sizeof( running ) )
            return false;

        auto pointer = vector.data();

        copy( &outputTime, pointer, sizeof( outputTime ) );
        time = getTime();
        if( outputTime <= time && time - outputTime < 2500000 )
            return false;

        pointer += sizeof( time );
        copy( &focused, pointer, sizeof( focused ) );
        pointer += sizeof( focused );
        copy( &running, pointer, sizeof( running ) );
        return true;
    }

    bool output( std::vector<uint8_t> &vector ) const
    {
        long long unsigned time;
        vector.resize( sizeof( time ) + sizeof( focused ) + sizeof( running ) );

        if( delay )
        {
            time = getTime();
        }
        else
        {
            time = -1;
        }

        auto pointer = vector.data();
        copy( pointer, &time, sizeof( time ) );
        pointer += sizeof( time );
        copy( pointer, &focused, sizeof( focused ) );
        pointer += sizeof( focused );
        copy( pointer, &running, sizeof( running ) );
        return true;
    }
};

class Parser
{
public:
    struct Command
    {
        std::wstring name;
        std::vector<std::wstring> arguments;
    };

    static std::vector<Command> parse( const std::wstring &input )
    {
        std::vector<Command> commands;
        size_t pos = 0;
        while( pos < input.size() )
        {
            skipWhitespace( input, pos );

            if( pos >= input.size() )
            {
                break;
            }

            auto commandName = parseIdentifier( input, pos );
            if( commandName.empty() || input[pos] != L'(' )
            {
                throw Exception( L"Syntax error: expected '(' after command name." );
            }
            ++pos;

            auto arguments = parseArguments( input, pos );

            if( pos >= input.size() || input[pos] != L')' )
            {
                throw Exception( L"Syntax error: expected ')' after arguments." );
            }
            ++pos;

            skipWhitespace( input, pos );

            if( pos >= input.size() || input[pos] != L';' )
            {
                throw Exception( L"Syntax error: expected ';' after command." );
            }
            ++pos;

            commands.push_back( {commandName, arguments} );
        }
        return commands;
    }

private:
    static void skipWhitespace( const std::wstring &input, size_t &pos )
    {
        while( pos < input.size() && iswspace( input[pos] ) )
        {
            ++pos;
        }
    }

    static std::wstring parseIdentifier( const std::wstring &input, size_t &pos )
    {
        size_t start = pos;
        while( pos < input.size() && iswalpha( input[pos] ) )
        {
            ++pos;
        }
        return input.substr( start, pos - start );
    }

    static std::vector<std::wstring> parseArguments( const std::wstring &input, size_t &pos )
    {
        std::vector<std::wstring> args;
        while( pos < input.size() && input[pos] != L')' )
        {
            skipWhitespace( input, pos );

            if( pos < input.size() && input[pos] == L'"' )
            {
                args.push_back( parseStringLiteral( input, pos ) );
            }

            skipWhitespace( input, pos );

            if( pos < input.size() && input[pos] == L',' )
            {
                ++pos;
            }
        }
        return args;
    }

    static std::wstring parseStringLiteral( const std::wstring &input, size_t &pos )
    {
        if( input[pos] != L'"' )
        {
            throw Exception( L"Syntax error: expected '\"' at the beginning of a string literal." );
        }
        ++pos;

        std::wstring result;
        while( pos < input.size() )
        {
            if( input[pos] == L'"' )
            {
                ++pos;
                break;
            }
            if( input[pos] == L'\\' && pos + 1 < input.size() && input[pos + 1] == L'"' )
            {
                result += L'"';
                pos += 2;
            }
            else
            {
                result += input[pos];
                ++pos;
            }
        }
        return result;
    }
};

struct LargeStaticText : public GraphicInterface::Object
{
    LargeStaticText() : scroller( nullptr ), lineHeight( 17 ), linePadding( 1 )
    {
        clear();
    }

    virtual ~LargeStaticText()
    {}

    GraphicInterface::Scroller *scroller;
    std::vector<std::wstring> lines;
    int lineHeight, linePadding;
    wchar_t color;

    virtual int width() const override
    {
        int dx = 0, dy = 0;

        scroller->scroll( dx, dy );
        dx = -dx;
        dy = -dy;

        auto& s = scroller->s;
        size_t firstLine = -dy / lineHeight;
        size_t lastLine = ( s.ah - dy + lineHeight - 1 ) / lineHeight;

        if( lastLine > lines.size() )
            lastLine = lines.size();

        int result = 0;
        for( size_t i = firstLine; i < lastLine; ++i )
        {
            int length = lines[i].length() * 10;
            if( length > result )
                result = length;
        }

        return result;
    }

    virtual int height() const override
    {
        return lineHeight * ( int )lines.size();
    }

    virtual bool contains( int, int ) const override
    {
        return true;
    }

    virtual void draw( GraphicInterface::Canvas& canvas, int dx, int dy ) const override
    {
        if( !visible )
            return;

        auto& s = scroller->s;
        size_t firstLine = -dy / lineHeight;
        size_t lastLine = ( s.ah - dy + lineHeight - 1 ) / lineHeight;

        if( lastLine > lines.size() )
            lastLine = lines.size();

        GraphicInterface::StaticText t;
        t.size = lineHeight - linePadding;
        t.color = 0xffddaabb;

        /*
        t.value = L"Sample Text.\nThis is a test\nABC123\n";
        t.prepare();
        t.draw( canvas, dx, dy );
        */

        for( size_t i = firstLine; i < lastLine; ++i )
        {
            t.x = 0;
            t.y = i * lineHeight + linePadding;
            t.value = lines[i];

            t.prepare();
            t.draw( canvas, dx, dy );
            t.x += t.w;
        }
    }

    void clear()
    {
        lines.clear();
        lines.emplace_back() += ( color = 0xFDE1 );
    }
};

class Console::Data
{
public:
    std::wstring fontName;
    int tabSize, tabs;

    GraphicInterface::Window window;
    LargeStaticText text;

    Thread thread;

    std::function<void()> configureWindow;

    void clear()
    {
        text.clear();
        tabs = 0;
    }

    Data()
    {
        window.title.value = L"Console";
        window.title.prepare();
        window.self.w = 256;
        window.self.h = 256;
        window.scroller.content = &text;
        window.scroller.corner = true;
        window.client.color = 0xff555555;

        text.scroller = &window.scroller;

        tabSize = 8;
        tabs = 0;
    }

    ~Data()
    {
        thread.stop();
    }
};

Console::Color::Color() : r( 0.0f ), g( 0.0f ), b( 0.0f )
{};

Console::Color::Color( float red, float green, float blue, float alpha ) : r( red ), g( green ), b( blue ), a( alpha )
{};

uint32_t Console::Color::get() const
{
    float red = r, green = g, blue = b;
    if( red > 1.0f ) red = 1.0f;
    if( red < 0.0f ) red = 0.0f;
    if( green > 1.0f ) green = 1.0f;
    if( green < 0.0f ) green = 0.0f;
    if( blue > 1.0f ) blue = 1.0f;
    if( blue < 0.0f ) blue = 0.0f;
    return GraphicInterface::makeColor( Round( red * 255 ), Round( green * 255 ), Round( blue * 255 ), 255 );
}

Console::Console() : connect( uniqueId )
{
    serverInfo = new ServerInformation( false, false );
    data = connect.isServer() ? new Data() : nullptr;
    if( !data )
    {
        while( connect.input( *serverInfo ) )
        {}
    }
}

Console::~Console()
{
    if( data )
        data->thread.stop();
    delete serverInfo;
    delete data;
}

bool Console::run()
{
    if( !data || data->thread.running() )
        return false;

    serverInfo->running = true;
    connect.output( *serverInfo );

    sysMsg( L"[Console] Running...\n" );

    data->thread.launch( [this]()
    {
        static Message message;
        while( connect.input( message ) )
        {
            switch( message.type )
            {
            case Message::Type::text:
                msg( message.data );
                break;
            case Message::Type::tabRight:
                ++data->tabs;
                break;
            case Message::Type::tabLeft:
                --data->tabs;
                break;
            case Message::Type::color:
                data->text.lines.back() += ( data->text.color = message.color );
                break;
            case Message::Type::configure:
                configure( message.data );
                break;
            case Message::Type::configureEmptyOpt:
                configure();
                break;
            case Message::Type::save:
                save( message.data );
                break;
            case Message::Type::saveEmptyOpt:
                save();
                break;
            case Message::Type::command:
                command( message.data );
                break;
            case Message::Type::clear:
                clear();
                break;
            default:
                // ???
                break;
            }
        }
        return true;
    } );

    /*
    int x = 0, y = 0;
    RECT screenRect;
    GetClientRect( GetDesktopWindow(), &screenRect );
    x = screenRect.right - screenRect.left - data->windowWidth;
    */

    data->window.run();
    data->thread.stop();

    sysMsg( L"[Console] Stopped.\n" );

    serverInfo->running = false;
    serverInfo->focused = false;
    connect.output( *serverInfo );

    return true;
}

void Console::msg( const std::wstring &message )
{
    if( !data )
    {
        // Communicating with a console from another application
        Message cmessage;
        cmessage.data = message;
        makeException( connect.output( cmessage ) );
        return;
    }

    bool outside = !data->thread.inside();

    Finalizer _;
    if( outside )
        data->thread.pauseForScope( _ );

    std::wstring tabs( data->tabs * data->tabSize, L' ' );
    auto &body = data->text.lines;
    auto* l = &body.back();
    size_t i = 0;

    for( auto c : message )
    {
        if( c == L'\t' )
        {
            *l += L' ';
            while( ++i % data->tabSize != 0 )
                *l += L' ';
            continue;
        }

        if( c == L'\u2028' || c == L'\n' )
        {
            i = 0;
            l = &body.emplace_back();
            *l += data->text.color;
            *l += tabs;
            continue;
        }

        // Ignoring those right now
        if( c < L' ' )
            continue;

        // This might count zero-width characters
        *l += c;
        ++i;
    }
}

void Console::operator++()
{
    if( !data )
    {
        // Communicating with a console from another application
        Message message;
        message.type = Message::Type::tabRight;
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    ++data->tabs;
}

void Console::operator--()
{
    if( !data )
    {
        // Communicating with a console from another application
        Message message;
        message.type = Message::Type::tabLeft;
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    --data->tabs;
}

void Console::color( const std::optional<Color> &c )
{
    auto getColor = [&]()
    {
        return c ? GraphicInterface::getCode( c->get() ) : 0xFDE0;
    };

    if( !data )
    {
        // Communicating with a console from another application
        Message message;
        message.type = Message::Type::color;
        message.color = getColor();
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    data->text.lines.back() += ( data->text.color = getColor() );
}

void Console::configure( const std::optional<std::filesystem::path> &configFile )
{
    if( !data )
    {
        // Communicating with a console from another application
        Message message;
        message.type = configFile ? Message::Type::configure : Message::Type::configureEmptyOpt;
        if( configFile )
            message.data = configFile->wstring();
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    if( data->configureWindow )
        _.push( data->configureWindow );

    auto filePath = configFile ? *configFile : std::filesystem::path( L"config.cfg" );
    std::wifstream file( filePath );
    if( !file )
    {
        _( false );
        return;
    }

    std::map<std::wstring, std::wstring> config;
    std::wstring line;
    while( std::getline( file, line ) )
    {
        if( line.empty() )
            continue;

        auto delimiterPos = line.find( L'=' );
        if( delimiterPos == std::wstring::npos )
        {
            _( false );
            return;
        }

        std::wstring key = line.substr( 0, delimiterPos );
        std::wstring value = line.substr( delimiterPos + 1 );
        config.emplace( key, value );
    }

    auto get = [&]( const std::wstring & key, std::wstring & value )
    {
        auto i = config.find( key );
        if( i == config.end() )
            return _( false );

        value = i->second;
        return _( true );
    };

    auto getColor = []( unsigned long value )
    {
        auto b = value % 0x100;
        value /= 0x100;
        auto g = value % 0x100;
        value /= 0x100;
        auto r = value % 0x100;
        value /= 0x100;
        makeException( value == 0 );
        return GraphicInterface::makeColor( r, g, b, 255 );
    };

    try
    {
        std::wstring value;

        if( get( L"backgroundColor", value ) )
            data->window.client.color = getColor( std::stoul( value, nullptr, 16 ) );

        if( get( L"userColor", value ) )
            GraphicInterface::customColors[0] = getColor( std::stoul( value, nullptr, 16 ) );

        if( get( L"systemColor", value ) )
            GraphicInterface::customColors[1] = getColor( std::stoul( value, nullptr, 16 ) );

        if( get( L"fontName", value ) )
            data->fontName = value;

        if( get( L"fontHeight", value ) )
            data->text.lineHeight = std::stoul( value );

        if( get( L"windowWidth", value ) )
            data->window.self.w = std::stoul( value );

        if( get( L"windowHeight", value ) )
            data->window.self.h = std::stoul( value );

        if( get( L"linePadding", value ) )
            data->text.linePadding = std::stoul( value );

        if( get( L"tabSize", value ) )
            data->tabSize = std::stoul( value );
    }
    catch( ... )
    {
        _( false );
        return;
    }

    sysMsg( std::wstring( L"[Console] Configured with " ) + filePath.wstring() + L"\n" );
    _( true );
}

void Console::save( const std::optional<std::filesystem::path> &path )
{
    if( !data )
    {
        Message message;
        message.type = path ? Message::Type::save : Message::Type::saveEmptyOpt;
        if( path )
            message.data = path->wstring();
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    if( path )
    {
        std::ofstream file( *path, std::ios::binary );
        if( !file )
            return;

        sysMsg( std::wstring( L"[Console] Saved to " ) + path->wstring() + L"\n" );

        const auto last = &data->text.lines.back();
        for( const auto &line : data->text.lines )
        {
            std::vector<uint8_t> output;
            Unicode::String s;
            size_t pos = 0;

            s << GraphicInterface::toPlainText( line );
            if( &line != last )
                s << L"\r\n";

            makeException( s.EncodeUtf8( output, pos ) );
            file.write( ( char * )output.data(), output.size() );
        }

        return;
    }

    savePath( [this]( const auto & userPath )
    {
        if( userPath )
            save( userPath );
    } );
}

void Console::command( const std::wstring &cmd )
{
    if( !data )
    {
        // Communicating with a console from another application
        Message message;
        message.type = Message::Type::command;
        message.data = cmd;
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    // Should have better command system (!!!)
    try
    {
        auto calls = Parser::parse( cmd );
        for( const auto &call : calls )
        {
            if( call.name == L"configure" )
            {
                auto size = call.arguments.size();
                if( size == 0 )
                {
                    configure();
                    continue;
                }
                if( size == 1 )
                {
                    configure( call.arguments[0] );
                    continue;
                }
                throw Exception( L"Logic error: expected 'configure' command to receive no more than 1 arguments." );
            }
            if( call.name == L"save" )
            {
                auto size = call.arguments.size();
                if( size == 0 )
                {
                    save();
                    continue;
                }
                if( size == 1 )
                {
                    save( call.arguments[0] );
                    continue;
                }
                throw Exception( L"Logic error: expected 'save' command to receive no more than 1 arguments." );
            }
            if( call.name == L"command" )
            {
                auto size = call.arguments.size();
                if( size == 1 )
                {
                    command( call.arguments[0] );
                    continue;
                }
                throw Exception( L"Logic error: expected 'command' command to receive 1 argument." );
            }
            if( call.name == L"clear" )
            {
                auto size = call.arguments.size();
                if( size == 0 )
                {
                    clear();
                    continue;
                }
                throw Exception( L"Logic error: expected 'clear' command to receive 0 arguments." );
            }
            throw Exception( L"Logic error: unknown command '" + call.name + L"'." );
        }
    }
    catch( const Exception &e )
    {
        sysMsg( L"[Console] Error: " + e.message() + L"\n" );
    }
}

void Console::clear()
{
    if( !data )
    {
        // Communicating with a console from another application
        Message message;
        message.type = Message::Type::clear;
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    data->clear();

    sysMsg( L"[Console] Cleared.\n" );
}

bool Console::focused()
{
    if( !data )
    {
        while( connect.input( *serverInfo ) )
        {}
    }
    return serverInfo->focused;
}

bool Console::running()
{
    if( !data )
    {
        while( connect.input( *serverInfo ) )
        {}
    }
    return serverInfo->running;
}

void Console::numericBase( short int value )
{
    buffer.numericBase( value );
}

short int Console::numericBase()
{
    return buffer.numericBase();
}

void Console::showBase( std::optional<short int> value )
{
    buffer.showBase( value );
}

std::optional<short int> Console::showBase()
{
    return buffer.showBase();
}

void Console::sysMsg( const std::wstring &message )
{
    if( !data )
        return;

    bool outside = !data->thread.inside();

    Finalizer _;
    if( outside )
        data->thread.pauseForScope( _ );

    auto backup = data->text.color;
    data->text.lines.back() += ( data->text.color = 0xFDE1 );
    msg( message );
    data->text.lines.back() += backup;
}
