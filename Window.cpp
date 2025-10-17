#include "Window.h"

#include <windows.h>
#include <commctrl.h>

#include <cwctype>
#include <memory>

#include "UnicodeString.h"
#include "Exception.h"

#define WM_APP_SHOWMENU (WM_APP + 1)

Popup::Popup( Type t, std::wstring tl, std::wstring i )
    : title( std::move( tl ) ), information( std::move( i ) ), type( t )
{}

void Popup::run()
{
    auto lastWindow = GetForegroundWindow();

    // Map the MessageType to the appropriate MessageBoxW flags
    UINT flags = MB_OK; // Default to OK button only
    switch( type )
    {
    case Type::Info:
        flags |= MB_ICONINFORMATION;
        break;
    case Type::Error:
        flags |= MB_ICONERROR;
        break;
    case Type::Warning:
        flags |= MB_ICONWARNING;
        break;
    case Type::Question:
        flags |= MB_YESNO | MB_ICONQUESTION;
        break;
    default:
        makeException( false );
    }

    // Show the message box
    int result = MessageBoxW( GetConsoleWindow(), information.c_str(), title.c_str(), flags );

    // Map the result to the Response enum
    switch( result )
    {
    case IDABORT:
        break;
    case IDCANCEL:
        break;
    case IDCONTINUE:
        break;
    case IDIGNORE:
        break;
    case IDNO:
        answer = false;
        break;
    case IDOK:
        break;
    case IDRETRY:
        break;
    case IDTRYAGAIN:
        break;
    case IDYES:
        answer = true;
        break;
    default:
        makeException( false );
    }

    DWORD windowProcessId = 0;
    GetWindowThreadProcessId( lastWindow, &windowProcessId );
    if( windowProcessId == GetCurrentProcessId() )
        SetForegroundWindow( lastWindow );
}

static bool isDigit( wchar_t c )
{
    return std::iswdigit( c ) != 0;
}

static bool containsCharExcludingSelection( HWND edit, wchar_t ch, size_t selStart, size_t selEnd )
{
    std::wstring buffer;

    int length = GetWindowTextLengthW( edit );
    buffer.resize( length );

    GetWindowTextW( edit, &buffer[0], length + 1 );

    for( size_t i = 0; i < buffer.size(); ++i )
    {
        if( i >= selStart && i < selEnd )
            continue;
        if( buffer[i] == ch )
            return true;
    }
    return false;
}

static LRESULT CALLBACK editSubclassProcInt( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR )
{
    switch( uMsg )
    {
    case WM_CHAR:
        {
            wchar_t ch = ( wchar_t )wParam;
            if( ch == 0 || ch == 1 || ch == '\b' || ch == '\r' || ch == '\t' ) // Ignore control chars
                break;

            DWORD sel = 0;
            SendMessageW( hWnd, EM_GETSEL, ( WPARAM )&sel, ( LPARAM )&sel );
            int selStart = LOWORD( sel );
            int selEnd = HIWORD( sel );

            if( isDigit( ch ) )
                break;

            if( ch == L'-' )
            {
                if( selStart != 0 )
                    return 0;
                if( containsCharExcludingSelection( hWnd, L'-', selStart, selEnd ) )
                    return 0;
                break;
            }

            return 0;
        }
    case WM_PASTE:
        {
            if( !IsClipboardFormatAvailable( CF_UNICODETEXT ) )
                return 0;

            if( !OpenClipboard( hWnd ) )
                return 0;

            HGLOBAL hg = GetClipboardData( CF_UNICODETEXT );
            if( !hg )
            {
                CloseClipboard();
                return 0;
            }

            wchar_t* clip = ( wchar_t* )GlobalLock( hg );
            if( !clip )
            {
                GlobalUnlock( hg );
                CloseClipboard();
                return 0;
            }

            std::wstring s = clip;
            GlobalUnlock( hg );
            CloseClipboard();

            DWORD sel = 0;
            SendMessageW( hWnd, EM_GETSEL, ( WPARAM )&sel, ( LPARAM )&sel );
            int selStart = LOWORD( sel );
            int selEnd = HIWORD( sel );

            std::wstring out;
            bool seenMinus = containsCharExcludingSelection( hWnd, L'-', selStart, selEnd );

            for( size_t i = 0; i < s.size(); ++i )
            {
                wchar_t c = s[i];
                if( isDigit( c ) )
                {
                    out.push_back( c );
                    continue;
                }
                if( c == L'-' && !seenMinus && selStart == 0 && out.empty() )
                {
                    out.push_back( c );
                    seenMinus = true;
                    continue;
                }
            }

            if( !out.empty() )
            {
                SendMessageW( hWnd, EM_REPLACESEL, TRUE, ( LPARAM )out.c_str() );
            }
            return 0;
        }
    default:
        break;
    }

    return DefSubclassProc( hWnd, uMsg, wParam, lParam );
}

