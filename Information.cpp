#include "Information.h"

#include <fstream>

#include "UnicodeString.h"
#include "Scanner.h"

namespace Information
{
// KeyVerbatim
bool verifyVerbatim( const KeyVerbatim &key )
{
    if( key.empty() )
        return false;

    size_t i = 0;
    for( auto c : key )
    {
        if( !( ( i > 0 && L'0' <= c && c <= L'9' ) || ( L'a' <= c && c <= L'z' ) || ( L'A' <= c && c <= L'Z' ) || ( c == L'_' ) ) )
            return false;
        ++i;
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
        if( !root )
        {
            if( self )
                *self = i;
            return false;
        }

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

void getItem( Scanner& s, Item & item );
void getObject( Scanner& s, Object & object );
void getArray( Scanner& s, Array & array );

void getItem( Scanner& s, Item & item )
{
    if( s.token.t == Scanner::Name )
    {
        if( s.token.s == "true" )
        {
            item = true;
        }
        else if( s.token.s == "false" )
        {
            item = false;
        }
        else if( s.token.s == "null" )
        {
            item = Null();
        }
        else
        {
            s.token.error( L"Expected a value." );
        }
    }
    else if( s.token.t == Scanner::Int )
    {
        item = s.token.n;
    }
    else if( s.token.t == Scanner::Real )
    {
        item = s.token.x;
    }
    else if( s.token.t == Scanner::Text )
    {
        item = String( ( std::wstring )s.token.s );
    }
    else if( s.token.t == Scanner::BraceO )
    {
        item = Object();
        getObject( s, item.as<Object>() );
    }
    else if( s.token.t == Scanner::BracketO )
    {
        item = Array();
        getArray( s, item.as<Array>() );
    }
    else
    {
        s.token.error( L"Expected a value." );
    }
}

void getObject( Scanner& s, Object & object )
{
    s.token.error( Scanner::BraceO );
    s.getToken();

    if( s.token.t == Scanner::BraceC )
        return;

    while( true )
    {
        s.token.error( Scanner::Name );
        auto key = ( KeyVerbatim )s.token.s;
        s.getToken();

        s.token.error( Scanner::Colon );
        s.getToken();

        Item item;
        getItem( s, item );
        object.push( std::move( key ), std::move( item ) );

        s.getToken();

        if( s.token.t != Scanner::Comma )
            break;

        s.getToken();
    }

    s.token.error( Scanner::BraceC );
};

void getArray( Scanner& s, Array & array )
{
    s.token.error( Scanner::BracketO );
    s.getToken();

    if( s.token.t == Scanner::BracketC )
        return;

    while( true )
    {
        Item item;
        getItem( s, item );
        array.push( std::move( item ) );

        s.getToken();

        if( s.token.t != Scanner::Comma )
            break;

        s.getToken();
    }

    s.token.error( Scanner::BracketC );
};

bool Item::input( const std::filesystem::path &path )
{
    try
    {
        std::ifstream file;
        file.open( path, std::ios::binary );
        Scanner s( file, path.generic_wstring() );
        getItem( s, *this );
        file.close();
    }
    catch( ... )
    {
        return false;
    }
    return true;
}

void setItem( ::String& data, const Item & item, const ::String& tab );
void setObject( ::String& data, const Object & object, const ::String& tab );
void setArray( ::String& data, const Array & array, const ::String& tab );
void setString( ::String& data, const String & string );

void setItem( ::String& data, const Item & item, const ::String& tab )
{
    if( item.is<bool>() )
    {
        data << item.as<bool>();
    }
    else if( item.is<long long>() )
    {
        data << item.as<long long>();
    }
    else if( item.is<unsigned long long>() )
    {
        data << item.as<unsigned long long>();
    }
    else if( item.is<long double>() )
    {
        data << item.as<long double>();
    }
    else if( item.is<Object>() )
    {
        setObject( data, item.as<Object>(), tab );
    }
    else if( item.is<Array>() )
    {
        setArray( data, item.as<Array>(), tab );
    }
    else if( item.is<String>() )
    {
        setString( data, item.as<String>() );
    }
    else if( item.is<Null>() )
    {
        data << "null";
    }
    else
    {
        makeException( false );
    }
};

void setObject( ::String& data, const Object & object, const ::String& tab )
{
    ::String t;
    t << tab << "\t";

    unsigned i = 0, size = object.size();

    data << "{\n";
    for( auto& [key, item] : object )
    {
        data << t << key << ": ";
        setItem( data, item, t );
        ++i;
        data << ( i < size ? ",\n" : "\n" );
    }
    data << tab << "}";
};

void setArray( ::String& data, const Array & array, const ::String& tab )
{
    ::String t;
    t << tab << "\t";

    unsigned i = 0, size = array.size();

    data << "[\n";
    for( auto& item : array )
    {
        data << t;
        setItem( data, item, t );
        ++i;
        data << ( i < size ? ",\n" : "\n" );
    }

    data << tab << "]";
};

void setString( ::String& data, const String & string )
{
    data << "\"";
    for( auto s : ( std::wstring )string )
    {
        if( s == L'\\' || s == L'\"' )
        {
            data << L'\\' << s;
        }
        else if( s == L'\t' )
        {
            data << L"\\t";
        }
        else if( s == L'\n' )
        {
            data << L"\\n";
        }
        else
        {
            data << s;
        }
    }
    data << "\"";
}

bool Item::output( const std::filesystem::path &path ) const
{
    try
    {
        ::String data;
        setItem( data, *this, "" );
        data << "\n";

        size_t pos = 0;
        std::vector<uint8_t> output;
        makeException( data.EncodeUtf8( output, pos, false ) );

        std::filesystem::create_directories( path.parent_path() );
        std::ofstream file( path, std::ios::binary );
        file.write( ( const char* )output.data(), output.size() );
        file.close();
    }
    catch( ... )
    {
        return false;
    }
    return true;
}
}
