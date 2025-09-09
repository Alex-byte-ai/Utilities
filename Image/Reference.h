#pragma once

#include <functional>
#include <optional>
#include <string>

namespace ImageConvert
{
// The Reference class represents an image
class Reference
{
public:
    // If defined, reset must update link in accordance with other settings and return true on success
    std::function<bool( Reference & )> reset;

    // If defined, clear must deallocate data, will be called when Reference is destroyed
    std::function<void( Reference & )> clear;

    // Channels ('A' - 'Z', '_') are described by channel names followed by their sizes in bits
    // For example R8G8B8A8 or R3G3B2
    // Channel '_' is treated as unused, it's not read and is filled with zeros when written
    // Commands:
    // *PAD allows controlling line padding
    // *SAME will make destination use same format as source has, everything else will be ignored, this command is ignored for source
    // *REP allows assigning different source channel's values or a constant to a destination's channel, if it's missing in source, ignored for source
    // *ALPHA sets name of alpha channel, use '_' to not treat any channel as alpha channel, default value is 'A', ignored for source, uses target's setting instead
    // Formats:
    // Can be added to format string, channels and *PAD will be ignored
    // When format is added first bytes at 'source.link'/'destination.link' should have header(s) before/after reading/writing
    // '.BMP' to process data of 'bmp' files
    // '.DIB' same as 'bmp', but does not contain file header
    // '.PNG' to process data of 'png' files
    // '.JPG' to process data of 'jpg' files
    // '.ANYF' program will make a guess, when reading, and use default format for writing. Only works for file contents
    std::optional<std::string> format;

    // Number of bytes stored at the address
    unsigned bytes;

    // Pointer to data
    void *link;

    // Dimensions
    int w, h;

    Reference();
    Reference( Reference &&other ) noexcept;
    ~Reference();

    Reference &operator=( Reference &&other ) noexcept;

    bool operator==( const Reference &other ) const;

    // Creates reference to internal data, which starts empty
    void fill();
};
}
