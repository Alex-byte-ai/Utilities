#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <any>

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
    // Field types
    enum class Type
    {
        Wstring,
        Int64,
        Double,
        Bool,
        Enum16,
        Button
    };

    struct Parameter
    {
        std::function<bool( std::wstring& )> callback;
        std::vector<std::wstring> options;
        std::wstring name;
        std::any value;

        std::function<std::wstring()> getString;
        std::function<void()> apply;

        Parameter( std::wstring n, std::any v, std::vector<std::wstring> o = {}, std::function<bool( std::wstring& )> c = nullptr )
            : callback( std::move( c ) ), options( std::move( o ) ), name( std::move( n ) ), value( std::move( v ) )
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

        Parameter( std::wstring n, bool a = true, std::function<void()> c = nullptr, std::vector<Parameter> p = {} )
            : parameters( std::move( p ) ), callback( std::move( c ) ), name( std::move( n ) ), active( a )
        {}
    };

    using Parameters = std::vector<Parameter>;

    ContextMenu( Parameters parameters );
    ~ContextMenu();

    Parameters parameters;

    void run();
};
