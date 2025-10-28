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

class Field
{
public:
    Field( Settings::Parameter v, int hor, int ver, int width, int height, HWND hwnd, HINSTANCE hInst, int &index )
        : value( std::move( v ) ), x( hor ), y( ver ), w( width ), h( height )
    {
        host = hwnd;
        edit = nullptr;
        offset = -1;

        idleColor = RGB( 255, 255, 255 );
        errorColor = RGB( 255, 127, 127 );
        focusColor = RGB( 255, 255, 127 );

        idle = CreateSolidBrush( idleColor );
        error = CreateSolidBrush( errorColor );
        focus = CreateSolidBrush( focusColor );

        valid = true;
        focused = false;

        if( value.get )
        {
            description = CreateWindowW( L"STATIC", value.name.c_str(), WS_VISIBLE | WS_CHILD | ES_CENTER, x, y, w, h, host, nullptr, hInst, nullptr );
        }
        else
        {
            description = nullptr;
        }

        self = ( HMENU )( long long unsigned )( unsigned )index++;

        if( !value.get )
        {
            edit = CreateWindowW( L"BUTTON", value.name.c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, h, host, self, hInst, nullptr );
            return;
        }

        auto initialValue = value.get();

        if( !value.options.empty() )
        {
            edit = CreateWindowW( L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 0, hwnd, self, hInst, nullptr );

            int id = 0, current = 0;
            for( auto& option : value.options )
            {
                if( option == initialValue )
                    current = id;

                SendMessageW( edit, CB_ADDSTRING, 0, ( LPARAM )option.c_str() );
                ++id;
            }

            SendMessageW( edit, CB_SETCURSEL, current, 0 );
            return;
        }

        edit = CreateWindowW( L"EDIT", initialValue.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT, 0, 0, 0, 0, host, self, hInst, nullptr );
    }

    ~Field()
    {
        if( edit )
            DestroyWindow( edit );
        if( description )
            DestroyWindow( description );
        if( idle )
            DeleteObject( idle );
        if( error )
            DeleteObject( error );
    }

    std::wstring input() const
    {
        if( !value.get )
            return L"";

        if( !value.options.empty() )
        {
            wchar_t buffer[256] = {};
            int sel = ( int )SendMessageW( edit, CB_GETCURSEL, 0, 0 );
            if( sel != CB_ERR )
                SendMessageW( edit, CB_GETLBTEXT, sel, ( LPARAM )buffer );

            return buffer;
        }

        std::wstring buffer;
        int length = GetWindowTextLengthW( edit );
        buffer.resize( length );
        GetWindowTextW( edit, buffer.data(), length + 1 );
        return buffer;
    }

    bool apply( WPARAM wParam )
    {
        if( !value.set )
            return false;

        HMENU id = ( HMENU )( long long unsigned )LOWORD( wParam );
        if( self != id )
            return false;

        WORD code = HIWORD( wParam );

        if( value.get )
        {
            if( value.options.empty() )
            {
                auto update = [&]()
                {
                    InvalidateRect( edit, nullptr, TRUE );
                    UpdateWindow( edit );
                };
                if( code == EN_UPDATE )
                {
                    valid = value.set( input() );
                    update();
                    return true;
                }
                if( code == EN_SETFOCUS )
                {
                    focused = true;
                    update();
                    return true;
                }
                if( code == EN_KILLFOCUS )
                {
                    focused = false;
                    update();
                    return true;
                }
            }
            else
            {
                if( code == CBN_DROPDOWN )
                {
                    COMBOBOXINFO cbi{};
                    cbi.cbSize = sizeof( cbi );
                    if( GetComboBoxInfo( edit, &cbi ) )
                        dropdown = cbi.hwndList;
                    return true;
                }
                if( code == CBN_CLOSEUP )
                {
                    dropdown = nullptr;
                    return true;
                }
                if( code == CBN_SELCHANGE || code == CBN_SELENDOK )
                {
                    value.set( input() );
                    return true;
                }
            }
        }
        else
        {
            if( code == BN_CLICKED )
            {
                value.set( L"" );
                return true;
            }
        }

        return false;
    }

    void size( int width, int /*height*/ )
    {
        if( offset < 0 )
            offset = x;

        auto descriptionHeight = description ? h : 0;

        w = width - x - offset;
        SetWindowPos( edit, nullptr, x, y + descriptionHeight, w, value.options.empty() ? h : 512, SWP_NOZORDER );

        if( description )
            SetWindowPos( description, nullptr, x, y, w, h, SWP_NOZORDER );
    }