static LRESULT CALLBACK editSubclassProcFloat( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR )
{
    switch( uMsg )
    {
    case WM_CHAR:
        {
            wchar_t ch = ( wchar_t )wParam;
            if( ch == 0 || ch == 1 || ch == '\b' || ch == '\r' || ch == '\t' ) // Ignore control chars
                break;

            DWORD sel = 0;
            SendMessageW( hWnd, EM_GETSEL, ( WPARAM )&sel, ( LPARAM )&sel );
            int selStart = LOWORD( sel );
            int selEnd = HIWORD( sel );

            if( isDigit( ch ) )
                break;

            if( ch == L'.' )
            {
                if( containsCharExcludingSelection( hWnd, L'.', selStart, selEnd ) )
                    return 0;
                break;
            }

            if( ch == L'-' )
            {
                if( selStart != 0 )
                    return 0;
                if( containsCharExcludingSelection( hWnd, L'-', selStart, selEnd ) )
                    return 0;
                break;
            }

            return 0;
        }
    case WM_PASTE:
        {
            if( !IsClipboardFormatAvailable( CF_UNICODETEXT ) )
                return 0;

            if( !OpenClipboard( hWnd ) )
                return 0;

            HGLOBAL hg = GetClipboardData( CF_UNICODETEXT );
            if( !hg )
            {
                CloseClipboard();
                return 0;
            }

            wchar_t* clip = ( wchar_t* )GlobalLock( hg );
            if( !clip )
            {
                GlobalUnlock( hg );
                CloseClipboard();
                return 0;
            }

            std::wstring s = clip;
            GlobalUnlock( hg );
            CloseClipboard();

            DWORD sel = 0;
            SendMessageW( hWnd, EM_GETSEL, ( WPARAM )&sel, ( LPARAM )&sel );
            int selStart = LOWORD( sel );
            int selEnd = HIWORD( sel );

            std::wstring out;
            bool seenDot = containsCharExcludingSelection( hWnd, L'.', selStart, selEnd );
            bool seenMinus = containsCharExcludingSelection( hWnd, L'-', selStart, selEnd );

            for( size_t i = 0; i < s.size(); ++i )
            {
                wchar_t c = s[i];
                if( isDigit( c ) )
                {
                    out.push_back( c );
                    continue;
                }
                if( c == L'.' && !seenDot )
                {
                    out.push_back( c );
                    seenDot = true;
                    continue;
                }
                if( c == L'-' && !seenMinus && selStart == 0 && out.empty() )
                {
                    out.push_back( c );
                    seenMinus = true;
                    continue;
                }
            }

            if( !out.empty() )
            {
                SendMessageW( hWnd, EM_REPLACESEL, TRUE, ( LPARAM )out.c_str() );
            }
            return 0;
        }
    default:
        break;
    }

    return DefSubclassProc( hWnd, uMsg, wParam, lParam );
}

class Field
{
public:
    enum class Type
    {
        Text,
        Integer,
        Float,
        Option,
        Button,
        None
    };

