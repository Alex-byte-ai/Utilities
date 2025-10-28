#include "Tests.h"

Tests::Tests( Console &console, Pause &pause, const Information::Item &information )
    : context( console, pause, information )
{}

void Tests::operator()( std::function<void( Context & )> function )
{
    functions.emplace_back( std::move( function ) );
}

void Tests::run()
{
    std::vector<std::optional<std::wstring>> results;
    for( const auto &function : functions )
    {
        try
        {
            function( context );
        }
        catch( const Exception &e )
        {
            context.error = e.message();
        }
        catch( const std::exception &e )
        {
            context.error = Exception::extract( e.what() );
        }
        catch( ... )
        {
            context.error = L"";
        }

        std::optional<std::wstring> status;
        try
        {
            status = context.Status();
        }
        catch( ... )
        {
            status.reset();
        }
        results.push_back( status );
        context.error.reset();
    }

    if( !results.empty() )
        context.output() << L"\n";

    for( const auto &result : results )
    {
        if( result )
        {
            context.output() << *result;
        }
        else
        {
            context.output() << L"Can't determine test status.\n";
        }
    }
}
