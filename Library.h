#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Library
{
public:
    struct Function
    {
        const char *name;
        void ( *address )( const void *, void * );

        Function( const char *n, void ( *a )( const void *, void * ) )
            : name( n ), address( a )
        {}

        bool operator<( const Library::Function &other ) const
        {
            return std::string( name ) < other.name;
        }
    };

    class Data;

    Library( const std::filesystem::path &fileName );
    ~Library();

    std::vector<std::string> functions() const;
    bool call( const std::string &functionName, const void *arguments, void *result ) const;
private:
    Data *data;
};
