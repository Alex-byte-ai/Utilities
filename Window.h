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
    Object();
    Object( const Object& other );
    virtual ~Object();

    bool visible;
    int x, y;

    void inner( int& x, int& y ) const;
    void outer( int& x, int& y ) const;

    virtual bool contains( int x, int y ) const = 0;
    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const = 0;
};

struct Group : virtual public Object
{
    Group();
    Group( const Group& other );
    virtual ~Group();

    virtual bool contains( int x, int y ) const override;
    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;

    void add( Object *object );
    void remove( Object *object );

private:
    std::vector<Object*> objects;
};

struct Active : virtual public Object
{
    Active();
    Active( const Active& other );
    virtual ~Active();

    bool hovered;

    // These functions return true, if an object needs focus after that action
    virtual bool hover( int x, int y ) = 0;
    virtual bool click( bool release, int x, int y ) = 0;
    virtual bool input( wchar_t c ) = 0;

    virtual void focus( bool f ) = 0;
};

struct ActiveGroup : public Group, public Active
{
    ActiveGroup();
    ActiveGroup( const ActiveGroup& other );
    virtual ~ActiveGroup();

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void focus( bool f ) override;

    void add( Object *object );
    void remove( Object *object );

    bool activeContains( int x, int y ) const;

private:
    std::vector<Active*> interactive;
    Active* target = nullptr;
};

struct Box : virtual public Object
{
    Box();
    Box( const Box& other );
    virtual ~Box();

    int w, h;

    virtual bool contains( int x, int y ) const override;

    Box& place( const Box& other );
};

