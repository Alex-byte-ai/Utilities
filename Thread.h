#pragma once

#include <functional>

#include "Lambda.h"

class Thread
{
public:
    Thread();

    // Destructor calls 'stop', but 'stop' should be called before data, thread is using, is destroyed.
    ~Thread();

    Thread( const Thread & ) = delete;
    Thread &operator=( const Thread & ) = delete;

    // 'function' returns true, if it needs to be called again.
    bool launch( std::function<bool()> function = nullptr );

    void stop();

    bool running() const;

    // This function checks, if it's called from within this thread
    bool inside();

    // Pauses for the current scope, if it's outside of this thread, returns true, if it did something at all.
    bool pauseForScope( Finalizer<bool> &_ );

    static void sleep( unsigned milliseconds = 50 );
private:
    bool commandRun, stateRun;
    std::function<bool()> f;
    long long unsigned id;
};
