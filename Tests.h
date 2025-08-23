#pragma once

#include <functional>

#include "Context.h"

class Tests
{
public:
    Tests( Console &console, Pause &pause, const Information::Item &information );
    void operator()( std::function<void( Context & )> function );
    void run();
private:
    std::vector<std::function<void( Context & )>> functions;
    Context context;
};