    Field( Settings::Parameter val, int hor, int ver, int width, int height, HWND hwnd, HINSTANCE hInst, int &index )
        : value( std::move( val ) ), x( hor ), y( ver ), w( width ), h( height )
    {
        auto& v = value.value;

        type = Type::None;

        if( !v.has_value() )
            type = Type::Button;
        else if( v.type() == typeid( std::wstring* ) )
            type = Type::Text;
        else if( v.type() == typeid( int64_t* ) )
            type = Type::Integer;
        else if( v.type() == typeid( double* ) )
            type = Type::Float;
        else if( v.type() == typeid( bool* ) || v.type() == typeid( uint16_t* ) )
            type = Type::Option;

        makeException( type != Type::None );

        host = hwnd;
        edit = nullptr;
        offset = -1;

        if( type != Type::Button )
        {
            description = CreateWindowW( L"STATIC", value.name.c_str(), WS_VISIBLE | WS_CHILD | ES_CENTER,
                                         x, y, w, h, host, nullptr, hInst, nullptr );
        }
        else
        {
            description = nullptr;
        }

        self = ( HMENU )( long long unsigned )( unsigned )index++;

        auto initialValueOpt = getInitString();
        std::wstring initialValue = initialValueOpt ? *initialValueOpt : L"";

        switch( type )
        {
        case Type::Text:
            {
                edit = CreateWindowW( L"EDIT", initialValue.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                                      0, 0, 0, 0, host, self, hInst, nullptr );
            }
            break;
        case Type::Integer:
            {
                edit = CreateWindowW( L"EDIT", initialValue.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                                      0, 0, 0, 0, host, self, hInst, nullptr );
                SetWindowSubclass( edit, editSubclassProcInt, 1, 0 );
            }
            break;
        case Type::Float:
            {
                edit = CreateWindowW( L"EDIT", initialValue.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                                      0, 0, 0, 0, host, self, hInst, nullptr );
                SetWindowSubclass( edit, editSubclassProcFloat, 1, 0 );
            }
            break;
        case Type::Option:
            {
                edit = CreateWindowW( L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                      0, 0, 0, 0, hwnd, self, hInst, nullptr );

                if( value.options.empty() )
                    SendMessageW( edit, CB_ADDSTRING, 0, ( LPARAM )L"none" );

                int id = 0, current = 0;
                for( auto& option : value.options )
                {
                    if( option == initialValue )
                        current = id;

                    SendMessageW( edit, CB_ADDSTRING, 0, ( LPARAM )option.c_str() );
                    ++id;
                }

                SendMessageW( edit, CB_SETCURSEL, current, 0 );
            }
            break;
        case Type::Button:
            {
                edit = CreateWindowW( L"BUTTON", value.name.c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      x, y, w, h, host, self, hInst, nullptr );
            }
            break;
        case Type::None:
        default:
            makeException( false );
        }
    }

    ~Field()
    {
        if( edit )
            DestroyWindow( edit );
        if( description )
            DestroyWindow( description );
    }

    void apply()
    {
        if( type == Type::Button )
            return;

        auto& v = value.value;
        makeException( v.has_value() );

        auto string = getString();

        if( v.type() == typeid( std::wstring* ) )
            *std::any_cast<std::wstring*>( v ) = string;
        else if( value.value.type() == typeid( int64_t* ) )
            *std::any_cast<std::int64_t*>( v ) = std::stoll( string );
        else if( value.value.type() == typeid( double* ) )
            *std::any_cast<double*>( v ) = std::stod( string );
        else if( value.value.type() == typeid( bool* ) )
        {
            makeException( value.options.size() == 2 );
            *std::any_cast<bool*>( v ) = string == value.options[1];
        }
        else if( value.value.type() == typeid( uint16_t* ) )
        {
            uint16_t i = 0;
            for( auto& o : value.options )
            {
                if( o == string )
                {
                    *std::any_cast<uint16_t*>( v ) = i;
                    return;
                }
                ++i;
            }
            makeException( false );
        }
        else
        {
            makeException( false );
        }
    }

    std::optional<std::wstring> getInitString()
    {
        auto& v = value.value;
        if( !v.has_value() )
            return {};

        if( v.type() == typeid( std::wstring* ) )
            return *std::any_cast<std::wstring*>( v );
        if( value.value.type() == typeid( int64_t* ) )
            return std::to_wstring( *std::any_cast<std::int64_t*>( v ) );
        if( value.value.type() == typeid( double* ) )
            return std::to_wstring( *std::any_cast<double*>( v ) );
        if( value.value.type() == typeid( bool* ) )
        {
            makeException( value.options.size() == 2 );
            return value.options[*std::any_cast<bool*>( v )];
        }
        if( value.value.type() == typeid( uint16_t* ) )
        {
            auto i = *std::any_cast<uint16_t*>( v );
            makeException( i < value.options.size() );
            return value.options[i];
        }

        makeException( false );
        return {};
    }

    std::wstring getString() const
    {
        switch( type )
        {
        case Type::Text:
        case Type::Float:
        case Type::Integer:
            {
                wchar_t buffer[256] = {};
                GetDlgItemTextW( host, ( int )( unsigned )( long long unsigned )self, buffer, sizeof( buffer ) / sizeof( buffer[0] ) );
                return buffer;
            }
            break;
        case Type::Option:
            {
                wchar_t buffer[256] = {};
                int sel = ( int )SendMessageW( edit, CB_GETCURSEL, 0, 0 );
                if( sel != CB_ERR )
                    SendMessageW( edit, CB_GETLBTEXT, sel, ( LPARAM )buffer );

                return buffer;
            }
            break;
        case Type::Button:
            return buttonInfo;
            break;
        case Type::None:
        default:
            makeException( false );
        }
        return L"";
    }

