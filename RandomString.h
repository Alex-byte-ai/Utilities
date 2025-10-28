#pragma once

#include <string>

#include "RandomNumber.h"

// Output will contain n random symbols from sample
void RandomString( RandomNumber &number, const std::string &sample, std::string &output, unsigned n, bool clearOutput = true );
void RandomWString( RandomNumber &number, const std::wstring &sample, std::wstring &output, unsigned n, bool clearOutput = true );
