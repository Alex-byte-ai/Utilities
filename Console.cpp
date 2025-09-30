#include "Console.h"

#include <windows.h>

#include <stdexcept>
#include <cwctype>
#include <variant>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

#include "UnicodeString.h"
#include "GetPathToFile.h"
#include "Exception.h"
#include "Lambda.h"
#include "Thread.h"
#include "Basic.h"

#include "resource.h"

const std::wstring uniqueId = L"n45hJ24ihgUiojK30qhjIh45M2cV3";

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
        flush,
        clear,
    };

    Type type;
    std::wstring data;
    COLORREF color;

    Message()
    {
        type = Type::text;
    }

    size_t size() const
    {
        if( type != Type::color )
            return data.size() * sizeof( wchar_t );
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

class Coloring
{
public:
    enum class DefaultColor
    {
        user,
        console,
    };

    using Color = std::variant<DefaultColor, COLORREF>;

    class Repetitive
    {
    public:
        long long unsigned repeats;
        Color color;
    };

    using Sequence = std::variant<Repetitive, std::vector<Color>>;

    class Position
    {
    public:
        bool nextWord = false;
        long long unsigned left = 0, right = 0;
    private:
        long long unsigned blockId = 0, colorId = 0, letterId = 0;

        friend Coloring;
    };

    class ConnectText
    {
    public:
        Position position;
        long long unsigned line = 0; // First line fully contained in coloring starting at and including 'position'
    };

    // Minimum number of characters, that requires creating ConnectText instance
    // Real size of such block is determined in WM_PAINT and will be higher a bit
    const long long unsigned addressBlockSize;

    std::vector<Sequence> blocks;
    std::optional<Color> lastColor;
    std::vector<ConnectText> positions;

    COLORREF backgroundColor, textColor, consoleColor;
    Color currentColor;

    void clear()
    {
        blocks.clear();
        lastColor.reset();
        positions.clear();
        positions.emplace_back();
        currentColor = Coloring::DefaultColor::user;
    }

    Coloring() : addressBlockSize( 1024 )
    {
        backgroundColor = RGB( 85, 85, 85 );
        textColor = RGB( 170, 170, 170 );
        consoleColor = RGB( 255, 255, 255 );
        clear();
    }

    COLORREF color( Position &position, long long unsigned wordSize, long long unsigned &delta )
    {
        auto extractColor = [&]( const Color & colorV ) -> COLORREF
        {
            return std::visit( [&]( const auto & color ) -> COLORREF
            {
                if constexpr( std::is_same_v<decltype( color ), const DefaultColor &> )
                {
                    switch( color )
                    {
                    case DefaultColor::user:
                        return textColor;
                    case DefaultColor::console:
                        return consoleColor;
                    default:
                        makeException( false );
                    }
                    return textColor;
                }
                if constexpr( std::is_same_v<decltype( color ), const COLORREF &> )
                {
                    return color;
                }
            }, colorV );
        };

        if( wordSize <= 0 )
        {
            position.nextWord = true;
            position.left = position.right = 0;
            return 0;
        }

        makeException( position.blockId < blocks.size() );
        return std::visit( [&]( const auto & block ) -> COLORREF
        {
            COLORREF result;
            long long unsigned size;
            if constexpr( std::is_same_v<decltype( block ), const Repetitive &> )
            {
                result = extractColor( block.color );
                size = block.repeats;
                delta = size - position.colorId;
            }
            if constexpr( std::is_same_v<decltype( block ), const std::vector<Color>&> )
            {
                result = extractColor( block[position.colorId] );
                size = block.size();
                delta = 1;
            }
            makeException( size > 0 );

            position.left = position.letterId;
            if( position.letterId + delta >= wordSize )
            {
                position.letterId = 0;
                position.nextWord = true;
                delta = wordSize - position.letterId;
                position.right = wordSize;
            }
            else
            {
                position.letterId += delta;
                position.right = position.letterId;
            }

            position.colorId += delta;
            if( position.colorId >= size )
            {
                position.colorId = 0;
                ++position.blockId;
            }

            return result;
        }, blocks[position.blockId] );
    }

    void addRecursive( Repetitive block, size_t size )
    {
        if( block.repeats == 0 )
            return;

        if( size == 0 )
        {
            if( block.repeats == 1 )
                blocks.insert( blocks.begin() + size, std::vector<Color>( 1, block.color ) );
            else
                blocks.insert( blocks.begin() + size, block );
            return;
        }

        auto &lastV = blocks[size - 1];
        std::visit( [&]( auto & last )
        {
            if constexpr( std::is_same_v<decltype( last ), Repetitive &> )
            {
                if( last.color == block.color )
                {
                    last.repeats += block.repeats;
                    return;
                }
                if( block.repeats == 1 )
                    blocks.insert( blocks.begin() + size, std::vector<Color>( 1, block.color ) );
                else
                    blocks.insert( blocks.begin() + size, block );
            }
            if constexpr( std::is_same_v<decltype( last ), std::vector<Color>&> )
            {
                makeException( last.size() > 1 );
                if( block.color != last[last.size() - 1] )
                {
                    if( block.repeats == 1 )
                    {
                        last.push_back( block.color );
                        return;
                    }
                    blocks.insert( blocks.begin() + size, block );
                    return;
                }

                last.resize( last.size() - 1 );
                ++block.repeats;

                if( last.size() <= 1 )
                {
                    auto copyLast = last;
                    blocks.erase( blocks.begin() + size - 1 );
                    if( copyLast.size() == 1 )
                        addRecursive( {1, copyLast[0]}, size - 1 );
                }

                blocks.insert( blocks.begin() + size, block );
            }
        }, lastV );
    }

    void add( const Repetitive &block )
    {
        addRecursive( block, blocks.size() );
        lastColor = block.color;
    }

    ConnectText find( unsigned long long line )
    {
        unsigned long long left = 0, right = positions.size(), middle;
        do
        {
            middle = ( left + right ) / 2;
            if( positions[middle].line <= line )
            {
                left = middle;
            }
            else
            {
                right = middle;
            }
        }
        while( left + 1 < right );
        return positions[left];
    }
};

class Console::Data
{
public:
    int windowWidth, windowHeight, linePadding, tabSize, tabs;
    std::wstring name, className;

    std::vector<std::wstring> lines;
    Coloring coloring;
    bool colorText;

    Thread thread;

    SCROLLINFO scrollX, scrollY;
    ATOM windowClass;
    LOGFONTW font;
    HWND window;

    std::function<void()> configureWindow;

    void clear()
    {
        coloring.clear();

        auto prepare = []( SCROLLINFO & si )
        {
            memset( &si, 0, sizeof( si ) );
            si.cbSize = sizeof( si );
            si.fMask = SIF_ALL;
        };

        prepare( scrollX );
        prepare( scrollY );

        lines.clear();
        lines.emplace_back();
        tabs = 0;
    }

    Data( WNDPROC windowProc )
    {
        makeException( windowProc );

        name = L"Console";
        className = uniqueId + L"_console";

        memset( &font, 0, sizeof( font ) );
        font.lfHeight = 8;
        font.lfWidth = 0;
        font.lfEscapement = 0;
        font.lfOrientation = 0;
        font.lfWeight = FW_NORMAL;
        font.lfItalic = FALSE;
        font.lfUnderline = FALSE;
        font.lfStrikeOut = FALSE;
        font.lfCharSet = DEFAULT_CHARSET;
        font.lfOutPrecision = OUT_DEFAULT_PRECIS;
        font.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        font.lfQuality = NONANTIALIASED_QUALITY;
        font.lfPitchAndFamily = FF_DONTCARE;
        wcscpy( font.lfFaceName, L"DejaVuSansMono" );

        colorText = true;
        linePadding = 1;
        tabSize = 8;

        windowWidth = 256;
        windowHeight = 256;
        window = nullptr;

        WNDCLASSEXW wc;
        memset( &wc, 0, sizeof( wc ) );
        wc.cbSize = sizeof( wc );
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleW( nullptr );
        wc.hIcon = LoadIcon( wc.hInstance, MAKEINTRESOURCE( IDI_WINDOW_ICON ) );
        // wc.hIconSm = LoadIcon( wc.hInstance, MAKEINTRESOURCE( IDI_SMALL_WINDOW_ICON ) );
        wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
        wc.lpszClassName = className.c_str();

        windowClass = RegisterClassExW( &wc );
        makeException( windowClass );

        clear();
    }

    ~Data()
    {
        thread.stop();
        if( windowClass )
            Exception::terminate( UnregisterClassW( className.c_str(), GetModuleHandleW( nullptr ) ) );
    }
};

class Console::ServerInformation : public Connect::Message
{
public:
    // std::wstring title;
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
        GetSystemTimeAsFileTime( ( LPFILETIME )( void * )&time );
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
            GetSystemTimeAsFileTime( ( LPFILETIME )( void * )&time );
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

static COLORREF getColor( const Console::Color &c )
{
    float r = c.r, g = c.g, b = c.b;
    if( r > 1 ) r = 1;
    if( r < 0 ) r = 0;
    if( g > 1 ) g = 1;
    if( g < 0 ) g = 0;
    if( b > 1 ) b = 1;
    if( b < 0 ) b = 0;
    return RGB( Round( r * 255 ), Round( g * 255 ), Round( b * 255 ) );
}

struct Parameters
{
    int lineHeight;
    long long unsigned visibleLines;
};

static Parameters limitVertical( SCROLLINFO &si, Console::Data *data )
{
    // Calculate text dimensions
    RECT clientRect;
    GetClientRect( data->window, &clientRect );
    int clientRectHeight = clientRect.bottom - clientRect.top;
    int lineHeight = data->font.lfHeight + data->linePadding;
    int sizeLines = data->lines.size();
    long long unsigned visibleLines = Min( clientRectHeight / lineHeight + ( clientRectHeight % lineHeight != 0 ), sizeLines );

    si.nMin = 0;
    si.nPage = visibleLines;
    si.nMax = Max( sizeLines - 1, 0 );
    si.nPos = Max( si.nMin, Min( si.nPos, si.nMax - ( int )si.nPage + 1 ) );

    return Parameters{lineHeight, visibleLines};
}

static void limitHorizontal( SCROLLINFO &si, const RECT &clientRect, int maxLineLength )
{
    si.nMin = 0;
    si.nMax = maxLineLength;
    si.nPage = clientRect.right - clientRect.left;
    si.nPos = Max( si.nMin, Min( si.nPos, si.nMax ) );
}

static void toggleMenuItem( HMENU hMenu, UINT menuID, UINT parameter )
{
    MENUITEMINFOW menuInfo;
    memset( &menuInfo, 0, sizeof( menuInfo ) );
    menuInfo.cbSize = sizeof( menuInfo );
    menuInfo.fMask = MIIM_STATE;

    // Get the current state of the menu item
    makeException( GetMenuItemInfoW( hMenu, menuID, FALSE, &menuInfo ) );

    // Toggle the state of menu item's property chosen by 'parameter' argument
    if( menuInfo.fState & parameter )
        menuInfo.fState &= ~parameter;
    else
        menuInfo.fState |= parameter;

    // Update the menu item
    makeException( SetMenuItemInfo( hMenu, menuID, FALSE, &menuInfo ) );
}

Console::Console() : connect( uniqueId )
{
    // This code is positioned in lambda to accesses private members of Console
    auto windowProc = []( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) -> LRESULT
    {
        static Console *c = nullptr;
        static Console::Data *d = nullptr;

        static HBRUSH hBrush = nullptr;
        static HFONT oldFont = nullptr, font = nullptr;
        static HBITMAP hbmMem = nullptr;
        static HGDIOBJ hbmOld = nullptr;
        static HDC hdcMem = nullptr;
        static HMENU hMenu = nullptr;
        static RECT clientRect;

        static bool systemRedraw = false;
        if( systemRedraw && uMsg == WM_PAINT )
        {
            systemRedraw = false;
            return TRUE;
        }

        try
        {
            auto same = []( const RECT & a, const RECT & b )
            {
                return a.top == b.top && a.bottom == b.bottom && a.left == b.left && a.right == b.right;
            };

            auto configure = [&]( bool recreate )
            {
                if( font )
                {
                    SelectObject( hdcMem, oldFont );
                    DeleteObject( font );
                }

                if( hBrush )
                    DeleteObject( hBrush );

                if( hdcMem )
                {
                    SelectObject( hdcMem, hbmOld );
                    DeleteDC( hdcMem );
                    DeleteObject( hbmMem );
                }

                hdcMem = recreate ? CreateCompatibleDC( nullptr ) : nullptr;
                hbmMem = recreate ? CreateCompatibleBitmap( GetDC( d->window ), clientRect.right, clientRect.bottom ) : nullptr;
                hbmOld = recreate ? SelectObject( hdcMem, hbmMem ) : nullptr;

                hBrush = recreate ? CreateSolidBrush( d->coloring.backgroundColor ) : nullptr;

                font = recreate ? CreateFontIndirectW( &d->font ) : nullptr;
                oldFont = recreate ? ( HFONT )SelectObject( hdcMem, font ) : nullptr;

                if( hdcMem )
                    SetBkColor( hdcMem, d->coloring.backgroundColor );
            };

            switch( uMsg )
            {
            case WM_NCCREATE:
                return DefWindowProc( hwnd, uMsg, wParam, lParam );
            case WM_CREATE:
                {
                    auto cs = ( LPCREATESTRUCT )lParam;
                    c = ( Console * )cs->lpCreateParams;
                    SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )c );
                    // GetWindowLongPtr( hwnd, GWLP_USERDATA ) ???
                    if( c )
                    {
                        d = c->data;
                        if( d )
                        {
                            d->configureWindow = [&]()
                            {
                                configure( true );
                            };
                        }
                    }
                    memset( &clientRect, 0, sizeof( clientRect ) );

                    hMenu = CreateMenu();

                    HMENU hFileMenu = CreatePopupMenu();
                    AppendMenuW( hFileMenu, MF_STRING, 101, L"New" );
                    AppendMenuW( hFileMenu, MF_SEPARATOR, 0, L"" );
                    AppendMenuW( hFileMenu, MF_STRING, 102, L"Save" );
                    AppendMenuW( hFileMenu, MF_STRING, 103, L"Save as" );
                    AppendMenuW( hMenu, MF_POPUP, ( UINT_PTR )hFileMenu, L"File" );

                    HMENU hViewMenu = CreatePopupMenu();
                    AppendMenuW( hViewMenu, MF_STRING | MF_CHECKED, 201, L"Color text" );
                    AppendMenuW( hMenu, MF_POPUP, ( UINT_PTR )hViewMenu, L"View" );

                    // MF_OWNERDRAW ???

                    SetMenu( hwnd, hMenu );

                    return DefWindowProc( hwnd, uMsg, wParam, lParam );
                }
            default:
                break;
            }

            if( !d )
                return DefWindowProc( hwnd, uMsg, wParam, lParam );

            Finalizer _;
            d->thread.pauseForScope( _ );

            switch( uMsg )
            {
            case WM_SETFOCUS:
                {
                    c->serverInfo->delay = true;
                    c->serverInfo->focused = true;
                    c->connect.output( *c->serverInfo );
                    c->serverInfo->delay = false;
                    return TRUE;
                }
            case WM_KILLFOCUS:
                {
                    c->serverInfo->focused = false;
                    c->connect.output( *c->serverInfo );
                    return TRUE;
                }
            case WM_CLOSE:
                DestroyWindow( hwnd );
                return TRUE;
            case WM_DESTROY:
                {
                    configure( false );
                    d->window = nullptr;
                    d->configureWindow = nullptr;
                    PostQuitMessage( 0 );
                    return DefWindowProc( hwnd, uMsg, wParam, lParam );
                }
            case WM_PAINT:
                {
                    RECT newClientRect;
                    GetClientRect( hwnd, &newClientRect );
                    if( !same( clientRect, newClientRect ) )
                    {
                        clientRect = newClientRect;
                        configure( true );
                    }

                    FillRect( hdcMem, &clientRect, hBrush );

                    auto params = limitVertical( d->scrollY, d );
                    SetScrollInfo( hwnd, SB_VERT, &d->scrollY, TRUE );
                    unsigned long long firstVisibleLine = d->scrollY.nPos;

                    // Draw the visible lines, calculate maximum line length.
                    int maxLineLength = 0, lineLength;

                    Coloring::Position position;
                    long long unsigned delta;
                    if( d->colorText )
                    {
                        long long unsigned charactersInCurrentBlock = 0;
                        auto connectText = d->coloring.find( firstVisibleLine );
                        position = connectText.position;
                        for( unsigned long long i = connectText.line; i < firstVisibleLine; ++i )
                        {
                            if( charactersInCurrentBlock >= d->coloring.addressBlockSize )
                            {
                                d->coloring.positions.push_back( {position, i} );
                                charactersInCurrentBlock = 0;
                            }

                            while( !position.nextWord )
                            {
                                d->coloring.color( position, d->lines[i].size(), delta );
                                charactersInCurrentBlock += delta;
                            }
                            position.nextWord = false;
                        }
                    }

                    for( long long unsigned i = 0; i < params.visibleLines; ++i )
                    {
                        long long unsigned lineIndex = firstVisibleLine + i;
                        if( lineIndex >= d->lines.size() )
                            break;

                        auto &line = d->lines[lineIndex];

                        RECT lineRect =
                        {
                            LONG( clientRect.left - d->scrollX.nPos ), LONG( clientRect.top + i * params.lineHeight ),
                            LONG( clientRect.right ), LONG( clientRect.top + ( i + 1 ) *params.lineHeight )
                        };

                        if( d->colorText )
                        {
                            lineLength = 0;
                            while( !position.nextWord )
                            {
                                SetTextColor( hdcMem, d->coloring.color( position, line.size(), delta ) );

                                DrawTextExW( hdcMem, line.data() + position.left, position.right - position.left, &lineRect,
                                             DT_NOPREFIX | DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_CALCRECT, nullptr );

                                lineLength += lineRect.right - lineRect.left;

                                DrawTextExW( hdcMem, line.data() + position.left, position.right - position.left, &lineRect,
                                             DT_NOPREFIX | DT_LEFT | DT_SINGLELINE | DT_VCENTER, nullptr );

                                lineRect.left = lineRect.right;
                            }
                            position.nextWord = false;
                        }
                        else
                        {
                            DrawTextExW( hdcMem, line.data(), -1, &lineRect,
                                         DT_NOPREFIX | DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_CALCRECT, nullptr );

                            SetTextColor( hdcMem, d->coloring.textColor );
                            DrawTextExW( hdcMem, line.data(), -1, &lineRect,
                                         DT_NOPREFIX | DT_LEFT | DT_SINGLELINE | DT_VCENTER, nullptr );

                            lineLength = lineRect.right - lineRect.left;
                        }

                        if( lineLength > maxLineLength )
                            maxLineLength = lineLength;
                    }

                    limitHorizontal( d->scrollX, clientRect, maxLineLength );
                    SetScrollInfo( hwnd, SB_HORZ, &d->scrollX, TRUE );

                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint( hwnd, &ps );
                    BitBlt( hdc, 0, 0, clientRect.right, clientRect.bottom, hdcMem, 0, 0, SRCCOPY );
                    EndPaint( hwnd, &ps );

                    systemRedraw = true;
                    makeException( InvalidateRect( d->window, nullptr, TRUE ) );
                    return TRUE;
                }
            case WM_MOUSEWHEEL:
                {
                    // Extract wheel delta
                    int zDelta = GET_WHEEL_DELTA_WPARAM( wParam );

                    // Use SendMessage to trigger centralized scroll handling
                    SendMessage( hwnd, WM_VSCROLL, ( zDelta > 0 ? SB_LINEUP : SB_LINEDOWN ), 0 );
                    return TRUE;
                }
            case WM_KEYDOWN:
                {
                    WPARAM scrollCode = 0;

                    switch( wParam )
                    {
                    case VK_HOME:
                        scrollCode = SB_TOP;
                        break;
                    case VK_END:
                        scrollCode = SB_BOTTOM;
                        break;
                    case VK_PRIOR: // Page Up
                        scrollCode = SB_PAGEUP;
                        break;
                    case VK_NEXT: // Page Down
                        scrollCode = SB_PAGEDOWN;
                        break;
                    default:
                        return DefWindowProc( hwnd, uMsg, wParam, lParam );
                    }

                    // Use SendMessage to trigger centralized scroll handling
                    SendMessage( hwnd, WM_VSCROLL, scrollCode, 0 );
                    return TRUE;
                }
            case WM_VSCROLL:
                {
                    auto &si = d->scrollY;

                    int old = si.nPos;

                    switch( LOWORD( wParam ) )
                    {
                    case SB_LINEUP:
                        si.nPos -= 1;
                        break;
                    case SB_LINEDOWN:
                        si.nPos += 1;
                        break;
                    case SB_PAGEUP:
                        si.nPos -= si.nPage;
                        break;
                    case SB_PAGEDOWN:
                        si.nPos += si.nPage;
                        break;
                    case SB_TOP:
                        si.nPos = si.nMin;
                        break;
                    case SB_BOTTOM:
                        si.nPos = si.nMax;
                        break;
                    case SB_THUMBTRACK:
                        si.nPos = HIWORD( wParam );
                        break;
                    case SB_THUMBPOSITION:
                        si.nPos = HIWORD( wParam );
                        break;
                    case SB_ENDSCROLL:
                        break;
                    default:
                        makeException( false );
                    }

                    limitVertical( si, d );
                    SetScrollInfo( hwnd, SB_VERT, &si, TRUE );
                    if( si.nPos != old )
                        makeException( InvalidateRect( hwnd, nullptr, TRUE ) );
                    return TRUE;
                }
            case WM_HSCROLL:
                {
                    auto &si = d->scrollX;

                    int old = si.nPos;

                    switch( LOWORD( wParam ) )
                    {
                    case SB_LINELEFT:
                        si.nPos -= 1;
                        break;
                    case SB_LINERIGHT:
                        si.nPos += 1;
                        break;
                    case SB_PAGELEFT:
                        si.nPos -= si.nPage;
                        break;
                    case SB_PAGERIGHT:
                        si.nPos += si.nPage;
                        break;
                    case SB_LEFT:
                        si.nPos = si.nMin;
                        break;
                    case SB_RIGHT:
                        si.nPos = si.nMax;
                        break;
                    case SB_THUMBTRACK:
                        si.nPos = HIWORD( wParam );
                        break;
                    case SB_THUMBPOSITION:
                        si.nPos = HIWORD( wParam );
                        break;
                    case SB_ENDSCROLL:
                        break;
                    default:
                        makeException( false );
                    }

                    si.nPos = Max( si.nMin, Min( si.nPos, si.nMax ) );
                    SetScrollInfo( hwnd, SB_HORZ, &si, TRUE );
                    if( si.nPos != old )
                        makeException( InvalidateRect( hwnd, nullptr, TRUE ) );

                    return TRUE;
                }
            case WM_SIZING:
                {
                    RECT *rect = ( RECT * )lParam;

                    auto deltaHeight = d->font.lfHeight + d->linePadding;
                    int height = ( ( rect->bottom - rect->top ) / deltaHeight ) * deltaHeight;
                    if( wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT )
                    {
                        rect->top = rect->bottom - height;
                    }
                    else if( wParam == WMSZ_BOTTOM || wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT )
                    {
                        rect->bottom = rect->top + height;
                    }

                    limitVertical( d->scrollY, d );
                    SetScrollInfo( hwnd, SB_VERT, &d->scrollY, TRUE );

                    return TRUE;
                }
            case WM_COMMAND:
                {
                    switch( LOWORD( wParam ) )
                    {
                    case 101:
                        c->clear();
                        break;
                    case 102:
                        c->save( L"console.txt" );
                        break;
                    case 103:
                        c->save();
                        break;
                    case 201:
                        d->colorText = !d->colorText;
                        toggleMenuItem( hMenu, 201, MFS_CHECKED );
                        break;
                    default:
                        makeException( false );
                    }

                    return DefWindowProc( hwnd, uMsg, wParam, lParam );
                }
            default:
                return DefWindowProc( hwnd, uMsg, wParam, lParam );
            }

            return DefWindowProc( hwnd, uMsg, wParam, lParam );
        }
        catch( ... )
        {
            DestroyWindow( hwnd );
            return DefWindowProc( hwnd, uMsg, wParam, lParam );
        }
        return DefWindowProc( hwnd, uMsg, wParam, lParam );
    };

    serverInfo = new ServerInformation( false, false );
    data = connect.isServer() ? new Data( windowProc ) : nullptr;
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
    if( !data || data->thread.running() || data->window )
        return false;

    serverInfo->running = true;
    connect.output( *serverInfo );

    auto backup = data->coloring.currentColor;
    data->coloring.currentColor = Coloring::DefaultColor::console;
    ( *this )( L"[Console] Running...\n" );
    data->coloring.currentColor = backup;

    data->thread.launch( [this]()
    {
        static Message message;
        while( connect.input( message ) )
        {
            switch( message.type )
            {
            case Message::Type::text:
                ( *this )( message.data );
                break;
            case Message::Type::tabRight:
                ++data->tabs;
                break;
            case Message::Type::tabLeft:
                --data->tabs;
                break;
            case Message::Type::color:
                data->coloring.currentColor = message.color < 0x01000000 ? Coloring::Color( message.color ) : Coloring::Color( Coloring::DefaultColor::user );
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
            case Message::Type::flush:
                flush();
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

    int x, y = 0;
    {
        RECT screenRect;
        GetClientRect( GetDesktopWindow(), &screenRect );
        x = screenRect.right - screenRect.left - data->windowWidth;
    }

    auto window = CreateWindowExW(
                      0, data->className.c_str(), data->name.c_str(),
                      WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
                      x, y, data->windowWidth, data->windowHeight,
                      nullptr, nullptr, GetModuleHandleW( nullptr ), this );

    makeException( window );
    data->window = window;

    SetFocus( window );
    SetForegroundWindow( window );

    MSG msg;
    ShowWindow( window, SW_SHOW );
    while( GetMessage( &msg, nullptr, 0, 0 ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    data->thread.stop();

    backup = data->coloring.currentColor;
    data->coloring.currentColor = Coloring::DefaultColor::console;
    ( *this )( L"[Console] Stopped.\n" );
    data->coloring.currentColor = backup;

    serverInfo->running = false;
    serverInfo->focused = false;
    connect.output( *serverInfo );

    return true;
}

void Console::operator()( const std::wstring &msg )
{
    if( !data )
    {
        // Communicating with a console from another application.
        Message message;
        message.data = msg;
        makeException( connect.output( message ) );
        return;
    }

    bool outside = !data->thread.inside();

    Finalizer _;
    if( outside )
        data->thread.pauseForScope( _ );

    auto &body = data->lines;

    std::wstring tabulation( data->tabs * data->tabSize, L' ' );

    // Returns true, if it extracted a kine other than last.
    auto getLine = []( std::wstring::const_iterator & i, const std::wstring::const_iterator & end, std::wstring & output )
    {
        output.clear();
        while( i != end )
        {
            if( *i == L'\r' )
            {
                ++i;
                continue;
            }
            if( *i == L'\u2028' || *i == L'\n' )
            {
                ++i;
                return true;
            }
            output += *i;
            ++i;
        }
        return false;
    };

    auto fixAndAppend = [&]( const std::wstring & input, std::wstring & output )
    {
        if( input.empty() )
            return;

        size_t j = 0;
        std::wstring tmp;
        tmp.reserve( data->tabSize * input.length() + 1 );

        bool whiteSpace = true;

        for( auto symbol : input )
        {
            if( symbol == L'\t' )
            {
                tmp += L' ';
                while( ++j % data->tabSize != 0 )
                    tmp += L' ';
                continue;
            }

            // Ignoring those right now.
            if( symbol < L' ' )
                continue;

            if( !std::iswspace( symbol ) )
            {
                whiteSpace = false;
            }

            tmp += symbol;
            ++j; // This might count zero-width characters
        }

        Coloring::Color color = data->coloring.lastColor ? *data->coloring.lastColor : data->coloring.currentColor;
        if( !whiteSpace ) color = data->coloring.currentColor;
        tmp = ( output.empty() && !tmp.empty() ? tabulation : L"" ) + tmp;
        data->coloring.add( {tmp.size(), color} );
        output += tmp;
    };

    std::wstring lines = msg + L"\n", line;
    std::wstring::const_iterator stream = lines.cbegin();

    int linesAdded = body.size();

    getLine( stream, lines.cend(), line );
    fixAndAppend( line, body[body.size() - 1] );

    while( getLine( stream, lines.cend(), line ) )
        fixAndAppend( line, body.emplace_back() );

    linesAdded = body.size() - linesAdded;

    if( outside && data->window )
    {
        auto &s = data->scrollY;

        bool autoScroll = s.nPos >= s.nMax - ( int )s.nPage + 1;

        limitVertical( s, data );

        if( autoScroll )
            s.nPos = s.nMax - s.nPage + 1;

        limitVertical( s, data );

        SetScrollInfo( data->window, SB_VERT, &s, TRUE );
        makeException( InvalidateRect( data->window, nullptr, TRUE ) );
    }
}

void Console::operator++()
{
    if( !data )
    {
        // Communicating with a console from another application.
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
        // Communicating with a console from another application.
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
    if( !data )
    {
        // Communicating with a console from another application.
        Message message;
        message.type = Message::Type::color;
        message.color = c ? getColor( *c ) : 0x01000000;
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    data->coloring.currentColor = c ? Coloring::Color( getColor( *c ) ) : Coloring::Color( Coloring::DefaultColor::user );
}

bool Console::configure( const std::optional<std::filesystem::path> &configFile )
{
    if( !data )
    {
        // Communicating with a console from another application.
        Message message;
        message.type = configFile ? Message::Type::configure : Message::Type::configureEmptyOpt;
        if( configFile )
            message.data = configFile->native();
        makeException( connect.output( message ) );
        return true; //???
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    if( data->configureWindow )
        _.push( data->configureWindow );

    auto filePath = configFile ? *configFile : std::filesystem::path( L"config.cfg" );
    std::wifstream file( filePath );
    if( !file )
        return _( false );

    std::map<std::wstring, std::wstring> config;
    std::wstring line;
    while( std::getline( file, line ) )
    {
        if( line.empty() )
            continue;

        auto delimiterPos = line.find( L'=' );
        if( delimiterPos == std::wstring::npos )
            return _( false );

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
        return RGB( r, g, b );
    };

    auto getBool = []( const std::wstring & value )
    {
        if( value == L"true" )
            return true;
        makeException( value == L"false" );
        return false;
    };

    try
    {
        std::wstring value;

        if( get( L"backgroundColor", value ) )
            data->coloring.backgroundColor = getColor( std::stoul( value, nullptr, 16 ) );

        if( get( L"textColor", value ) )
            data->coloring.textColor = getColor( std::stoul( value, nullptr, 16 ) );

        if( get( L"consoleColor", value ) )
            data->coloring.consoleColor = getColor( std::stoul( value, nullptr, 16 ) );

        if( get( L"fontName", value ) )
        {
            makeException( value.size() < LF_FACESIZE );
            wcscpy( data->font.lfFaceName, value.c_str() );
        }

        if( get( L"fontAntialiased", value ) )
            data->font.lfQuality = getBool( value ) ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY;

        if( get( L"fontHeight", value ) )
            data->font.lfHeight = std::stoul( value );

        if( get( L"fontName", value ) )
        {
            makeException( value.size() < LF_FACESIZE );
            wcscpy( data->font.lfFaceName, value.c_str() );
        }

        if( get( L"windowWidth", value ) )
            data->windowWidth = std::stoul( value );

        if( get( L"windowHeight", value ) )
            data->windowHeight = std::stoul( value );

        if( get( L"linePadding", value ) )
            data->linePadding = std::stoul( value );

        if( get( L"tabSize", value ) )
            data->tabSize = std::stoul( value );
    }
    catch( ... )
    {
        return _( false );
    }

    auto backup = data->coloring.currentColor;
    data->coloring.currentColor = Coloring::DefaultColor::console;
    ( *this )( std::wstring( L"[Console] Configured with " ) + filePath.native() + L"\n" );
    data->coloring.currentColor = backup;
    return _( true );
}

bool Console::save( const std::optional<std::filesystem::path> &path )
{
    if( !data )
    {
        Message message;
        message.type = path ? Message::Type::save : Message::Type::saveEmptyOpt;
        if( path )
            message.data = path->native();
        makeException( connect.output( message ) );
        return true; //???
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    if( path )
    {
        std::ofstream file( *path, std::ios::binary );
        if( !file )
            return false;

        auto backup = data->coloring.currentColor;
        data->coloring.currentColor = Coloring::DefaultColor::console;
        ( *this )( std::wstring( L"[Console] Saved to " ) + path->native() + L"\n" );
        data->coloring.currentColor = backup;

        String s;
        for( const auto &line : data->lines )
        {
            if( !s.Empty() )
                s << L"\r\n";
            s << line;
        }

        size_t pos = 0;
        std::vector<uint8_t> output;
        makeException( s.EncodeUtf8( output, pos ) );
        file.write( ( char * )output.data(), output.size() );
        return true;
    }

    auto userPath = SavePath();
    if( userPath )
        return save( userPath );
    return false;
}

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

bool Console::command( const std::wstring &cmd )
{
    if( !data )
    {
        // Communicating with a console from another application.
        Message message;
        message.type = Message::Type::command;
        message.data = cmd;
        makeException( connect.output( message ) );
        return true; //???
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    // Should have better command system.
    // (!!!)
    try
    {
        // !:
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
            if( call.name == L"flush" )
            {
                auto size = call.arguments.size();
                if( size == 0 )
                {
                    flush();
                    continue;
                }
                throw Exception( L"Logic error: expected 'flush' command to receive 0 arguments." );
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
        return true;
    }
    catch( const Exception &e )
    {
        auto backup = data->coloring.currentColor;
        data->coloring.currentColor = Coloring::DefaultColor::console;
        ( *this )( L"[Console] Error: " + e.message() + L"\n" );
        data->coloring.currentColor = backup;
    }

    return false;
}

void Console::flush()
{
    if( !data )
    {
        // Communicating with a console from another application.
        Message message;
        message.type = Message::Type::flush;
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    data->scrollY.nPos = data->scrollY.nMax;
    limitVertical( data->scrollY, data );
    makeException( InvalidateRect( data->window, nullptr, TRUE ) );
}

void Console::clear()
{
    if( !data )
    {
        // Communicating with a console from another application.
        Message message;
        message.type = Message::Type::clear;
        makeException( connect.output( message ) );
        return;
    }

    Finalizer _;
    if( !data->thread.inside() )
        data->thread.pauseForScope( _ );

    data->clear();

    auto backup = data->coloring.currentColor;
    data->coloring.currentColor = Coloring::DefaultColor::console;
    ( *this )( L"[Console] Cleared.\n" );
    data->coloring.currentColor = backup;
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
