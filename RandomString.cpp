#include "RandomString.h"

template<class T>
void RandomStringBase( RandomNumber &number, const T &sample, T &output, unsigned n, bool clearOutput )
{
    if( clearOutput )
        output.clear();

    if( sample.length() <= 0 )
        return;

    output.reserve( n );
    while( n > 0 )
    {
        output += sample[number.getInteger( 0, sample.length() - 1 )];
        --n;
    }
}

void RandomString( RandomNumber &number, const std::string &sample, std::string &output, unsigned n, bool clearOutput )
{
    RandomStringBase( number, sample, output, n, clearOutput );
}

void RandomWString( RandomNumber &number, const std::wstring &sample, std::wstring &output, unsigned n, bool clearOutput )
{
    RandomStringBase( number, sample, output, n, clearOutput );
}
