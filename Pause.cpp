#include "Pause.h"

#include <windows.h>

#include <cstdlib>

#include "Exception.h"
#include "Clipboard.h"
#include "Thread.h"

static bool thisKey( UINT key )
{
    //return watchKeyboard.allKeys[key];
    return GetAsyncKeyState( key ) < 0;
}

static bool anyKey()
{
    for( UINT key = 8; key < 256; ++key )
    {
        if( thisKey( key ) )
            return true;
    }
    return false;
}

static void avoidOverload()
{
    // Avoid CPU overload
    Thread::sleep();
}

static bool isFocused()
{
    DWORD windowProcessId;
    GetWindowThreadProcessId( GetForegroundWindow(), &windowProcessId );
    return windowProcessId == GetCurrentProcessId();
}

Pause::Pause()
{}

Pause::~Pause()
{}

static bool defaultProcess( Pause::InputType category )
{
    switch( category )
    {
    case Pause::InputType::any:
        return anyKey();
    case Pause::InputType::enter:
        return thisKey( VK_RETURN );
    case Pause::InputType::shift:
        return thisKey( VK_SHIFT );
    case Pause::InputType::esc:
        return thisKey( VK_ESCAPE );
    case Pause::InputType::prtSc:
        return thisKey( VK_SNAPSHOT );
    case Pause::InputType::windowFocused:
        return isFocused();
    case Pause::InputType::clibpoardHasValue:
        return !Clipboard::isEmpty();
    default:
        makeException( false );
    }
    return true;
}

void Pause::wait( InputType category, const std::optional<std::wstring> &message ) const
{
    if( prepare )
    {
        if( !prepare( category, message ) )
            return;
    }

    auto synthesisProcess = [&]( InputType c )
    {
        if( !process )
            return defaultProcess( c );
        auto result = process( c );
        if( !result )
            return defaultProcess( c );
        return *result;
    };

    auto condition = [&]()
    {
        bool f = true;
        if( category & InputType::windowFocused )
            f = f && synthesisProcess( InputType::windowFocused );
        if( category & InputType::clibpoardHasValue )
            f = f && synthesisProcess( InputType::clibpoardHasValue );
        if( category & InputType::userCondition )
            f = f && synthesisProcess( InputType::userCondition );
        return f;
    };

    while( synthesisProcess( *category ) )
        avoidOverload();

    do
    {
        while( !synthesisProcess( *category ) )
            avoidOverload();
        Thread::sleep( 100 );
    }
    while( !condition() );
}
