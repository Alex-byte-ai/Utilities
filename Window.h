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
struct ConstCanvas
{
    const uint32_t *pixels;
    int width, height, stride;

    ConstCanvas();
    ConstCanvas( const uint32_t *pixels, int width, int height, int stride );

    const uint32_t *pixel( int x, int y ) const;
};

struct Canvas
{
    uint32_t *pixels;
    int width, height, stride;

    Canvas();
    Canvas( uint32_t *pixels, int width, int height, int stride );

    uint32_t *pixel( int x, int y );
    const uint32_t *pixel( int x, int y ) const;

    void stain();
    void fill( uint32_t color );
    void diamond( uint32_t inner, uint32_t outer );
    void draw( const ConstCanvas& other, int skipX, int skipY );
    void drawBlend( const ConstCanvas& other, int skipX, int skipY );

    void drawLineR( int x, int y, int size, uint32_t color );
    void drawLineD( int x, int y, int size, uint32_t color );
    void drawLineRD( int x, int y, int size, uint32_t color );
    void drawLineRU( int x, int y, int size, uint32_t color );

    Canvas trim( int& x0, int& y0, int w, int h, bool change = false );
};

struct Object
{
    Object();
    Object( const Object& other );
    virtual ~Object();

    bool visible;
    int x, y;

    void inner( int& x, int& y ) const;
    void outer( int& x, int& y ) const;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool contains( int x, int y ) const = 0;

    virtual void draw( Canvas& canvas, int x, int y ) const = 0;
};

struct Group : virtual public Object
{
    Group();
    Group( const Group& other );
    virtual ~Group();

    virtual int width() const override;
    virtual int height() const override;
    virtual bool contains( int x, int y ) const override;

    virtual void draw( Canvas& canvas, int x, int y ) const override;

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

    // These functions return true, if event should not be processed by objects after this one
    virtual bool hover( int x, int y ) = 0;
    virtual bool click( bool release, int x, int y ) = 0;
    virtual bool input( wchar_t c ) = 0;

    virtual void update() = 0;
};

struct ActiveGroup : public Group, public Active
{
    ActiveGroup();
    ActiveGroup( const ActiveGroup& other );
    virtual ~ActiveGroup();

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    using Active::update;

    virtual void update() override;

    void add( Object *object );
    void remove( Object *object );

private:
    std::vector<Active*> interactive;
};

struct Box : virtual public Object
{
    Box();
    Box( const Box& other );
    virtual ~Box();

    int w, h;

    virtual int width() const override;
    virtual int height() const override;
    virtual bool contains( int x, int y ) const override;

    Box& place( const Box& other );
};

