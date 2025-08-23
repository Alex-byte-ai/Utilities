#include "Information.h"

namespace Information
{
// KeyVerbatim
bool verifyVerbatim( const KeyVerbatim &key )
{
    static const KeyVerbatim &allowed = L"_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if( key.empty() )
        return false;
    for( auto &symbol : key )
    {
        if( allowed.find( symbol ) == std::string::npos )
            return false;
    }
    return true;
}

// Object
Object::Object()
{}

Object::Object( const Object &other ) : items( other.items )
{}

Object::Object( Object &&other ) : items( std::move( other.items ) )
{}

Object &Object::operator=( const Object &other )
{
    items = other.items;
    return *this;
}

Object &Object::operator=( Object &&other )
{
    items = std::move( other.items );
    return *this;
}

Item &Object::operator()( const KeyVerbatim &key )
{
    auto i = items.find( key );
    makeException( i != items.end() && !i->second.is<Null>() );
    return i->second;
}

const Item &Object::operator()( const KeyVerbatim &key ) const
{
    auto i = items.find( key );
    makeException( i != items.end() && !i->second.is<Null>() );
    return i->second;
}

bool Object::exists( const KeyVerbatim &key ) const
{
    auto i = items.find( key );
    return i != items.end() && !i->second.is<Null>();
}

void Object::push( const KeyVerbatim &key, const Item &item )
{
    Item copy = item;
    push( key, std::move( copy ) );
}

void Object::push( const KeyVerbatim &key, Item &&item )
{
    makeException( verifyVerbatim( key ) );

    if( item.is<Null>() )
    {
        items.erase( key );
        return;
    }

    auto i = items.find( key );
    if( i == items.end() )
    {
        items.emplace( key, item );
    }
    else
    {
        i->second = item;
    }
}

unsigned Object::size() const
{
    return items.size();
}

std::map<KeyVerbatim, Item>::const_iterator Object::begin() const
{
    return items.cbegin();
}

std::map<KeyVerbatim, Item>::const_iterator Object::end() const
{
    return items.cend();
}

//Array
Array::Array()
{}

Array::Array( const Array &other ): items( other.items )
{}

Array::Array( Array &&other ) : items( std::move( other.items ) )
{};

Array &Array::operator=( const Array &other )
{
    items = other.items;
    return *this;
}

Array &Array::operator=( Array &&other )
{
    items = std::move( other.items );
    return *this;
}

Item &Array::operator[]( const KeyNumeric &key )
{
    makeException( key < items.size() && !items[key].is<Null>() );
    return items[key];
}

const Item &Array::operator[]( const KeyNumeric &key ) const
{
    makeException( key < items.size() && !items[key].is<Null>() );
    return items[key];
}

bool Array::exists( const KeyNumeric &key ) const
{
    return key < items.size() && !items[key].is<Null>();
}

void Array::push( const Item &item )
{
    Item copy = item;
    push( std::move( copy ) );
}

void Array::push( Item &&item )
{
    if( !item.is<Null>() )
        items.emplace_back( item );
}

void Array::push( const KeyNumeric &key, const Item &item )
{
    Item copy = item;
    push( key, std::move( copy ) );
}

void Array::push( const KeyNumeric &key, Item &&item )
{
    if( key >= size() )
    {
        if( item.is<Null>() )
            return;
        makeException( key == size() );
        items.emplace_back( item );
        return;
    }
    if( item.is<Null>() )
    {
        items.erase( items.begin() + key );
        return;
    }
    items[key] = item;
}

unsigned Array::size() const
{
    return items.size();
}

std::vector<Item>::const_iterator Array::begin() const
{
    return items.cbegin();
}

std::vector<Item>::const_iterator Array::end() const
{
    return items.cend();
}

// Wrapper
Wrapper::Wrapper( Item &s ) : root( nullptr ), self( &s )
{}

Wrapper &Wrapper::operator=( const Item &item )
{
    Item copy = item;
    collapse( copy );
    return *this;
}

Wrapper &Wrapper::operator=( Item &&item )
{
    Item transit = item;
    collapse( transit );
    return *this;
}

Wrapper Wrapper::operator()( const KeyVerbatim &key )
{
    makeException( !self || self->is<Null>() || self->is<Object>() );

    if( keys.empty() && self && self->is<Object>() )
    {
        if( self->as<Object>().exists( key ) )
        {
            auto item = &self->as<Object>()( key );
            Wrapper result;
            result.root = self;
            result.self = item;
            result.id = key;
            return result;
        }
    }

    Wrapper result( *this );
    if( root )
    {
        result.keys.emplace_back( key );
    }
    else
    {
        result.root = self;
        result.self = nullptr;
        result.id = key;
    }
    return result;
}

Wrapper Wrapper::operator[]( const KeyNumeric &key )
{
    makeException( !self || self->is<Null>() || self->is<Array>() );

    if( keys.empty() && self && self->is<Array>() )
    {
        if( self->as<Array>().exists( key ) )
        {
            auto item = &self->as<Array>()[key];
            Wrapper result;
            result.root = self;
            result.self = item;
            result.id = key;
            return result;
        }
    }

    Wrapper result( *this );
    if( root )
    {
        result.keys.emplace_back( key );
    }
    else
    {
        result.root = self;
        result.self = nullptr;
        result.id = key;
    }
    return result;
}

Wrapper::Wrapper() : root( nullptr ), self( nullptr )
{}

Wrapper::Wrapper( const Wrapper &other ) : keys( other.keys ), root( other.root ), self( other.self ), id( other.id )
{}

bool Wrapper::descend()
{
    if( !self || self->is<Null>() || keys.empty() )
        return false;

    auto &keyV = keys.front();
    return std::visit( [&]( const auto & key )
    {
        using T = std::decay_t<decltype( key )>;

        Item *item = nullptr;

        if constexpr( std::is_same_v<T, KeyVerbatim> )
        {
            makeException( self->is<Object>() );
            if( self->as<Object>().exists( key ) )
                item = &self->as<Object>()( key );
        }

        if constexpr( std::is_same_v<T, KeyNumeric> )
        {
            makeException( self->is<Array>() );
            if( self->as<Array>().exists( key ) )
                item = &self->as<Array>()[key];
        }

        root = self;
        self = item;
        id = std::move( keyV );
        keys.pop_front();
        return true;
    }, keyV );
}

void Wrapper::collapse( Item &item )
{
    auto place = [this]( Item & i )
    {
        std::visit( [&]( const auto & key )
        {
            using T = std::decay_t<decltype( key )>;

            if constexpr( std::is_same_v<T, KeyVerbatim> )
            {
                if( root->is<Null>() )
                    *root = Object();
                auto &object = root->as<Object>();
                object.push( key, std::move( i ) );
                self = &object( key );
            }
            if constexpr( std::is_same_v<T, KeyNumeric> )
            {
                if( root->is<Null>() )
                    *root = Array();
                auto &array = root->as<Array>();
                array.push( key, std::move( i ) );
                self = &array[key];
            }
        }, id );

        if( keys.empty() )
            return false;

        i = std::move( *self );
        *self = Null();

        root = self;
        self = nullptr;
        id = std::move( keys.front() );
        keys.pop_front();
        return true;
    };

    if( is<Null>() && item.is<Null>() )
        return;

    while( descend() )
    {}

    while( place( item ) )
    {}
}

// Item
Item::Item() : item( Null() ) {}

Item::Item( const Item &other ) : item( other.item )
{}

Item::Item( Item &&other ) : item( std::move( other.item ) )
{}

Wrapper Item::operator()( const KeyVerbatim &key )
{
    return Wrapper( *this )( key );
}

Wrapper Item::operator[]( const KeyNumeric &key )
{
    return Wrapper( *this )[key];
}

const Item &Item::operator()( const KeyVerbatim &key ) const
{
    return as<Object>()( key );
}

const Item &Item::operator[]( const KeyNumeric &key ) const
{
    return as<Array>()[key];
}

Item &Item::operator=( const Item &other )
{
    item = other.item;
    return *this;
}

Item &Item::operator=( Item &&other )
{
    item = std::move( other.item );
    return *this;
}
}
