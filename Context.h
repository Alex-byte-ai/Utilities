#pragma once

#include <functional>
#include <filesystem>
#include <optional>
#include <string>
#include <stack>
#include <map>

#include "Information.h"
#include "Exception.h"
#include "Console.h"
#include "Pause.h"

// Paths to directories for reading/writing should be in Context.cpp

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

    Console &output();
    const Information::Item &information;
    const Pause &pause;

    Scope scope( std::string description );

    std::optional<std::wstring> error;
private:
    std::optional<std::filesystem::path> saveDirectory;
    std::optional<std::string> description;
    std::stack<Scope *> scopes;
    Console& out;

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
