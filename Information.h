﻿#pragma once

#include <type_traits>
#include <stdexcept>
#include <variant>
#include <vector>
#include <string>
#include <deque>
#include <map>

#include "Exception.h"

namespace Information
{
using KeyVerbatim = std::string;
using KeyNumeric = long long unsigned;
using Key = std::variant<KeyVerbatim, KeyNumeric>;

bool verifyVerbatim( const KeyVerbatim &key );

class Item;

class Object
{
public:
    Object();
    Object( const Object &other );
    Object( Object &&other );

    Object &operator=( const Object &other );
    Object &operator=( Object &&other );

    Item &operator()( const KeyVerbatim &key );
    const Item &operator()( const KeyVerbatim &key ) const;
    bool exists( const KeyVerbatim &key ) const;

    bool push( const KeyVerbatim &key, const Item &item );
    bool push( const KeyVerbatim &key, Item &&item );

    unsigned size() const;

    std::map<KeyVerbatim, Item>::const_iterator begin() const;
    std::map<KeyVerbatim, Item>::const_iterator end() const;
private:
    std::map<KeyVerbatim, Item> items;
};

class Array
{
public:
    Array();
    Array( const Array &other );
    Array( Array &&other );

    Array &operator=( const Array &other );
    Array &operator=( Array &&other );

    Item &operator[]( const KeyNumeric &key );
    const Item &operator[]( const KeyNumeric &key ) const;
    bool exists( const KeyNumeric &key ) const;

    bool push( const Item &item );
    bool push( Item &&item );
    bool push( const KeyNumeric &key, const Item &item );
    bool push( const KeyNumeric &key, Item &&item );

    unsigned size() const;

    std::vector<Item>::const_iterator begin() const;
    std::vector<Item>::const_iterator end() const;
private:
    std::vector<Item> items;
};

class String
{
public:
    String( std::wstring s ) : string( std::move( s ) ) {}
    String( const wchar_t *s ) : string( s ) {}
    operator const std::wstring &() const
    {
        return string;
    }
private:
    std::wstring string;
};

class Null
{};

class Wrapper
{
public:
    explicit Wrapper( Item &self );

    Wrapper &operator=( const Item &item );
    Wrapper &operator=( Item &&item );

    template <typename T>
    Wrapper operator=( const T &value )
    {
        Item item;
        item = value;
        collapse( item );
        return *this;
    }

    Wrapper operator()( const KeyVerbatim &key );
    Wrapper operator[]( const KeyNumeric &key );

    template <typename T>
    bool is() const;

    template <typename T>
    T &as();

    template <typename T>
    const T &as() const;
private:
    Wrapper();
    Wrapper( const Wrapper &other );
    bool descend();
    void collapse( Item &item );
    std::deque<Key> keys;
    Item *root, *self;
    Key id;
};

class Item
{
public:
    Item();
    Item( const Item &other );
    Item( Item &&other );

    Wrapper operator()( const KeyVerbatim &key );
    Wrapper operator[]( const KeyNumeric &key );

    const Item &operator()( const KeyVerbatim &key ) const;
    const Item &operator[]( const KeyNumeric &key ) const;

    Item &operator=( const Item &other );
    Item &operator=( Item &&other );

    template <typename T>
    Item &operator=( const T &value )
    {
        item = value;
        return *this;
    }

    template <typename T>
    bool is() const
    {
        return std::holds_alternative<T>( item );
    }

    template <typename T>
    T &as()
    {
        static_assert( !std::is_same_v<T, Null> );
        makeException( is<T>() );
        return std::get<T>( item );
    }

    template <typename T>
    const T &as() const
    {
        static_assert( !std::is_same_v<T, Null> );
        makeException( is<T>() );
        return std::get<T>( item );
    }
private:
    std::variant<bool,
        long long, unsigned long long, long double,
        Object, Array, String, Null> item;
};

template <typename T>
bool Wrapper::is() const
{
    if( self && keys.empty() )
        return self->is<T>();
    return std::is_same_v<T, Null>;
}

template <typename T>
T &Wrapper::as()
{
    static_assert( !std::is_same_v<T, Null> );
    makeException( self && keys.empty() );
    return self->as<T>();
}

template <typename T>
const T &Wrapper::as() const
{
    static_assert( !std::is_same_v<T, Null> );
    makeException( self && keys.empty() );
    return self->as<T>();
}
}
