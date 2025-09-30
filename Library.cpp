#include "Library.h"

#include <windows.h>

#include <functional>
#include <optional>
#include <string>
#include <set>

#include "Basic.h"

static std::string sectionName( BYTE *sectionNameBytes )
{
    char name[9];
    name[8] = '\0';
    copy( name, sectionNameBytes, 8 );
    return name;
}

static void parseModule( HMODULE dllHandle, std::set<Library::Function> &functions )
{
    // Module
    auto pImageDosHeader = ( PIMAGE_DOS_HEADER )dllHandle;
    auto pImageNtHeaders = ( PIMAGE_NT_HEADERS )( ( BYTE * )dllHandle + pImageDosHeader->e_lfanew );

    if( pImageNtHeaders->Signature != IMAGE_NT_SIGNATURE )
        return;

    // Extract sections
    auto pSection = IMAGE_FIRST_SECTION( pImageNtHeaders );
    for( int i = 0; i < pImageNtHeaders->FileHeader.NumberOfSections; i++ )
    {
        if( sectionName( pSection[i].Name ) == ".funcs" )
        {
            // Extract functions
            Library::Function *exported = ( Library::Function * )( ( BYTE * )dllHandle + pSection[i].VirtualAddress );
            if( exported )
            {
                for( Library::Function *function = exported; function->name != nullptr; ++function )
                    functions.emplace( *function );
            }
        }
    }
}

class Library::Data
{
public:
    HMODULE dllHandle;
    std::set<Library::Function> functions;

    Data( const std::filesystem::path &fileName )
    {
        dllHandle = LoadLibraryW( fileName.c_str() );
        if( !dllHandle )
            return;

        parseModule( dllHandle, functions );
    }

    ~Data()
    {
        if( dllHandle )
            FreeLibrary( dllHandle );
    }
};

Library::Library( const std::filesystem::path &fileName )
{
    data = new Data( fileName );
}

Library::~Library()
{
    delete data;
}

std::vector<std::string> Library::functions() const
{
    std::vector<std::string> result;
    result.reserve( data->functions.size() );
    for( const auto &function : data->functions )
        result.emplace_back( function.name );
    return result;
}

bool Library::call( const std::string &functionName, const void *arguments, void *result ) const
{
    if( !data->dllHandle )
        return false;

    auto i = data->functions.find( { functionName.c_str(), nullptr } );
    if( i == data->functions.end() )
        return false;

    auto function = i->address;
    if( !function )
        return false;

    function( arguments, result );
    return true;
}
