#include "Context.h"

#include <filesystem>

#include "Exception.h"

void Context::Standard( const std::string &input, std::string &output )
{
    bool underscore = false;
    for( auto symbol : input )
    {
        if( symbol == '_' )
        {
            if( underscore )
                output += '_';
            underscore = !underscore;
            continue;
        }
        if( underscore )
        {
            output += ' ';
            underscore = false;
        }
        output += symbol;
    }
}

static std::optional<std::filesystem::path> formConsoleSavePath( Context &context )
{
    auto writeDisk = context.information( L"writeDisk" );
    if( writeDisk.as<bool>() )
        return context.Output() / L"console.txt";
    return {};
}

Context::Context( Console &console, Pause &p, const Information::Item &i )
    : information( i ), pause( p ), out( console, formConsoleSavePath( *this ) )
{}

Context::~Context()
{}

std::string Context::Identity() const
{
    makeException( description );
    return *description;
}

std::string Context::Standard() const
{
    std::string result;
    Standard( Identity(), result );
    return result;
}

std::filesystem::path Context::Input() const
{
    auto folders = scopes;
    std::filesystem::path path = L".";
    while( !folders.empty() )
    {
        path = std::filesystem::path( folders.top()->description ) / path;
        folders.pop();
    }
    path = std::filesystem::path( L"input" ) / path;
    return path;
}

std::filesystem::path Context::Output() const
{
    auto folders = scopes;
    std::filesystem::path path = L".";
    while( !folders.empty() )
    {
        path = std::filesystem::path( folders.top()->description ) / path;
        folders.pop();
    }
    path = std::filesystem::path( L"output" ) / path;
    return path;
}

std::string Context::Opening() const
{
    return Standard() + ":\n{\n";
}

std::string Context::Closing() const
{
    return "}\n";
}

std::wstring Context::Status() const
{
    std::wstring result = error.has_value() ? L"Failed " : L"Passed ";
    result += Exception::extract( Standard().c_str() ) + L"\n";
    if( error && !error->empty() )
        result += L"\t" + *error + L"\n";
    return result;
}

void Context::Open()
{
    out << Opening();
    ++out;
}

void Context::Close()
{
    --out;
    out << Closing();
}

ConsoleOutput &Context::output()
{
    return out;
}

Scope Context::scope( std::string d )
{
    return Scope( *this, std::move( d ) );
}

Scope::Scope( Context &c, std::string d )
    : description( std::move( d ) ), context( c )
{
    context.description = description;

    for( auto list : std::vector<bool> {false, true} )
    {
        if( !context.scopes.empty() )
            continue;

        auto listName = list ? L"whitelist" : L"blacklist";
        if( !context.information.as<Information::Object>().exists( listName ) )
            continue;
        auto &listArray = context.information( listName ).as<Information::Array>();

        Unicode::String testName;
        testName << context.Standard();

        std::wstring testNameW;
        if( testName.EncodeW( testNameW ) )
        {
            bool present = false;
            for( const auto &element : listArray )
            {
                if( element.is<Information::String>() && ( ( const std::wstring & )element.as<Information::String>() == testNameW ) )
                    present = true;
            }

            if( present != list )
                throw Exception( list ? L"Is absent in whitelist." : L"Is present in blacklist." );
        }
    }

    context.scopes.push( this );
    context.Open();
}

Scope::~Scope()
{
    context.description = description;
    context.Close();
    context.scopes.pop();
}
