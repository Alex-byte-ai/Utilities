#pragma once

#include <functional>
#include <filesystem>
#include <optional>
#include <string>
#include <stack>
#include <map>

#include "UnicodeString.h"
#include "Information.h"
#include "Exception.h"
#include "Console.h"
#include "Pause.h"

// Paths to directories for reading/writing should be in Context.cpp

class ConsoleOutput
{
public:
    inline ConsoleOutput( Console &c, std::optional<std::filesystem::path> s )
        : console( c ), saveDirectory( std::move( s ) )
    {}

    inline ~ConsoleOutput()
    {
        if( saveDirectory )
            console.save( *saveDirectory );
    }

    inline ConsoleOutput &operator<<( const wchar_t *data )
    {
        console( data );
        return *this;
    }

    inline ConsoleOutput &operator<<( const std::wstring &data )
    {
        console( data );
        return *this;
    }

    template<typename T>
    inline ConsoleOutput &operator<<( const T &data )
    {
        buffer.Clear();
        buffer << data;
        std::wstring result;
        makeException( buffer.EncodeW( result ) );
        console( result );
        return *this;
    }

    inline void operator++()
    {
        ++console;
    }

    inline void operator--()
    {
        --console;
    }

    inline void color( const std::optional<Console::Color> &c )
    {
        console.color( c );
    }

    inline void numericBase( short int value )
    {
        buffer.numericBase( value );
    }

    inline short int numericBase()
    {
        return buffer.numericBase();
    }

    inline void showBase( std::optional<short int> value )
    {
        buffer.showBase( value );
    }

    inline std::optional<short int> showBase()
    {
        return buffer.showBase();
    }
private:
    String buffer;
    Console &console;
    std::optional<std::filesystem::path> saveDirectory;
};

class Scope;

class Context
{
public:
    static void Standard( const std::string &input, std::string &output );

    Context( Console &console, Pause &pause, const Information::Item &information );
    virtual ~Context();

    std::string Identity() const;
    std::string Standard() const;

    std::filesystem::path Input() const;
    std::filesystem::path Output() const;

    std::string Opening() const;
    std::string Closing() const;

    std::wstring Status() const;

    void Open();
    void Close();

    ConsoleOutput &output();
    const Information::Item &information;
    const Pause &pause;

    Scope scope( std::string description );

    std::optional<std::wstring> error;
private:
    std::optional<std::string> description;
    std::stack<Scope *> scopes;
    ConsoleOutput out;

    friend Scope;
};

class Scope
{
public:
    ~Scope();
private:
    Scope( Context &context, std::string description );

    std::string description;
    Context &context;

    friend Context;
};
