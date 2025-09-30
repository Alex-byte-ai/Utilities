#pragma GCC push_options
#pragma GCC optimize("O0")

// This code can't be optimized with preserving functionality

#include "Thread.h"

#include <thread>

Thread::Thread()
{
    commandRun = false;
    stateRun = false;
}

Thread::~Thread()
{
    stop();
}

bool Thread::launch( std::function<bool()> function )
{
    if( inside() )
        return false;

    if( !f && !function )
        return false;

    // If thread is running or it was not stopped
    // even after it finished all actions, it can't be launched again
    if( stateRun || commandRun )
        return false;

    if( function )
        f = std::move( function );

    id = {};
    commandRun = true;
    stateRun = true;
    std::thread( [this]()
    {
        id = std::hash<std::thread::id> {}( std::this_thread::get_id() );
        bool go = true; // Needed, so stop command, stops thread for good.
        while( commandRun && stateRun && go )
        {
            try
            {
                go = f() && go && commandRun;
            }
            catch( ... )
            {
                go = false;
            }
        }
        stateRun = false;
    } ).detach();

    return true;
}

void Thread::stop()
{
    if( inside() )
        std::terminate();

    commandRun = false;
    while( running() )
    {}
}

bool Thread::running() const
{
    return stateRun;
}

bool Thread::inside()
{
    return id == std::hash<std::thread::id> {}( std::this_thread::get_id() );
}

bool Thread::pauseForScope( Finalizer<bool> &_ )
{
    if( running() && !inside() )
    {
        stop();
        _.push( [this]()
        {
            launch();
        } );
        return true;
    }
    return false;
}

void Thread::sleep( unsigned milliseconds )
{
    std::this_thread::sleep_for( std::chrono::milliseconds( milliseconds ) );
}

#pragma GCC pop_options
