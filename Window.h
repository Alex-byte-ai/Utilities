#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

class Popup
{
public:
    // Message types
    enum class Type
    {
        Info,
        Error,
        Warning,
        Question
    };

    // Setup
    std::wstring title;
    std::wstring information;
    Type type;

    // User response
    std::optional<bool> answer;

    Popup( Type type = Type::Info, std::wstring title = L"", std::wstring information = L"" );

    // Display the popup and capture the user's response
    void run();
};

class Settings
{
private:
    class Implementation;
    Implementation *implementation;
public:
    struct Parameter
    {
        std::vector<std::wstring> options;
        std::wstring name;

        std::function<bool( const std::wstring& )> set;
        std::function<std::wstring()> get;

        std::function<std::wstring()> input;

        Parameter( std::wstring n, std::function<bool( std::wstring )> s = nullptr, std::function<std::wstring()> g = nullptr, std::vector<std::wstring> o = {} )
            : options( std::move( o ) ), name( std::move( n ) ), set( std::move( s ) ), get( std::move( g ) )
        {}
    };

    using Parameters = std::vector<Parameter>;

    // Setup
    std::wstring title;
    Parameters& parameters;

    Settings( std::wstring title, Parameters& parameters );
    ~Settings();

    // Display the settings and capture the user's input
    void run();
};

class ContextMenu
{
public:
    class Implementation;
private:
    Implementation *implementation;
public:
    struct Parameter
    {
        std::vector<Parameter> parameters;
        std::function<void()> callback;
        std::wstring name;
        bool active;

        Parameter( std::wstring n = L"", bool a = false, std::function<void()> c = nullptr, std::vector<Parameter> p = {} )
            : parameters( std::move( p ) ), callback( std::move( c ) ), name( std::move( n ) ), active( a )
        {}
    };

    using Parameters = std::vector<Parameter>;

    ContextMenu( Parameters parameters );
    ~ContextMenu();

    Parameters parameters;

    void run();
};