    HBRUSH color( HWND window, HDC hdc )
    {
        if( !value.options.empty() )
            return nullptr;

        if( edit != window )
            return nullptr;

        SetBkMode( hdc, OPAQUE );
        SetBkColor( hdc, valid ? ( focused ? focusColor : idleColor ) : errorColor );
        SetTextColor( hdc, RGB( 0, 0, 0 ) );
        return valid ? ( focused ? focus : idle ) : error;
    }

    HWND getDropdown()
    {
        return dropdown;
    }

private:
    Settings::Parameter value;
    int x, y, w, h, offset;

    HWND edit = nullptr, host = nullptr, description = nullptr, dropdown = nullptr;
    HMENU self = nullptr;

    COLORREF idleColor, errorColor, focusColor;
    HBRUSH idle, error, focus;
    bool valid, focused;
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
        settings = nullptr;
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
                    auto h = v.get ? 16 : 32;

                    auto& field = *impl->fields.emplace_back( std::make_shared<Field>( v, 16, y, 512, h, hwnd, impl->wc.hInstance, id ) );

                    v.input = [&field]()
                    {
                        return field.input();
                    };

                    y += 48;
                }
            }
            return 0;
        case WM_SIZE:
            {
                RECT r;
                GetClientRect( hwnd, &r );
                int width = r.right - r.left;
                int height = r.top - r.bottom;
                for( auto& field : impl->fields )
                    field->size( width, height );
            }
            return 0;
        case WM_CTLCOLOREDIT:
            for( auto& field : impl->fields )
            {
                if( auto brush = field->color( ( HWND )lParam, ( HDC )wParam ) )
                    return ( LRESULT )brush;
            }
            break;
        case WM_COMMAND:
            for( auto& field : impl->fields )
            {
                if( field->apply( wParam ) )
                    return 0;
            }
            break;
        case WM_DESTROY:
            {
                impl->fields.clear();
                impl->settings = nullptr;
                impl = nullptr;
                // PostQuitMessage( 0 );
            }
            return 0;
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

    auto handle = [&]( HWND window )
    {
        if( !window )
            return false;

        MSG msg;
        while( PeekMessageW( &msg, window, 0, 0, PM_NOREMOVE ) )
        {
            if( msg.message == WM_QUIT )
                return true;

            if( PeekMessageW( &msg, window, 0, 0, PM_REMOVE ) )
            {
                TranslateMessage( &msg );
                DispatchMessageW( &msg );
            }
        }
        return false;
    };

    auto cycle = [&]()
    {
        while( true )
        {
            if( !implementation->settings )
                return;

            // Wait for any input/message
            MsgWaitForMultipleObjects( 0, nullptr, FALSE, INFINITE, QS_ALLINPUT );

            MSG msg;
            if( PeekMessageW( &msg, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE ) )
                return;

            if( handle( settings ) )
                return;

            for( auto& field : implementation->fields )
            {
                if( handle( field->getDropdown() ) )
                    return;
            }
        }
    };

    cycle();

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

    std::vector<std::tuple<int, std::function<void()>>> instances;
    std::vector<HMENU> menus;

    Implementation( WNDPROC windowProc, Parameters& p ) : parameters( p )
    {
        memset( &wc, 0, sizeof( wc ) );
        wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleW( nullptr );
        wc.lpszClassName = L"DropDownMenuHolder";
        wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
        windowClass = RegisterClassW( &wc );
        menu = nullptr;
    }

    ~Implementation()
    {
        if( windowClass )
            UnregisterClassW( wc.lpszClassName, GetModuleHandleW( nullptr ) );
    }
};

static HMENU dropDown( ContextMenu::Implementation& impl, const ContextMenu::Parameters& parameters )
{
    static int index = 1001;

    if( parameters.empty() )
        return nullptr;

    HMENU popupMenu = CreatePopupMenu(), subMenu;
    for( auto& p : parameters )
    {
        if( p.name.empty() )
        {
            AppendMenuW( popupMenu, MF_SEPARATOR, 0, nullptr );
            continue;
        }

        if( p.active && ( subMenu = dropDown( impl, p.parameters ) ) )
        {
            AppendMenuW( popupMenu, MF_POPUP, ( UINT_PTR )subMenu, p.name.c_str() );
            continue;
        }

        AppendMenuW( popupMenu, p.active ? MF_STRING : MF_GRAYED, ( UINT_PTR )index, p.name.c_str() );
        impl.instances.emplace_back( index, p.callback );
        ++index;
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
        makeException( result != -1 );
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }
}
