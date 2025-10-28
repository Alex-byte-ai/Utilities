#pragma once

class EntityCounter
{
public:
    EntityCounter();
    virtual ~EntityCounter();

    static int Count();
private:
    static int count;
};