    bool press( WPARAM wParam )
    {
        HMENU id = ( HMENU )( long long unsigned )LOWORD( wParam );
        if( self != id )
            return false;

        if( type != Type::Button && !value.callback )
            return false;

        WORD code = HIWORD( wParam );
        if( code != BN_CLICKED )
            return false;

        return value.callback( buttonInfo );
    }

    bool size( int width, int /*height*/ )
    {
        if( offset < 0 )
            offset = x;

        auto descriptionHeight = description ? h : 0;

        w = width - x - offset;
        SetWindowPos( edit, nullptr, x, y + descriptionHeight, w, type != Type::Option ? h : 512, SWP_NOZORDER );

        if( description )
            SetWindowPos( description, nullptr, x, y, w, h, SWP_NOZORDER );

        return true;
    }

private:
    Settings::Parameter value;
    std::wstring buttonInfo;
    int x, y, w, h, offset;
    Type type;

    HWND edit = nullptr, host = nullptr, description = nullptr;
    HMENU self = nullptr;
};

class Settings::Implementation
{
public:
    std::vector<std::shared_ptr<Field>> fields;
    Parameters& parameters;
    ATOM windowClass;
    HWND settings;
    WNDCLASSW wc;

    Implementation( WNDPROC windowProc, Parameters& p ) : parameters( p )
    {
        memset( &wc, 0, sizeof( wc ) );
        wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleW( nullptr );
        wc.lpszClassName = L"Settings";
        wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
        wc.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassW( &wc );
    }

    ~Implementation()
    {
        if( windowClass )
            UnregisterClassW( wc.lpszClassName, GetModuleHandleW( nullptr ) );
    }
};

Settings::Settings( std::wstring t, Parameters& p )
    : title( std::move( t ) ), parameters( p )
{
    // This code is positioned in lambda to accesses private members of Settings
    auto windowProc = []( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) -> LRESULT
    {
        auto impl = ( Implementation * )GetWindowLongPtr( hwnd, GWLP_USERDATA );

        switch( message )
        {
        case WM_CREATE:
            {
                INITCOMMONCONTROLSEX iccex;
                iccex.dwSize = sizeof( iccex );
                iccex.dwICC = ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS;
                InitCommonControlsEx( &iccex );

                impl = ( Implementation* )( ( LPCREATESTRUCT )lParam )->lpCreateParams;
                SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )impl );

                int id = 1000;

                int y = 16;
                for( auto& v : impl->parameters )
                {
                    auto h = v.value.has_value() ? 16 : 32;

                    auto& field = *impl->fields.emplace_back( std::make_shared<Field>( v, 16, y, 512, h, hwnd, impl->wc.hInstance, id ) );

                    v.getString = [&field]()
                    {
                        return field.getString();
                    };
                    v.apply = [&field]()
                    {
                        return field.apply();
                    };

                    y += 48;
                }

                return 0;
            }
        case WM_SIZE:
            {
                RECT r;
                GetClientRect( hwnd, &r );
                int width = r.right - r.left;
                int height = r.top - r.bottom;

                for( auto& field : impl->fields )
                    field->size( width, height );

                return 0;
            }
        case WM_COMMAND:
            for( auto& field : impl->fields )
                field->press( wParam );
            return 0;
        case WM_DESTROY:
            {
                impl->fields.clear();
                impl->settings = nullptr;
                impl = nullptr;
                // PostQuitMessage( 0 );
                return 0;
            }
        default:
            break;
        }

        return DefWindowProcW( hwnd, message, wParam, lParam );
    };

    implementation = new Implementation( windowProc, parameters );
}

Settings::~Settings()
{
    delete implementation;
}