struct Trigger : public Box
{
    Trigger();
    Trigger( const Trigger& other );
    virtual ~Trigger();

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct Rectangle : public Box
{
    Rectangle();
    Rectangle( const Rectangle& other );
    virtual ~Rectangle();

    uint32_t color;

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct ImageBase : public Box
{
    ImageBase();
    ImageBase( const ImageBase& other );
    virtual ~ImageBase();

    std::vector<uint32_t> pixels;
    int bufferW, bufferH;

    void prepare( int stride, int height );
};

struct Image : public ImageBase
{
    Image();
    Image( const Image& other );
    virtual ~Image();

    void prepare( const void *data, int stride, int height );

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct ImageBlend : public ImageBase
{
    ImageBlend();
    ImageBlend( const ImageBlend& other );
    virtual ~ImageBlend();

    void prepare( const void *data, int stride, int height );

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct StaticText : public ImageBlend
{
    StaticText();
    StaticText( const StaticText& other );
    virtual ~StaticText();

    uint32_t color;
    std::wstring value;

    void prepare();
};

struct DynamicText : public StaticText, public Active
{
    DynamicText();
    DynamicText( const DynamicText& other );
    virtual ~DynamicText();

    bool valid = true, focused = false;

    std::function<bool( std::wstring )> setCallback;

    void prepare( bool write = true );

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void focus( bool f ) override;
};

struct Combobox : public Box, public Active
{
    Combobox();
    Combobox( const Combobox& other );
    virtual ~Combobox();

    std::function<bool( std::wstring )> setCallback;
    std::vector<std::wstring> options;
    size_t option;
    bool isOpen;

    void open( bool f );
    size_t select( int x, int y );

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void focus( bool f ) override;
};

struct Button : public Box, public Active
{
    Button();
    Button( const Button& other );
    virtual ~Button();

    bool wasHovered, activateByHovering, off;
    std::function<void( bool )> use;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void focus( bool f ) override;
};

struct ActiveTrigger : public Button
{
    ActiveTrigger();
    ActiveTrigger( const ActiveTrigger& other );
    virtual ~ActiveTrigger();

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct TextButton : public Button
{
    TextButton();
    TextButton( const TextButton& other );
    virtual ~TextButton();

    bool centerX, centerY;
    std::wstring desc;

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct MinimizeButton : public Button
{
    MinimizeButton();
    MinimizeButton( const MinimizeButton& other );
    virtual ~MinimizeButton();

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct MaximizeButton : public Button
{
    MaximizeButton();
    MaximizeButton( const MaximizeButton& other );
    virtual ~MaximizeButton();

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct CloseButton : virtual public Button
{
    CloseButton();
    CloseButton( const CloseButton& other );
    virtual ~CloseButton();

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct PlusButton : virtual public Button
{
    PlusButton();
    PlusButton( const PlusButton& other );
    virtual ~PlusButton();

    std::wstring desc;
    bool toggle;

    void setDefaultCallback();

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct DropArea : virtual public Button
{
    DropArea();
    DropArea( const DropArea& other );
    virtual ~DropArea();

    virtual void draw( uint32_t *pixels, int width, int height, int x, int y ) const override;
};

struct Node : virtual public ActiveGroup
{
    struct Parameter
    {
        std::vector<Parameter> parameters;
        std::wstring name;
        bool open;

        Parameter() : open( false )
        {}

        Parameter( std::wstring n, std::vector<Parameter> p = {}, bool o = false ) : parameters( std::move( p ) ), name( std::move( n ) ), open( o )
        {}

        Parameter( const Parameter& other ) : parameters( other.parameters ), name( other.name ), open( other.open )
        {}

        Parameter( Parameter&& other ) : parameters( std::move( other.parameters ) ), name( std::move( other.name ) ), open( other.open )
        {}
    };

    enum class Action
    {
        None,
        Move,
        Open,
        Close
    };

    struct ActionData
    {
        std::optional<std::vector<size_t>> path, secondary;
        Action action = Action::None;
    };

    Node( ActionData &data, Node *root, bool open );
    Node( ActionData &data, const Parameter& parameter, Node *root = nullptr );
    Node( const Node& other ) = delete;
    virtual ~Node();

    void update( const Parameter& parameter );

    ActionData &data;

    std::vector<std::shared_ptr<Node>> nodes;
    ActiveGroup wrapper;
    PlusButton button;
    DropArea space;
    Node *root;
    size_t id;

    void open( bool f );

    void reposition();
    void repositionRecursive();
    void recount();
    int height() const;

    std::vector<size_t> getPath() const;
    Node *getObject( const std::vector<size_t>& path );

    Node *addNode( std::shared_ptr<Node> node );
    Node *addNode( std::shared_ptr<Node> node, size_t id );

    std::shared_ptr<Node> detach();
};

struct ScrollerContent
{};

struct Scroller
{};

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

class OutputData;

using HandleMsg = std::function<void( const InputData &, OutputData & )>;

struct Window : virtual public ActiveGroup
{
    Window( int h = 24, int sz = 16, int bh = 24, int tgw = 8, int b = 1 );
    Window( const Window &other );
    virtual ~Window();

    int titlebarHeight, buttonSize, buttonSpacingH, buttonSpacingV, triggerWidth, borderWidth;

    Trigger self, topTrigger, bottomTrigger, leftTrigger, rightTrigger, mouseTrigger;

    Rectangle titleBar, leftBorder, rightBorder, topBorder, bottomBorder, client;
    Image icon, content;

    StaticText title;

    MinimizeButton minimizeButton;
    MaximizeButton maximizeButton;
    CloseButton closeButton;

    HandleMsg handleMsg;

    virtual int minWidth() const;
    virtual int minHeight() const;
    virtual void update();

    void run();
};

class OutputData
{
public:
    ChangedValue<GraphicInterface::Image &> image;
    ChangedValue<int> x{ 0 }, y{ 0 };
    bool quit = false;

    OutputData( GraphicInterface::Window &desc );
};

uint32_t makeColor( uint8_t r, uint8_t g, uint8_t b, uint8_t a );
uint8_t getR( uint32_t color );
uint8_t getG( uint32_t color );
uint8_t getB( uint32_t color );
uint8_t getA( uint32_t color );
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

        Parameter( Parameter&& other )
            : options( std::move( other.options ) ), name( std::move( other.name ) ), set( std::move( other.set ) ), get( std::move( other.get ) )
        {}

        Parameter( const Parameter& other )
            : options( other.options ), name( other.name ), set( other.set ), get( other.get )
        {}
    };

    using Parameters = std::vector<Parameter>;

    Settings( std::wstring title, const Parameters& parameters );
    Settings( const Settings& other ) = delete;
    virtual ~Settings();

    std::vector<std::shared_ptr<GraphicInterface::Box>> fields;

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

    Popup( Type type = Type::Info, std::wstring title = L"", std::wstring information = L"" );
    Popup( const Popup& other );
    virtual ~Popup();

    Type type;

    GraphicInterface::TextButton yesButton, noButton, cancelButton;
    GraphicInterface::StaticText info;

    std::vector<GraphicInterface::TextButton*> buttons;

    // User response
    std::optional<bool> answer;

    virtual void update() override;
};

struct ContextMenu : public GraphicInterface::Window
{
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

        Parameter( const Parameter& other )
            : parameters( other.parameters ), callback( other.callback ), name( other.name ), active( other.active )
        {}
    };

    using Parameters = std::vector<Parameter>;

    ContextMenu( const Parameters& parameters );
    ContextMenu( const ContextMenu& other ) = delete;
    virtual ~ContextMenu();

    std::vector<std::shared_ptr<GraphicInterface::Object>> storage;

    virtual bool hover( int x, int y ) override;

    virtual void update() override;

    void run();
};

struct FileManager : public GraphicInterface::Window
{
    FileManager( bool write );
    FileManager( const FileManager& other ) = delete;
    virtual ~FileManager();

    GraphicInterface::DynamicText file;
    GraphicInterface::TextButton confirm, reject;

    std::vector<std::shared_ptr<GraphicInterface::TextButton>> paths;

    std::optional<std::filesystem::path> root, choice;

    void select();

    virtual void update() override;
};

std::optional<std::filesystem::path> savePath();
std::optional<std::filesystem::path> openPath();

struct Hierarchy : public GraphicInterface::Window
{
    Hierarchy( const GraphicInterface::Node::Parameter& parameter );
    Hierarchy( const Hierarchy& other ) = delete;
    virtual ~Hierarchy();

    std::function<bool( const GraphicInterface::Node::ActionData& )> callback;
    GraphicInterface::Node::ActionData data;
    GraphicInterface::Node root;

    virtual bool click( bool release, int x, int y ) override;

    virtual void update() override;
};

class GenericWindow
{
public:
    GenericWindow( GraphicInterface::Window &desc );
    ~GenericWindow();

    void run( bool lock = true );

    void close();
    void maximize();
    void minimize();
private:
    void inputReset();
    void inputRelease();

    class Implementation;
    Implementation *implementation;

    GraphicInterface::InputData inputData;
    GraphicInterface::OutputData outputData;
    GraphicInterface::Window& desc;

    struct Data
    {
        GraphicInterface::Window* window;
        bool lock;
    };

    static std::vector<Data> stack;

};
