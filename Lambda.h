#pragma once

#include <functional>
#include <optional>
#include <vector>
#include <stack>

// Expands std::vector, if nescessary
template<typename T>
class Expander
{
public:
    Expander( std::vector<T> &d ) : data( d )
    {};

    ~Expander()
    {}

    template<typename P>
    [[nodiscard]] bool operator()( size_t minSize, P *&pointer, size_t &size )
    {
        if( data.capacity() < minSize )
            data.reserve( std::max( data.capacity() * 2, 1024ull ) );
        if( data.size() < minSize )
        {
            data.resize( minSize, 0 );
            pointer = data.data();
            size = data.size();
        }
        return minSize <= data.size();
    }

    [[nodiscard]] bool operator()( size_t minSize )
    {
        size_t size;
        T *pointer;
        return ( *this )( minSize, pointer, size );
    }
private:
    std::vector<T> &data;
};

// Calls all added actions in reverse order, compared to order of addition
// Don't capture references to local variables in lambdas for this class, it activates them in destructor
// Local variables might be destroyed at that point
template<typename T = bool>
class Finalizer
{
public:
    using Key = std::optional<T>;
    using Lock = std::optional<T>;

    class Item
    {
    public:
        Item( std::function<void()> v, Lock l )
            : value( std::move( v ) ), lock( std::move( l ) )
        {}

        void operator()( const Key &key ) const
        {
            if( !lock || lock == key )
                value();
        }

        bool valid() const
        {
            return value != nullptr;
        }
    private:
        std::function<void()> value;
        Lock lock;
    };

    Finalizer( Key k = {} ) : key( std::move( k ) )
    {}

    const T &operator()( Key k )
    {
        key = std::move( k );
        return *key;
    }

    void push( const Item &onDestroy )
    {
        if( onDestroy.valid() )
            toDoList.push( onDestroy );
    }

    void push( const std::function<void()> &value )
    {
        push( Item( value, {} ) );
    }

    void pop()
    {
        toDoList.pop();
    }

    ~Finalizer()
    {
        while( !toDoList.empty() )
        {
            toDoList.top()( key );
            toDoList.pop();
        }
    }
private:
    std::stack<Item> toDoList;
    Key key;
};
