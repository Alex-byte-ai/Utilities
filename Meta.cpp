#include "Meta.h"

#include <stdexcept>
#include <cwctype>

std::wstring Storage::clean( const wchar_t *code )
{
    std::wstring result;
    bool insideString = false;
    wchar_t prevChar = 0;
    size_t pos = 0;
    std::wstring input( code );

    while( pos < input.size() )
    {
        if( !insideString )
        {
            // Check if we are entering a string
            if( input[pos] == L'"' )
            {
                insideString = true;
                result.push_back( input[pos] );
                ++pos;
                continue;
            }

            // Detect "Storage::absorb<"
            if( input.compare( pos, 16, L"Storage::absorb<" ) == 0 )
            {
                pos += 16; // Move past "Storage::absorb<"
                // Skip the template argument
                while( pos < input.size() && input[pos] != L'>' )
                    ++pos;
                ++pos; // Skip '>'

                // Ensure '(' follows after 'Storage::absorb'
                makeException( pos < input.size() && input[pos] == L'(' );

                ++pos; // Skip '('

                // Remove whitespace after '('
                while( pos < input.size() && std::iswspace( input[pos] ) )
                    ++pos;

                // Extract the argument inside parentheses
                std::wstring argument;
                int depth = 1; // Tracks nested parentheses
                while( pos < input.size() && depth > 0 )
                {
                    if( input[pos] == L'(' )
                    {
                        depth++;
                    }
                    else if( input[pos] == L')' )
                    {
                        depth--;
                        if( depth == 0 )
                        {
                            ++pos;
                            break; // Stop after the closing parenthesis
                        }
                    }

                    if( depth > 0 )
                    {
                        argument.push_back( input[pos] );
                    }
                    ++pos;
                }

                // Unbalanced parentheses in argument
                makeException( depth == 0 );

                // Remove trailing whitespace from the argument
                size_t endPos = argument.size();
                while( endPos > 0 && std::iswspace( argument[endPos - 1] ) )
                {
                    endPos--;
                }
                argument.resize( endPos );

                // Recursively process the extracted argument
                result.append( clean( argument.c_str() ) ); // Replace the absorb call with the processed argument
                continue;
            }

            // Add regular characters to the result
            result.push_back( input[pos] );
        }
        else
        {
            // Inside a string, handle escaped quotes
            if( input[pos] == L'"' && prevChar != L'\\' )
            {
                insideString = false; // End of the string
            }

            result.push_back( input[pos] );
        }

        // Update previous character and move to the next position
        prevChar = input[pos];
        pos++;
    }

    return result;
}
