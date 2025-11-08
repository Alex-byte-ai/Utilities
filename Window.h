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
struct Object
{
    virtual ~Object()
    {}

    virtual bool inside( int x, int y ) const = 0;
    virtual void draw( uint32_t *pixels, int width, int height ) const = 0;
};

struct Box : virtual public Object
{
    int x = 0, y = 0, w = 0, h = 0;

    virtual ~Box()
    {}

    virtual bool inside( int x0, int y0 ) const override;

    Box& place( const Box& other );
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

struct Image : public Box
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

struct Active : virtual public Object
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

struct Combobox : public Box, public Active
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

struct Button : public Box, public Active
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
    bool centerX = true, centerY = true;
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

struct CloseButton : virtual public Button
{
    virtual void draw( uint32_t *pixels, int width, int height ) const override;
};

struct Window : virtual public Box
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

    virtual void draw( uint32_t *pixels, int width, int height ) const override;

    void add( Object *object );
    void remove( Object *object );

    void run();

    std::vector<Object*> objects;
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

struct FileManager : public GraphicInterface::Window
{
    FileManager( bool write );

    GraphicInterface::DynamicText file;
    GraphicInterface::TextButton confirm, reject;

    std::vector<std::shared_ptr<GraphicInterface::TextButton>> paths;

    std::optional<std::filesystem::path> root, choice;

    void select();

    virtual void update() override;
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