struct Trigger : public Box
{
    Trigger();
    Trigger( const Trigger& other );
    virtual ~Trigger();

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct Rectangle : public Box
{
    Rectangle();
    Rectangle( const Rectangle& other );
    virtual ~Rectangle();

    uint32_t color;

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct ImageBase : public Box
{
    ImageBase();
    ImageBase( const ImageBase& other );
    virtual ~ImageBase();

    std::vector<uint32_t> pixels;

    void prepare( const void *input, int stride, int height );
};

struct Image : public ImageBase
{
    Image();
    Image( const Image& other );
    virtual ~Image();

    void prepare();
    void prepare( const void *data, int stride, int height );

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct ImageBlend : public ImageBase
{
    ImageBlend();
    ImageBlend( const ImageBlend& other );
    virtual ~ImageBlend();

    void prepare();
    void prepare( const void *data, int stride, int height );

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct StaticText : public ImageBlend
{
    StaticText();
    StaticText( const StaticText& other );
    virtual ~StaticText();

    int size;
    uint32_t color;
    std::wstring value;

    void prepare();
    void prepare( const void *data, int stride, int height );
};

struct DynamicText : public StaticText, public Active
{
    DynamicText();
    DynamicText( const DynamicText& other );
    virtual ~DynamicText();

    bool valid = true;

    std::function<bool( std::wstring )> setCallback;

    void prepare( bool write = true );

    virtual void draw( Canvas& canvas, int x, int y ) const override;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void update() override;
private:
    bool focused = false;
    static DynamicText *focus;
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

    virtual void draw( Canvas& canvas, int x, int y ) const override;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void update() override;
};

struct Button : public Box, public Active
{
    Button();
    Button( const Button& other );
    virtual ~Button();

    bool wasHovered, off;
    std::function<bool( bool )> onHover, onClick;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void update() override;
};

struct ActiveTrigger : public Button
{
    ActiveTrigger();
    ActiveTrigger( const ActiveTrigger& other );
    virtual ~ActiveTrigger();

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct TextButton : public Button
{
    TextButton();
    TextButton( const TextButton& other );
    virtual ~TextButton();

    bool centerX, centerY;
    std::wstring desc;

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct MinimizeButton : public Button
{
    MinimizeButton();
    MinimizeButton( const MinimizeButton& other );
    virtual ~MinimizeButton();

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct MaximizeButton : public Button
{
    MaximizeButton();
    MaximizeButton( const MaximizeButton& other );
    virtual ~MaximizeButton();

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct CloseButton : virtual public Button
{
    CloseButton();
    CloseButton( const CloseButton& other );
    virtual ~CloseButton();

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct PlusButton : virtual public Button
{
    PlusButton();
    PlusButton( const PlusButton& other );
    virtual ~PlusButton();

    std::wstring desc;
    bool toggle;

    void setDefaultCallback();

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct DropArea : virtual public Button
{
    DropArea();
    DropArea( const DropArea& other );
    virtual ~DropArea();

    virtual void draw( Canvas& canvas, int x, int y ) const override;
};

struct Scroller : public Box, public Active
{
    struct State
    {
        int tw = 0, th = 0, aw = 0, ah = 0, cw = 0, ch = 0, sw = 0, sh = 0, sizeh = 0, sizev = 0, posh = 0, posv = 0, xside = 0, yside = 0;
        bool hscroll = false, vscroll = false, ok = false;
    };

    Scroller();
    Scroller( const Scroller& other );

    std::optional<int> holdx, holdy;
    float horizontal, vertical;
    Object *content;
    bool corner;
    int size;
    State s;

    void scroll( int& x, int& y ) const;

    virtual bool contains( int x, int y ) const override;

    virtual bool hover( int x, int y ) override;
    virtual bool click( bool release, int x, int y ) override;
    virtual bool input( wchar_t c ) override;

    virtual void update() override;

    virtual void draw( Canvas& canvas, int x, int y ) const override;
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

    virtual void update() override;
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
    ChangedValue<int> mouseX{-1}, mouseY{-1};
    bool init = false, scale = false;
    int width = 0, height = 0;
    wchar_t typed = L'\0';
    Keys keys;
};

class OutputData;

using HandleMsg = std::function<bool( const InputData &, OutputData & )>;

struct Window : virtual public ActiveGroup
{
    Window( int h = 24, int sz = 16, int bh = 24, int tgw = 8, int b = 1 );
    Window( const Window &other );
    virtual ~Window();

    int titlebarHeight, buttonSize, buttonSpacingH, buttonSpacingV, triggerWidth, borderWidth;

    Trigger self, topTrigger, bottomTrigger, leftTrigger, rightTrigger, mouseTrigger;
    Rectangle titleBar, leftBorder, rightBorder, topBorder, bottomBorder, client;
    Image icon, content;
    Scroller scroller;
    StaticText title;

    MinimizeButton minimizeButton;
    MaximizeButton maximizeButton;
    CloseButton closeButton;

    HandleMsg handleMsg;
    std::function<void()> onClose;

    virtual int minWidth() const;
    virtual int minHeight() const;
    virtual void update();

    bool run( bool lock = true );
};

class OutputData
{
public:
    ChangedValue<GraphicInterface::Image &> image;
    ChangedValue<int> x{ 0 }, y{ 0 };
    bool quit = false;

    OutputData( GraphicInterface::Window &desc );
};

extern uint32_t customColors[16];

bool getColor( wchar_t code, uint32_t& color );
wchar_t getCode( uint32_t color );
std::wstring toPlainText( const std::wstring& colorfulText );

uint32_t makeColor( uint8_t r, uint8_t g, uint8_t b, uint8_t a );

uint8_t getR( uint32_t color );
uint8_t getG( uint32_t color );
uint8_t getB( uint32_t color );
uint8_t getA( uint32_t color );

bool noWindows();
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

    virtual int minWidth() const override;
    virtual int minHeight() const override;
    virtual void update() override;

    bool run( bool lock = true );
};

struct FileManager : public GraphicInterface::Window
{
    FileManager( std::filesystem::path initial, bool write );
    FileManager( const FileManager& other ) = delete;
    virtual ~FileManager();

    GraphicInterface::DynamicText file;
    GraphicInterface::TextButton confirm, reject;

    std::optional<Popup> popup;

    std::vector<std::shared_ptr<GraphicInterface::TextButton>> paths;

    std::optional<std::filesystem::path> root, choice;

    void select();

    virtual void update() override;
};

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

void savePath( std::function<void( const std::optional<std::filesystem::path>& )> callback, std::filesystem::path path = L"" );
void openPath( std::function<void( const std::optional<std::filesystem::path>& )> callback, std::filesystem::path path = L"" );
