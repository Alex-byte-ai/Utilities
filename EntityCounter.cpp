#include "EntityCounter.h"

EntityCounter::EntityCounter()
{
    ++count;
}

EntityCounter::~EntityCounter()
{
    --count;
}

int EntityCounter::Count()
{
    return count;
}

int EntityCounter::count = 0;
