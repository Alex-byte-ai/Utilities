#pragma once

#include <string>
#include <optional>
#include <functional>

#include "Enum.h"

class Pause
{
public:
    class InputType final : public Enum<unsigned, 0b111, InputType>
    {
    protected:
        using Parent = Enum<unsigned, 0b111, InputType>;

        constexpr InputType( unsigned v ) : Parent( v ) {};
    public:
        constexpr InputType( const Parent &other ) : Parent( other ) {};
        constexpr InputType( const InputType &other ) : Parent( other ) {};

        static constexpr Parent any = produce( 0 );
        static constexpr Parent enter = produce( 1 );
        static constexpr Parent shift = produce( 2 );
        static constexpr Parent esc = produce( 3 );
        static constexpr Parent prtSc = produce( 4 );
        static constexpr Parent userInput = produce( 5 );

        static constexpr Parent windowFocused = produce( 8 );
        static constexpr Parent clibpoardHasValue = produce( 16 );
        static constexpr Parent userCondition = produce( 32 );
    };

    Pause();
    ~Pause();

    // Pauses execution. Resumes if a key is pressed and a condition is satisfied.
    void wait( InputType category = InputType::any | InputType::windowFocused, const std::optional<std::wstring> &message = {} ) const;

    // This will be called before pausing. If the return value is false, the pause is canceled.
    std::function<bool( InputType, const std::optional<std::wstring> & )> prepare;

    // If the return value is empty for a certain flag combination, the standard function will be used.
    std::function<std::optional<bool>( InputType )> process;
};
