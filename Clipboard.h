#pragma once

#include <cstdint>
#include <variant>
#include <vector>
#include <string>

#include "Image/Translate.h"

namespace Clipboard
{
using Empty = std::monostate;
using Text = std::wstring;
using Image = ImageConvert::Reference;

using Item = std::variant<Empty, Text, Image>;

bool output( const Item &item );
bool input( Item &item );

bool isEmpty();
}