void Settings::run()
{
    auto lastWindow = GetForegroundWindow();

    auto settings = implementation->settings = CreateWindowExW( 0, implementation->wc.lpszClassName, title.c_str(),
                    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE | WS_EX_TOPMOST, CW_USEDEFAULT, CW_USEDEFAULT, 300, 1024,
                    nullptr, nullptr, implementation->wc.hInstance, implementation );
    makeException( settings );

    MSG msg;
    BOOL result;
    while( implementation->settings && ( result = GetMessage( &msg, settings, 0, 0 ) ) != 0 )
    {
        if( result == -1 )
        {
            makeException( false );
            break;
        }
        else
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    DWORD windowProcessId = 0;
    GetWindowThreadProcessId( lastWindow, &windowProcessId );
    if( windowProcessId == GetCurrentProcessId() )
        SetForegroundWindow( lastWindow );
}

class ContextMenu::Implementation
{
public:
    Parameters& parameters;
    ATOM windowClass;
    WNDCLASSW wc;
    HWND menu;

    std::vector<std::tuple<UINT_PTR, std::function<void()>>> instances;
    std::vector<HMENU> menus;

    Implementation( WNDPROC windowProc, Parameters& p ) : parameters( p )
    {
        memset( &wc, 0, sizeof( wc ) );
        wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleW( nullptr );
        wc.lpszClassName = L"DropDownMenuHolder";
        wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
        windowClass = RegisterClassW( &wc );
    }

    ~Implementation()
    {
        if( windowClass )
            UnregisterClassW( wc.lpszClassName, GetModuleHandleW( nullptr ) );
    }
};

static HMENU dropDown( ContextMenu::Implementation& impl, const ContextMenu::Parameters& parameters )
{
    static UINT_PTR index = 1001;

    if( parameters.empty() )
        return nullptr;

    auto popupMenu = CreatePopupMenu();
    for( auto& p : parameters )
    {
        if( auto subMenu = dropDown( impl, p.parameters ) )
        {
            AppendMenuW( popupMenu, MF_POPUP, ( UINT_PTR )subMenu, p.name.c_str() );
        }
        else
        {
            AppendMenuW( popupMenu, p.active ? MF_STRING : MF_GRAYED, index, p.name.c_str() );
            impl.instances.emplace_back( index, p.callback );
            ++index;
        }
    }

    impl.menus.emplace_back( popupMenu );
    return popupMenu;
}

ContextMenu::ContextMenu( Parameters p ) : parameters( std::move( p ) )
{
    auto windowProc = []( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) -> LRESULT
    {
        auto impl = ( Implementation * )GetWindowLongPtr( hwnd, GWLP_USERDATA );

        switch( message )
        {
        case WM_CREATE:
            {
                impl = ( Implementation * )( ( LPCREATESTRUCT )lParam )->lpCreateParams;
                SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )impl );
                dropDown( *impl, impl->parameters );
                return 0;
            }
        case WM_APP_SHOWMENU:
            {
                if( impl->menus.empty() )
                    break;

                POINT pt;
                if( lParam == -1 ) GetCursorPos( &pt );
                else
                {
                    pt.x = LOWORD( lParam );
                    pt.y = HIWORD( lParam );
                }

                ShowWindow( hwnd, SW_SHOWNOACTIVATE );
                SetWindowPos( hwnd, HWND_TOPMOST, pt.x, pt.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE );
                SetForegroundWindow( hwnd );

                auto tracked = TrackPopupMenuEx( impl->menus.back(), TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, hwnd, nullptr );
                for( auto& [index, callback] : impl->instances )
                {
                    if( callback && tracked == index )
                        callback();
                }
                return 0;
            }
        case WM_EXITMENULOOP:
            {
                DestroyWindow( hwnd );
                MessageBoxW( hwnd, L"Clicked away", L"Menu", MB_OK );
                return 0;
            }
        case WM_DESTROY:
            for( auto menu : impl->menus )
            {
                DestroyMenu( menu );
            }
            impl->menus.clear();
            impl->menu = nullptr;
            impl = nullptr;
            // PostQuitMessage( 0 );
            return 0;
        default:
            break;
        }
        return DefWindowProcW( hwnd, message, wParam, lParam );
    };

    implementation = new Implementation( windowProc, parameters );
}

ContextMenu::~ContextMenu()
{
    delete implementation;
}

void ContextMenu::run()
{
    POINT point;
    GetCursorPos( &point );

    auto menu = implementation->menu = CreateWindowExW( WS_EX_TOOLWINDOW | WS_EX_TOPMOST, implementation->wc.lpszClassName, L"",
                                       WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 8, 8, nullptr, nullptr, implementation->wc.hInstance, implementation );
    makeException( menu );

    LPARAM l = MAKELPARAM( point.x, point.y );
    PostMessageW( menu, WM_APP_SHOWMENU, 0, l );

    MSG msg;
    BOOL result;
    while( implementation->menu && ( result = GetMessage( &msg, nullptr, 0, 0 ) ) != 0 )
    {
        if( result == -1 )
        {
            makeException( false );
            break;
        }
        else
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }
}
