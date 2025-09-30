#pragma once

#include <optional>
#include <string>

class Popup
{
public:
    // Enum for message types
    enum class Type
    {
        Info,
        Error,
        Warning,
        Question
    };

    // Setting
    std::wstring title;
    std::wstring information;
    Type type;

    // User response
    std::optional<bool> answer;

    Popup( Type type = Type::Info, const std::wstring &title = L"", const std::wstring &information = L"" );

    // Display the popup and capture the user's response
    void run();
};
