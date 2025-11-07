#pragma once

#include <functional>
#include <filesystem>
#include <optional>
#include <cstdint>
#include <string>
#include <vector>

#include "ChangedValue.h"

namespace GraphicInterface
{
struct Box
{
    int x = 0, y = 0, w = 0, h = 0;

    virtual ~Box()
    {}

    bool inside( int x0, int y0 ) const;

    Box& place( const Box& other );

    virtual void draw( uint32_t *pixels, int width, int height ) const = 0;

    void fill( uint32_t *pixels, int width, int height, uint32_t color ) const;
    void gradient( uint32_t *pixels, int width, int height ) const;
};

struct Trigger : public Box
{
    virtual void draw( uint32_t *pixels, int width, int height ) const override;
};

struct Rectangle : public Box
{
    uint32_t color;

    virtual void draw( uint32_t *pixels, int width, int height ) const override;
};

struct Image : virtual public Box
{
    std::vector<uint32_t> pixels;
    int bufferW = 0, bufferH = 0;

    void prepare( int stride, int height );
    void prepare( const void *data, int stride, int height );

    virtual void draw( uint32_t *pixels, int width, int height ) const override;
};

struct StaticText : public Image
{
    std::wstring value;

    void prepare( uint32_t background );
};

struct Active : virtual public Box
{
    // These functions return true, if an object needs focus after that action
    virtual bool hover( int x, int y ) = 0;
    virtual bool click( bool release, int x, int y ) = 0;
    virtual bool input( wchar_t c ) = 0;

    virtual void focus( bool f ) = 0;
};

struct DynamicText : public StaticText, public Active
{
    bool valid = true, focused = false;

    std::function<bool( std::wstring )> setCallback;

    void prepare( bool write = true );

    virtual void draw( uint32_t *pixels, int width, int height ) const override;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void focus( bool f ) override;
};

struct Combobox : public Active
{
    std::vector<std::wstring> options;
    bool isOpen = false;
    size_t option = 0;

    std::function<bool( std::wstring )> setCallback;

    void open( bool f );
    size_t select( int x, int y );

    virtual void draw( uint32_t *pixels, int width, int height ) const override;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void focus( bool f ) override;
};

struct Button : virtual public Box, public Active
{
    std::function<void()> use;
    bool hovered = false;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void focus( bool f ) override;
};

struct TextButton : public Button
{
    std::wstring desc;
    virtual void draw( uint32_t *pixels, int width, int height ) const override;
};

struct MinimizeButton : public Button
{
    virtual void draw( uint32_t *pixels, int width, int height ) const override;
};

struct MaximizeButton : public Button
{
    virtual void draw( uint32_t *pixels, int width, int height ) const override;
};

struct CloseButton : public Button
{
    virtual void draw( uint32_t *pixels, int width, int height ) const override;
};

struct Window : public Box
{
    Window( int h = 24, int sz = 16, int bh = 24, int tgw = 8, int b = 1 );
    Window( const Window &other ) = default;

    int titlebarHeight, buttonSize, buttonSpacingH, buttonSpacingV, triggerWidth, borderWidth;

    Trigger topTrigger, bottomTrigger, leftTrigger, rightTrigger;

    Rectangle titleBar, leftBorder, rightBorder, topBorder, bottomBorder, client;
    Image icon, content;

    StaticText titleOrig;

    MinimizeButton minimizeButton;
    MaximizeButton maximizeButton;
    CloseButton closeButton;

    virtual int minWidth() const;
    virtual int minHeight() const;
    virtual void update();

    void draw( uint32_t *pixels, int width, int height ) const;

    void add( Box *object );

    void run();

    std::vector<Box*> objects;
    std::vector<Active*> interactive;

    Active* focus = nullptr;
};

uint32_t makeColor( uint8_t r, uint8_t g, uint8_t b, uint8_t a );
}

struct Settings : public GraphicInterface::Window
{
public:
    struct Parameter
    {
        std::vector<std::wstring> options;
        std::wstring name;

        std::function<bool( const std::wstring& )> set;
        std::function<std::wstring()> get;

        Parameter( std::wstring n, std::function<bool( const std::wstring& )> s = nullptr, std::function<std::wstring()> g = nullptr, std::vector<std::wstring> o = {} )
            : options( std::move( o ) ), name( std::move( n ) ), set( std::move( s ) ), get( std::move( g ) )
        {}
    };

    using Parameters = std::vector<Parameter>;

    Parameters& parameters;

    std::vector<std::shared_ptr<Box>> fields;

    Settings( std::wstring title, Parameters& parameters );
    ~Settings();

    virtual void update() override;
};

struct Popup : public GraphicInterface::Window
{
public:
    // Message types
    enum class Type
    {
        Info,
        Error,
        Warning,
        Question
    };

    GraphicInterface::TextButton yes, no, cancel;
    GraphicInterface::StaticText info;

    std::vector<GraphicInterface::TextButton*> buttons;

    // User response
    std::optional<bool> answer;

    Popup( Type type = Type::Info, std::wstring title = L"", std::wstring information = L"" );

    virtual void update() override;
};

class ContextMenu
{
public:
    class Implementation;
private:
    Implementation *implementation;
public:
    struct Parameter
    {
        std::vector<Parameter> parameters;
        std::function<void()> callback;
        std::wstring name;
        bool active;

        Parameter( std::wstring n = L"", bool a = false, std::function<void()> c = nullptr, std::vector<Parameter> p = {} )
            : parameters( std::move( p ) ), callback( std::move( c ) ), name( std::move( n ) ), active( a )
        {}
    };

    using Parameters = std::vector<Parameter>;

    ContextMenu( Parameters parameters );
    ~ContextMenu();

    Parameters parameters;

    void run();
};

std::optional<std::filesystem::path> SavePath();
std::optional<std::filesystem::path> OpenPath();

class GenericWindow
{
public:
    class Keys
    {
    private:
        std::array<ChangedValue<bool>, 26> letters;
        std::array<ChangedValue<bool>, 10> digits;
    public:
        ChangedValue<bool> &letter( char symbol );
        const ChangedValue<bool> &letter( char symbol ) const;

        ChangedValue<bool> &digit( unsigned short symbol );
        const ChangedValue<bool> &digit( unsigned short symbol ) const;

        void reset();
        void release();
    };

    class InputData
    {
    public:
        ChangedValue<bool> up, down, left, right, escape, del, shift, ctrl, space, enter, leftMouse, rightMouse, middleMouse, f1;
        ChangedValue<int> mouseX{ -1 }, mouseY{ -1 };
        bool init = false;
        Keys keys;
    };

    class OutputData
    {
    public:
        ChangedValue<GraphicInterface::Image &> image;
        ChangedValue<int> x{ 0 }, y{ 0 };
        ChangedValue<Popup> popup;
        bool quit = false;

        OutputData( GraphicInterface::Window &desc );
    };

    using HandleMsg = std::function<void( const InputData &, OutputData & )>;

    GenericWindow( GraphicInterface::Window &desc, HandleMsg handleMsg );
    ~GenericWindow();

    void run();

    void close();
    void maximize();
    void minimize();
private:
    void inputReset();
    void inputRelease();

    class Implementation;
    Implementation *implementation;

    HandleMsg handleMsg;
    InputData inputData;
    OutputData outputData;
    GraphicInterface::Window& desc;
};
