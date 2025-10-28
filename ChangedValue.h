#pragma once

template<typename T>
class ChangedValue
{
private:
    T value;
    bool change;
public:
    ChangedValue() : value(), change( false ) {}

    ChangedValue( T &&v ) : value( v ), change( false ) {}

    ChangedValue( const T &v ) : value( v ), change( false ) {}

    ChangedValue &operator=( const T &other )
    {
        if( value != other )
            change = true;
        value = other;
        return *this;
    }

    ChangedValue &operator=( const ChangedValue &other )
    {
        return *this = other.value;
    }

    const T &operator*() const
    {
        return value;
    }

    const T &operator*()
    {
        return value;
    }

    const T *operator->() const
    {
        return &value;
    }

    const T *operator->()
    {
        return &value;
    }

    T &get()
    {
        change = true;
        return value;
    }

    bool changed() const
    {
        return change;
    }

    bool reset()
    {
        bool result = change;
        change = false;
        return result;
    }
};

// Specialization for reference types
template<typename T>
class ChangedValue<T &>
{
private:
    T *value;
    bool change;
public:
    ChangedValue( T &v ) : value( &v ), change( false ) {}

    ChangedValue &operator=( T &other )
    {
        if( *value != other )
            change = true;
        value = &other;
        return *this;
    }

    ChangedValue &operator=( const ChangedValue &other )
    {
        return *this = *other.value;
    }

    const T &operator*() const
    {
        return *value;
    }

    const T &operator*()
    {
        return *value;
    }

    const T *operator->() const
    {
        return value;
    }

    const T *operator->()
    {
        return value;
    }

    T &get()
    {
        change = true;
        return *value;
    }

    bool changed() const
    {
        return change;
    }

    bool reset()
    {
        bool result = change;
        change = false;
        return result;
    }
};
