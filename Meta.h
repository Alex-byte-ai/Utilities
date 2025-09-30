#pragma once

#include <optional>
#include <string>

#include "Exception.h"

namespace Storage
{
template <typename T, unsigned Index>
class Value
{
public:
    static std::optional<T> value;

    static T &absorb( T other )
    {
        makeException( !value );
        value = std::move( other );
        return *value;
    }

    static T extract()
    {
        makeException( value );
        auto result = std::move( *value );
        value.reset();
        return result;
    }

    static bool empty()
    {
        return !value;
    }
};

// Define static member outside the class
template <typename T, unsigned Index>
std::optional<T> Value<T, Index>::value;

// Helper functions to deduce the type
template <unsigned Index, typename T>
T &absorb( T other )
{
    return Value<T, Index>::absorb( std::move( other ) );
}

template <unsigned Index, typename T>
T extract()
{
    return Value<T, Index>::extract();
}

template <unsigned Index, typename T>
bool empty()
{
    return Value<T, Index>::empty();
}

// Removes absorb from strings representing code/expression.
// If you want parentheses around absorbed expressions add them manually.
// Don't use absorb's type template parameter.
// Don't use shortcuts for this namespace and its items.
std::wstring clean( const wchar_t *code );
}

// Executes code and describes it as text
#define executeAndDescribe(description, code) do{description=Storage::clean(L""#code);{code;}}while(false)

// Calculates an expression and describes it as text
#define calculateAndDescribe(description, expression) (([&](){description=Storage::clean(L""#expression);return(expression);})())
