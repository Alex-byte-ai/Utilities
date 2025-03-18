#pragma once

#include <cstdint>

using BitList = uint64_t;
void readBits( const uint8_t *&pointer, unsigned &bitOffset, unsigned bits, BitList &value );
void writeBits( uint8_t *&pointer, unsigned &bitOffset, unsigned bits, BitList value );
