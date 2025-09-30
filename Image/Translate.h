#pragma once

#include "Image/Reference.h"

namespace ImageConvert
{
// Translates (or scales) the source image into the destination image
// Conversion occurs in normalized space (each channel is in [0,1])
// Uses area–weighted scaling
void translate( const Reference &source, Reference &destination, bool scale );
}
