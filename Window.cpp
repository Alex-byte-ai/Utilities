#include "Window.h"

#include <windows.h>

#include "Exception.h"

Popup::Popup( Type t, const std::wstring &tl, const std::wstring &i )
    : title( tl ), information( i ), type( t )
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
    {
        SetForegroundWindow( lastWindow );
    }
}
