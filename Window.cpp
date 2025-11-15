#include "Window.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <memory>

#include "UnicodeString.h"
#include "Exception.h"
#include "Lambda.h"
#include "Basic.h"

#define WM_APP_SHOWMENU (WM_APP + 1)

namespace GraphicInterface
{
static void drawLineR( uint32_t *pixels, int width, int, int x, int y, int size, uint32_t color )
{
    while( size > 0 )
    {
        pixels[y * width + x] = color;
        ++x;
        --size;
    }
}

static void drawLineD( uint32_t *pixels, int width, int, int x, int y, int size, uint32_t color )
{
    while( size > 0 )
    {
        pixels[y * width + x] = color;
        ++y;
        --size;
    }
}

static void drawLineRD( uint32_t *pixels, int width, int, int x, int y, int size, uint32_t color )
{
    while( size > 0 )
    {
        pixels[y * width + x] = color;
        ++x;
        ++y;
        --size;
    }
}

static void drawLineRU( uint32_t *pixels, int width, int, int x, int y, int size, uint32_t color )
{
    while( size > 0 )
    {
        pixels[y * width + x] = color;
        ++x;
        --y;
        --size;
    }
}

static bool renderTextToBuffer(
    const std::wstring& text, const std::wstring& fontName, uint32_t color, uint32_t background, int padding,
    int& outWidth, int& outHeight, std::vector<uint32_t>& outBuffer )
{
    std::vector<std::wstring> lines;
    {
        size_t pos = 0, start = 0;
        while( pos != std::wstring::npos )
        {
            pos = text.find( L'\n', start );
            if( pos == std::wstring::npos )
            {
                lines.push_back( text.substr( start ) );
                break;
            }
            lines.push_back( text.substr( start, pos - start ) );
            start = pos + 1;
        }

        if( lines.empty() )
            lines.push_back( L"" );
    }

    HDC hdc = CreateCompatibleDC( NULL );
    if( !hdc )
        return false;

    LOGFONTW font;
    clear( &font, sizeof( font ) );

    font.lfHeight = 16;
    font.lfWeight = 400;
    font.lfOutPrecision = OUT_RASTER_PRECIS;
    font.lfQuality = ANTIALIASED_QUALITY;
    font.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
    copy( font.lfFaceName, fontName.c_str(), sizeof( wchar_t ) * Min( size_t( LF_FACESIZE ), fontName.length() ) );

    HFONT hFont = CreateFontIndirectW( &font );

    if( !hFont )
    {
        DeleteDC( hdc );
        return false;
    }

    HGDIOBJ oldFont = SelectObject( hdc, hFont );

    TEXTMETRICW tm;
    if( !GetTextMetricsW( hdc, &tm ) )
    {
        SelectObject( hdc, oldFont );
        DeleteObject( hFont );
        DeleteDC( hdc );
        return false;
    }

    int maxWidth = 0;
    SIZE sz;
    for( auto &ln : lines )
    {
        if( !GetTextExtentPoint32W( hdc, ln.c_str(), ln.size(), &sz ) )
            sz.cx = 0;
        if( sz.cx > maxWidth )
            maxWidth = sz.cx;
    }

    int lineHeight = tm.tmHeight;
    int totalHeight = lineHeight * lines.size();
    int finalWidth = maxWidth + padding * 2;
    int finalHeight = totalHeight + padding * 2;

    if( finalWidth <= 0 || finalHeight <= 0 )
    {
        SelectObject( hdc, oldFont );
        DeleteObject( hFont );
        DeleteDC( hdc );
        return false;
    }

    BITMAPINFO bmi;
    ZeroMemory( &bmi, sizeof( bmi ) );
    bmi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    bmi.bmiHeader.biWidth = finalWidth;
    bmi.bmiHeader.biHeight = -finalHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0 );
    if( !hBitmap || !bits )
    {
        SelectObject( hdc, oldFont );
        DeleteObject( hFont );
        DeleteDC( hdc );
        if( hBitmap )
            DeleteObject( hBitmap );
        return false;
    }

    HDC memDC = CreateCompatibleDC( NULL );
    if( !memDC )
    {
        DeleteObject( hBitmap );
        SelectObject( hdc, oldFont );
        DeleteObject( hFont );
        DeleteDC( hdc );
        return false;
    }

    HGDIOBJ oldBmp = SelectObject( memDC, hBitmap );
    HGDIOBJ oldMemFont = SelectObject( memDC, hFont );

    // White background so we can derive alpha later
    uint32_t *pix = ( uint32_t * )bits;
    size_t pixelCount = finalWidth * finalHeight;
    for( size_t i = 0; i < pixelCount; ++i )
        pix[i] = 0x00FFFFFFu;

    SetBkMode( memDC, TRANSPARENT );
    SetTextColor( memDC, RGB( 0, 0, 0 ) );

    int y = padding;
    for( auto &ln : lines )
    {
        TextOutW( memDC, padding, y, ln.c_str(), ln.size() );
        y += lineHeight;
    }

    outBuffer.resize( pixelCount );
    auto bytes = ( uint8_t* )bits;
    for( int yy = 0; yy < finalHeight; ++yy )
    {
        for( int xx = 0; xx < finalWidth; ++xx )
        {
            size_t idx = yy * finalWidth + xx;
            uint8_t inB = bytes[idx * 4 + 0];
            uint8_t inG = bytes[idx * 4 + 1];
            uint8_t inR = bytes[idx * 4 + 2];

            // Derive alpha from luminance (white background, black text)
            int lum = ( 77 * inR + 150 * inG + 29 * inB ) >> 8;
            uint32_t a = 255 - lum;

            uint32_t outR = ( ( ( color >> 16 ) & 0xFFu ) * a + ( ( background >> 16 ) & 0xFFu ) * ( 255 - a ) ) / 255;
            uint32_t outG = ( ( ( color >> 8 ) & 0xFFu ) * a + ( ( background >> 8 ) & 0xFFu ) * ( 255 - a ) ) / 255;
            uint32_t outB = ( ( ( color >> 0 ) & 0xFFu ) * a + ( ( background >> 0 ) & 0xFFu ) * ( 255 - a ) ) / 255;

            a = 255;
            outBuffer[idx] = makeColor( outR, outG, outB, a );
        }
    }
    // copy( outBuffer.data(), bits, pixelCount * 4 );

    outWidth = finalWidth;
    outHeight = finalHeight;

    SelectObject( memDC, oldMemFont );
    SelectObject( memDC, oldBmp );
    SelectObject( hdc, oldFont );
    DeleteObject( hFont );
    DeleteObject( hBitmap );
    DeleteDC( memDC );
    DeleteDC( hdc );

    return true;
}

Object::Object()
{}

Object::Object( const Object& )
{}

Object::~Object()
{}

Group::Group() : Object()
{}

Group::Group( const Group& other ) : Object( other )
{}

Group::~Group()
{}

bool Group::contains( int x, int y ) const
{
    for( auto object : objects )
    {
        if( object->contains( x, y ) )
            return true;
    }
    return false;
}

void Group::draw( uint32_t *pixels, int width, int height ) const
{
    for( auto object : objects )
        object->draw( pixels, width, height );
}

void Group::add( Object *object )
{
    objects.push_back( object );
}

void Group::remove( Object *object )
{
    auto i = std::find( objects.begin(), objects.end(), object );
    if( i != objects.end() )
        objects.erase( i );
}

Active::Active() : hovered( false )
{}

Active::Active( const Active& other ) : Object(), hovered( other.hovered )
{}

Active::~Active()
{}

ActiveGroup::ActiveGroup() : Group(), Active(), target( nullptr )
{}

ActiveGroup::ActiveGroup( const ActiveGroup& other ) : Object(), Group( other ), Active( other ), target( nullptr )
{}

ActiveGroup::~ActiveGroup()
{}

static bool focusIt( Active *& target, Active * active, bool refocus )
{
    if( !refocus )
        return false;

    if( target )
        target->focus( false );

    target = active;
    target->focus( true );
    return true;
}

bool ActiveGroup::hover( int x, int y )
{
    bool absorbed = false;
    for( auto i = interactive.rbegin(); i != interactive.rend(); ++i )
    {
        auto active = *i;
        bool contains = !absorbed && active->contains( x, y );
        active->hovered = contains;
        if( contains )
            absorbed = false;
    }

    bool needFocus = false;
    for( auto active : interactive )
    {
        if( focusIt( target, active, active->hover( x, y ) ) )
            needFocus = true;
    }
    return needFocus;
}

bool ActiveGroup::click( bool release, int x, int y )
{
    for( auto i = interactive.rbegin(); i != interactive.rend(); ++i )
    {
        auto& active = * i;
        if( active->contains( x, y ) )
        {
            if( focusIt( target, active, active->click( release, x, y ) ) )
                return true;
        }
    }
    return false;
}

bool ActiveGroup::input( wchar_t c )
{
    return target && target->input( c );
}

void ActiveGroup::focus( bool )
{}

void ActiveGroup::add( Object *object )
{
    Group::add( object );
    if( auto active = dynamic_cast<Active*>( object ) )
        interactive.push_back( active );
}

void ActiveGroup::remove( Object *object )
{
    Group::remove( object );
    auto i = std::find( interactive.begin(), interactive.end(), object );
    if( i != interactive.end() )
        interactive.erase( i );
}

Box::Box() : Object(), x( 0 ), y( 0 ), w( 0 ), h( 0 )
{}

Box::Box( const Box& other ) : Object( other )
{}

Box::~Box()
{}

bool Box::contains( int x0, int y0 ) const
{
    return x <= x0 && x0 < x + w && y <= y0 && y0 < y + h;
}

Box& Box::place( const Box& other )
{
    x = other.x;
    y = other.y;
    w = other.w;
    h = other.h;
    return *this;
}

void Box::fill( uint32_t *pixels, int width, int height, uint32_t c ) const
{
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if( x0 < 0 )
        x0 = 0;
    if( y0 < 0 )
        y0 = 0;

    if( x1 > width )
        x1 = width;
    if( y1 > height )
        y1 = height;

    for( int j = y0; j < y1; ++j )
    {
        for( int i = x0; i < x1; ++i )
        {
            pixels[j * width + i] = c;
        }
    }
}

void Box::gradient( uint32_t *pixels, int width, int height ) const
{
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if( x0 < 0 )
        x0 = 0;
    if( y0 < 0 )
        y0 = 0;

    if( x1 > width )
        x1 = width;
    if( y1 > height )
        y1 = height;

    int gw = x1 - x0;

    for( int j = y0; j < y1; ++j )
    {
        for( int i = x0; i < x1; ++i )
        {
            int p = i - x0;
            uint8_t alpha = p * 255 / gw;
            uint8_t red   = ( ( gw - p ) * 255 ) / gw;
            uint8_t green = 0;
            uint8_t blue  = p * 255 / gw;

            pixels[j * width + i] = makeColor( red, green, blue, alpha );
        }
    }
}

Trigger::Trigger() : Box()
{}

Trigger::Trigger( const Trigger& other ) : Object( other ), Box( other )
{}

Trigger::~Trigger()
{}

void Trigger::draw( uint32_t *, int, int ) const
{}

Rectangle::Rectangle() : color( 0 )
{}

Rectangle::Rectangle( const Rectangle& other ) : Object( other ), Box( other ), color( other.color )
{}

Rectangle::~Rectangle()
{}

void Rectangle::draw( uint32_t *pixels, int width, int height ) const
{
    fill( pixels, width, height, color );
}

Image::Image() : Box(), bufferW( 0 ), bufferH( 0 )
{}

Image::Image( const Image& other ) : Object( other ), Box( other ), pixels( other.pixels ), bufferW( other.bufferW ), bufferH( other.bufferH )
{}

Image::~Image()
{}

void Image::prepare( int stride, int height )
{
    bufferW = w = Abs( stride );
    bufferH = h = height;

    pixels.resize( w * h );
}

void Image::prepare( const void *data, int stride, int height )
{
    prepare( stride, height );

    auto output = ( RGBQUAD * )pixels.data();
    auto input = ( const RGBQUAD * )data;

    while( height > 0 )
    {
        for( int j = 0; j < w; ++j )
        {
            auto &o = *( output + j );
            auto &i = *( input + j );
            o.rgbBlue = i.rgbBlue * i.rgbReserved / 255;
            o.rgbGreen = i.rgbGreen * i.rgbReserved / 255;
            o.rgbRed = i.rgbRed * i.rgbReserved / 255;

            // It seems, fully transparent parts of window can't be interacted with, and there is no way to disable that
            o.rgbReserved = i.rgbReserved <= 0 ? 1 : i.rgbReserved;
        }
        output += bufferW;
        input += stride;
        --height;
    }
}

void Image::draw( uint32_t *otherPixels, int width, int height ) const
{
    int x0 = x;
    int y0 = y;
    int x1 = x + Min( w, bufferW );
    int y1 = y + Min( h, bufferH );

    if( x0 < 0 )
        x0 = 0;
    if( y0 < 0 )
        y0 = 0;

    if( x1 > width )
        x1 = width;
    if( y1 > height )
        y1 = height;

    int bx0 = x0 - x;

    int lineW = x1 - x0;
    if( lineW <= 0 )
        return;

    auto line = lineW * sizeof( uint32_t );
    auto pointer = pixels.data() + bx0;
    for( int j = y0; j < y1; ++j )
    {
        copy( &otherPixels[j * width + x0], pointer, line );
        pointer += bufferW;
    }
}

StaticText::StaticText() : Image()
{
    color = makeColor( 0, 0, 0, 255 );
}

StaticText::StaticText( const StaticText& other ) : Object( other ), Image( other ), color( other.color ), value( other.value )
{}

StaticText::~StaticText()
{}

void StaticText::prepare( uint32_t background )
{
    w = 0;
    h = 0;
    if( renderTextToBuffer( value, L"DejaVuSansMono", color, background, 0, bufferW, bufferH, pixels ) )
    {
        w = bufferW;
        h = bufferH;
    }
    else
    {
        pixels.push_back( background );
        w = bufferW = 1;
        h = bufferH = 1;
    }
}

DynamicText::DynamicText() : StaticText(), Active()
{}

DynamicText::DynamicText( const DynamicText& other ) : Object( other ), StaticText( other ), Active( other ), valid( other.valid ), focused( other.focused ), setCallback( other.setCallback )
{}

DynamicText::~DynamicText()
{}

void DynamicText::prepare( bool write )
{
    auto idleColor = makeColor( 255, 255, 255, 255 );
    auto errorColor = makeColor( 255, 127, 127, 255 );
    auto focusColor = makeColor( 255, 255, 127, 255 );

    if( write )
        valid = setCallback ? setCallback( value ) : false;

    auto background = valid ? ( focused ? focusColor : idleColor ) : errorColor;
    StaticText::prepare( background );
}

void DynamicText::draw( uint32_t *canvas, int width, int height ) const
{
    if( !pixels.empty() )
        fill( canvas, width, height, pixels[0] );
    StaticText::draw( canvas, width, height );
}

bool DynamicText::hover( int, int )
{
    return false;
}

bool DynamicText::click( bool release, int, int )
{
    return release;
}

bool DynamicText::input( wchar_t c )
{
    if( c == L'\b' )
    {
        if( !value.empty() )
            value.pop_back();
    }
    else
    {
        value += c;
    }
    prepare();
    return false;
}

void DynamicText::focus( bool f )
{
    if( focused != f )
    {
        focused = f;
        prepare( false );
    }
}

Combobox::Combobox() : Box(), Active(), option( 0 ), isOpen( false )
{}

Combobox::Combobox( const Combobox& other ) : Object( other ), Box( other ), Active( other ), setCallback( other.setCallback ), options( other.options ), option( other.option ), isOpen( false )
{}

Combobox::~Combobox()
{}

void Combobox::open( bool f )
{
    if( !f && isOpen && setCallback )
        setCallback( options[option] );

    isOpen = f;
    h = isOpen ? 16 * options.size() : 16;
}

size_t Combobox::select( int x0, int y0 )
{
    if( !contains( x0, y0 ) )
        return option;

    return ( y0 - y ) / 16;
}

void Combobox::draw( uint32_t *pixels, int width, int height ) const
{
    auto base = makeColor( 255, 255, 255, 255 );
    auto selection = makeColor( 255, 255, 127, 255 );
    StaticText text;
    Rectangle area;

    area.w = w;
    area.h = 16;

    auto drawItem = [&]( std::wstring o, size_t i, uint32_t c )
    {
        area.color = c;
        area.x = x;
        area.y = y + i * 16;
        area.draw( pixels, width, height );
        text.value = std::move( o );
        text.prepare( c );
        text.x = area.x + ( w - text.w ) * 0.5;
        text.y = area.y + ( 16 - text.h ) * 0.5;
        text.draw( pixels, width, height );
    };

    if( isOpen )
    {
        size_t i = 0;
        for( auto& o : options )
        {
            drawItem( o, i, i == option ? selection : base );
            ++i;
        }
    }
    else
    {
        drawItem( options[option], 0, base );
    }
}

bool Combobox::hover( int x0, int y0 )
{
    if( !contains( x0, y0 ) )
        open( false );
    if( isOpen )
        option = select( x0, y0 );
    return false;
}

bool Combobox::click( bool release, int, int )
{
    if( release )
        open( !isOpen );
    return false;
}

bool Combobox::input( wchar_t )
{
    return false;
}

void Combobox::focus( bool )
{}

Button::Button() : Box(), Active(), wasHovered( false ), activateByHovering( false ), off( false )
{}

Button::Button( const Button& other ) : Object( other ), Box( other ), Active( other ), wasHovered( false ), activateByHovering( other.activateByHovering ), off( other.off ), use( other.use )
{}

Button::~Button()
{}

bool Button::hover( int, int )
{
    if( activateByHovering && !off && use && wasHovered != hovered )
        use( hovered );

    wasHovered = hovered;
    return false;
}

bool Button::click( bool release, int, int )
{
    if( !activateByHovering && !off && use )
        use( release );
    return false;
}

bool Button::input( wchar_t )
{
    return false;
}

void Button::focus( bool )
{}

ActiveTrigger::ActiveTrigger() : Button()
{}

ActiveTrigger::ActiveTrigger( const ActiveTrigger& other ) : Object( other ), Button( other )
{}

ActiveTrigger::~ActiveTrigger()
{}

void ActiveTrigger::draw( uint32_t *, int, int ) const
{}

TextButton::TextButton() : Button(), centerX( true ), centerY( true )
{}

TextButton::TextButton( const TextButton& other ) : Object( other ), Button( other ), centerX( other.centerX ), centerY( other.centerX ), desc( other.desc )
{}

TextButton::~TextButton()
{}

void TextButton::draw( uint32_t *pixels, int width, int height ) const
{
    if( w <= 0 || h <= 0 )
        return;

    auto color = hovered && !off ? makeColor( 220, 220, 60, 255 ) : makeColor( 200, 200, 200, 255 );
    fill( pixels, width, height, color );

    StaticText text;
    text.color = off ? makeColor( 128, 128, 128, 255 ) : makeColor( 0, 0, 0, 255 );

    text.value = desc;
    text.prepare( color );

    text.x = x + ( centerX ? ( w - text.w ) * 0.5 : 16 );
    text.y = y + ( centerY ? ( h - text.h ) * 0.5 : 16 );

    text.draw( pixels, width, height );

    if( activateByHovering )
    {
        drawLineRD( pixels, width, height, x + w - 16, y + 4, 4, text.color );
        drawLineRU( pixels, width, height, x + w - 16, y + h - 5, 4, text.color );
    }
}

MinimizeButton::MinimizeButton() : Button()
{}

MinimizeButton::MinimizeButton( const MinimizeButton& other ) : Object( other ), Button( other )
{}

MinimizeButton::~MinimizeButton()
{}

void MinimizeButton::draw( uint32_t *pixels, int width, int height ) const
{
    auto color = hovered ? makeColor( 235, 235, 235, 255 ) : makeColor( 255, 255, 255, 255 );
    fill( pixels, width, height, color );

    auto black = makeColor( 0, 0, 0, 255 );

    drawLineR( pixels, width, height, x + 3, y + h / 2 - 1, w - 6, black );
}

MaximizeButton::MaximizeButton() : Button()
{}

MaximizeButton::MaximizeButton( const MaximizeButton& other ) : Object( other ), Button( other )
{}

MaximizeButton::~MaximizeButton()
{}

void MaximizeButton::draw( uint32_t *pixels, int width, int height ) const
{
    auto color = hovered ? makeColor( 235, 235, 235, 255 ) : makeColor( 255, 255, 255, 255 );
    fill( pixels, width, height, color );

    auto black = makeColor( 0, 0, 0, 255 );
    auto half = hovered ? makeColor( 188, 188, 188, 255 ) : makeColor( 204, 204, 204, 255 );

    drawLineR( pixels, width, height, x + 4, y + 4, w - 8, half );
    drawLineD( pixels, width, height, x + 4, y + 4, w - 8, half );
    drawLineR( pixels, width, height, x + 4, y + h - 5, w - 8, half );
    drawLineD( pixels, width, height, x + w - 5, y + 4, w - 8, half );

    drawLineR( pixels, width, height, x + 3, y + 3, w - 6, black );
    drawLineD( pixels, width, height, x + 3, y + 3, w - 6, black );
    drawLineR( pixels, width, height, x + 3, y + h - 4, w - 6, black );
    drawLineD( pixels, width, height, x + w - 4, y + 3, w - 6, black );
}

CloseButton::CloseButton() : Button()
{}

CloseButton::CloseButton( const CloseButton& other ) : Object( other ), Button( other )
{}

CloseButton::~CloseButton()
{}

void CloseButton::draw( uint32_t *pixels, int width, int height ) const
{
    auto color = hovered ? makeColor( 245, 10, 10, 255 ) : makeColor( 255, 255, 255, 255 );
    fill( pixels, width, height, color );

    auto black = makeColor( 0, 0, 0, 255 );
    auto half = hovered ? makeColor( 196, 8, 8, 255 ) : makeColor( 204, 204, 204, 255 );

    drawLineRD( pixels, width, height, x + 4, y + 3, w - 7, half );
    drawLineRU( pixels, width, height, x + 4, y + h - 4, w - 7, half );

    drawLineRD( pixels, width, height, x + 3, y + 4, w - 7, half );
    drawLineRU( pixels, width, height, x + 3, y + h - 5, w - 7, half );

    drawLineRD( pixels, width, height, x + 3, y + 3, w - 6, black );
    drawLineRU( pixels, width, height, x + 3, y + h - 4, w - 6, black );
}

Window::Window( int th, int sz, int bh, int tgw, int b )
    : titlebarHeight( th ), buttonSize( sz ), buttonSpacingH( bh ), triggerWidth( tgw ), borderWidth( b )
{
    buttonSpacingV = ( titlebarHeight - buttonSize ) / 2;

    titleBar.color = makeColor( 255, 255, 255, 255 );
    leftBorder.color = rightBorder.color = topBorder.color = bottomBorder.color = makeColor( 85, 85, 85, 255 );

    add( &self );
    add( &client );
    add( &content );
    add( &titleBar );
    add( &icon );
    add( &title );
    add( &closeButton );
    add( &maximizeButton );
    add( &minimizeButton );
    add( &leftBorder );
    add( &rightBorder );
    add( &topBorder );
    add( &bottomBorder );
    add( &topTrigger );
    add( &bottomTrigger );
    add( &leftTrigger );
    add( &rightTrigger );
}

Window::Window( const Window &other ) :
    Object( other ), ActiveGroup( other ),
    titlebarHeight( other.titlebarHeight ), buttonSize( other.buttonSize ), buttonSpacingH( other.buttonSpacingH ),
    triggerWidth( other.triggerWidth ), borderWidth( other.borderWidth ),
    self( other.self ), topTrigger( other.topTrigger ), bottomTrigger( other.bottomTrigger ), leftTrigger( other.leftTrigger ), rightTrigger( other.rightTrigger ),
    titleBar( other.titleBar ), leftBorder( other.leftBorder ), rightBorder( other.rightBorder ), topBorder( other.topBorder ), bottomBorder( other.bottomBorder ), client( other.client ),
    icon( other.icon ), content( other.content ), title( other.title ),
    minimizeButton( other.minimizeButton ), maximizeButton( other.maximizeButton ), closeButton( other.closeButton )
{
    minimizeButton.use = nullptr;
    maximizeButton.use = nullptr;
    closeButton.use = nullptr;

    add( &self );
    add( &client );
    add( &content );
    add( &titleBar );
    add( &icon );
    add( &title );
    add( &closeButton );
    add( &maximizeButton );
    add( &minimizeButton );
    add( &leftBorder );
    add( &rightBorder );
    add( &topBorder );
    add( &bottomBorder );
    add( &topTrigger );
    add( &bottomTrigger );
    add( &leftTrigger );
    add( &rightTrigger );
}

Window::~Window()
{}

int Window::minWidth() const
{
    auto titleBarMinWidth = 3 * buttonSize + buttonSpacingV + 3 * buttonSpacingH + borderWidth + icon.x + icon.w;
    auto minWidth = content.w + 2 * borderWidth;

    if( titleBarMinWidth > minWidth )
        return titleBarMinWidth;

    return minWidth;
}

int Window::minHeight() const
{
    return titlebarHeight + content.h + 2 * borderWidth;
}

void Window::update()
{
    titleBar.x = self.x;
    titleBar.y = self.y;
    titleBar.w = self.w;
    titleBar.h = titlebarHeight + borderWidth;

    rightTrigger.x = self.x + self.w - triggerWidth;
    rightTrigger.y = self.y;
    rightTrigger.w = triggerWidth;
    rightTrigger.h = self.h;

    bottomTrigger.x = self.x;
    bottomTrigger.y = self.y + self.h - triggerWidth;
    bottomTrigger.w = self.w;
    bottomTrigger.h = triggerWidth;

    leftBorder.x = self.x;
    leftBorder.y = self.y;
    leftBorder.w = borderWidth;
    leftBorder.h = self.h;

    rightBorder.x = self.x + self.w - borderWidth;
    rightBorder.y = self.y;
    rightBorder.w = borderWidth;
    rightBorder.h = self.h;

    topBorder.x = self.x;
    topBorder.y = self.y;
    topBorder.w = self.w;
    topBorder.h = borderWidth;

    bottomBorder.x = self.x;
    bottomBorder.y = self.y + self.h - borderWidth;
    bottomBorder.w = self.w;
    bottomBorder.h = borderWidth;

    closeButton.x = self.x + self.w - buttonSpacingV - buttonSize - borderWidth;
    closeButton.y = self.y + buttonSpacingV + borderWidth;
    closeButton.w = buttonSize;
    closeButton.h = buttonSize;

    maximizeButton.place( closeButton );
    maximizeButton.x -= buttonSpacingH + buttonSize;

    minimizeButton.place( maximizeButton );
    minimizeButton.x -= buttonSpacingH + buttonSize;

    icon.y = borderWidth + ( titlebarHeight - icon.h ) / 2;
    icon.x = icon.y;

    title.x = icon.x + icon.w + buttonSpacingV;
    title.y = closeButton.y;

    title.w = self.w - borderWidth - 2 * buttonSpacingV - 3 * buttonSize - 2 * buttonSpacingH - title.x;
    if( title.w < title.bufferW )
        title.w = 0;

    client.x = self.x + borderWidth;
    client.y = self.y + borderWidth + titlebarHeight;
    client.w = self.w - 2 * borderWidth;
    client.h = self.h - titlebarHeight - 2 * borderWidth;

    content.w = content.bufferW;
    content.h = content.bufferH;

    content.x = ( client.w - content.w ) / 2;
    content.y = ( client.h - content.h ) / 2;

    if( content.x <= 0 )
    {
        content.x = 0;
        content.w = client.w;
    }

    if( content.y <= 0 )
    {
        content.y = 0;
        content.h = client.h;
    }

    content.x += borderWidth;
    content.y += borderWidth + titlebarHeight;

    if( content.w > 0 && content.h > 0 )
    {
        client.color = makeColor( 60, 70, 200, 255 );
    }
    else
    {
        content.w = 0;
        content.h = 0;
        client.color = makeColor( 170, 170, 170, 255 );
    }
}

void Window::run()
{
    GenericWindow g( *this, nullptr );
    g.run();
}

uint32_t makeColor( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
    return ( a << 24 ) | ( r << 16 ) | ( g << 8 ) | b;
}
}

Settings::Settings( std::wstring tl, const Parameters& p ) : parameters( p )
{
    using namespace GraphicInterface;

    std::vector<std::shared_ptr<Combobox>> comboboxes;

    auto make = [this, &comboboxes]( Settings::Parameter & value, int position, int width, int height )
    {
        if( value.get )
        {
            auto object = std::make_shared<StaticText>();
            fields.push_back( object );
            add( object.get() );

            object->y = position;
            object->w = width;
            object->h = height;

            object->value = value.name;
            object->prepare( client.color );
        }

        if( !value.get )
        {
            auto object = std::make_shared<TextButton>();
            fields.push_back( object );
            add( object.get() );

            object->desc = value.name;
            object->y = position;
            object->w = width;
            object->h = height;

            object->use = [s = value.set]( bool release )
            {
                if( release )
                    s( L"released" );
            };
            return;
        }

        auto initialValue = value.get();

        if( !value.options.empty() )
        {
            auto object = std::make_shared<Combobox>();
            comboboxes.push_back( object );

            object->y = position + height;
            object->w = width;
            object->h = height;

            object->setCallback = value.set;
            object->options = value.options;
            object->option = std::find( value.options.begin(), value.options.end(), value.get() ) - value.options.end();
            if( object->option > value.options.size() )
                object->option = 0;
            return;
        }

        auto object = std::make_shared<DynamicText>();
        fields.push_back( object );
        add( object.get() );

        object->y = position + height;
        object->w = width;
        object->h = height;

        object->setCallback = value.set;
        object->value = value.get();
        object->prepare();
    };

    self.w = 300;
    self.h = 1024;
    title.value = tl;
    title.prepare( titleBar.color );
    Window::update();

    int position = 16 + Window::titleBar.h;
    for( auto& v : parameters )
    {
        make( v, position, 512, v.get ? 16 : 32 );
        position += 48;
    }

    // Adding comboboxes after other objects, so they are not covered by anything else
    for( auto& combobox : comboboxes )
    {
        add( combobox.get() );
        fields.push_back( std::move( combobox ) );
    }
}

Settings::Settings( const Settings& other ) : Settings( other.title.value, other.parameters )
{}

Settings::~Settings()
{}

void Settings::update()
{
    Window::update();

    for( auto& field : fields )
    {
        field->w = self.w - 2 * borderWidth - 2 * 16;
        field->x = self.x + borderWidth + 16;
    }
}

Popup::Popup( Type t, std::wstring tl, std::wstring inf ) : type( t )
{
    using namespace GraphicInterface;

    self.w = 320;
    self.h = 240;

    title.value = tl;
    title.prepare( titleBar.color );

    switch( type )
    {
    case Type::Info:
        inf = L"Info: " + inf;
        break;
    case Type::Error:
        inf = L"Error: " + inf;
        break;
    case Type::Warning:
        inf = L"Warning: " + inf;
        break;
    case Type::Question:
        inf = L"Question: " + inf;
        break;
    default:
        break;
    }

    Window::update();
    info.value = inf;
    info.prepare( client.color );
    if( info.w + 16 > self.w )
        self.w = info.w + 16;

    add( &info );

    if( type == Type::Question )
    {
        buttons.push_back( &yesButton );
        yesButton.desc = L"yes";
        yesButton.use = [this]( bool release )
        {
            if( release )
                answer = true;
            closeButton.use( release );
        };

        buttons.push_back( &noButton );
        noButton.desc = L"no";
        noButton.use = [this]( bool release )
        {
            if( release )
                answer = false;
            closeButton.use( release );
        };
    }
    else
    {
        buttons.push_back( &cancelButton );
        cancelButton.desc = L"ok";
        cancelButton.use = [this]( bool release )
        {
            closeButton.use( release );
        };
    }

    for( auto button : buttons )
        add( button );
}

Popup::Popup( const Popup& other ) : Popup( other.type, other.title.value, other.info.value )
{
    answer = other.answer;

    switch( type )
    {
    case Type::Info:
        info.value = info.value.substr( 6 );
        break;
    case Type::Error:
        info.value = info.value.substr( 7 );
        break;
    case Type::Warning:
        info.value = info.value.substr( 9 );
        break;
    case Type::Question:
        info.value = info.value.substr( 10 );
        break;
    default:
        break;
    }

    info.prepare( client.color );
}

Popup::~Popup()
{}

void Popup::update()
{
    Window::update();

    info.x = client.x + 16;
    info.y = client.y + 16;

    auto count = buttons.size();

    int bw = ( client.w - ( count + 1 ) * 16 ) / count;
    int bx = 16;

    size_t i = 0;
    for( auto button : buttons )
    {
        button->h = 24;
        button->w = bw;

        button->x = client.x + bx;
        button->y = client.y + client.h - button->h - 32;

        bx += bw + 16;
        ++i;
    }
}

static void storageCapacity( ContextMenu::Parameters& parameters, size_t &index )
{
    ++index;

    for( auto& p : parameters )
    {
        if( p.name.empty() )
            continue;

        if( p.active && !p.parameters.empty() )
            storageCapacity( p.parameters, index );
    }
}

static void sidedrop( ContextMenu& root, ContextMenu::Parameters& parameters, int& x, int& y, size_t &index, GraphicInterface::ActiveTrigger *last )
{
    using namespace GraphicInterface;

    auto& result = root.storage.emplace_back();
    ++index;

    int width = 256, maxX = x, maxY = y;

    auto trigger = std::make_shared<ActiveTrigger>();
    auto current = trigger.get();
    trigger->x = x - 1;
    trigger->y = y;
    trigger->activateByHovering = true;
    trigger->use = [&root, id = index - 1, last]( bool inside )
    {
        if( !inside )
        {
            if( last && last->hovered )
            {
                for( size_t i = id; i < root.storage.size(); ++i )
                {
                    for( auto& item : root.storage[i] )
                        item->w = 0;
                }
            }
            else
            {
                root.closeButton.use( true );
            }
        }
    };

    for( auto& p : parameters )
    {
        if( p.name.empty() )
        {
            auto separator = std::make_shared<GraphicInterface::Rectangle>();
            separator->x = x;
            separator->y = y;
            separator->w = width;
            separator->h = 1;
            separator->color = makeColor( 50, 50, 50, 255 );

            y += separator->h;
            result.push_back( separator );
            continue;
        }

        auto button = std::make_shared<TextButton>();
        button->x = x;
        button->y = y;
        button->w = width;
        button->h = 16;
        button->centerX = false;
        button->desc = p.name;
        button->off = !p.active;

        if( p.parameters.empty() && p.callback )
        {
            button->use = [callback = p.callback]( bool release )
            {
                if( release )
                    callback();
            };
        }

        if( p.active && !p.parameters.empty() )
        {
            size_t drop = index;

            int nextX = x + width + 1, nextY = y;
            sidedrop( root, p.parameters, nextX, nextY, index, current );

            auto& items = root.storage[drop];
            auto next = dynamic_cast<ActiveTrigger*>( items.back().get() );

            if( maxX < nextX )
                maxX = nextX;

            if( maxY < nextY )
                maxY = nextY;

            // Sub-menu
            button->activateByHovering = true;
            button->use = [current, next, width, &items]( bool inside )
            {
                if( inside )
                {
                    for( auto& item : items )
                        item->w = width;
                    items.back()->w += 1;
                }
                else if( !next->hovered )
                {
                    for( auto& item : items )
                        item->w = 0;
                }
                else
                {
                    current->wasHovered = false;
                }
            };

            for( auto& item : items )
                item->w = 0;
        }

        button->off = button->off && button->use;

        y += button->h;
        result.push_back( button );
    }

    trigger->w = width + 1;
    trigger->h = y - trigger->y;
    result.push_back( trigger );

    if( x < maxX )
        x = maxX;

    if( y < maxY )
        y = maxY;
}

ContextMenu::ContextMenu( Parameters p ) : parameters( std::move( p ) )
{
    int x = 0, y = 0;
    size_t index = 0;

    storageCapacity( parameters, index );
    storage.reserve( index );

    index = 0;
    sidedrop( *this, parameters, x, y, index, nullptr );

    for( auto i = storage.rbegin(); i != storage.rend(); ++i )
    {
        auto& items = *i;
        for( auto& item : items )
        {
            add( item.get() );
        }
    }
}

ContextMenu::ContextMenu( const ContextMenu& other ) : ContextMenu( other.parameters )
{}

ContextMenu::~ContextMenu()
{}

void ContextMenu::update()
{}

void ContextMenu::run()
{
    if( storage.empty() )
        return;

    if( storage[0].empty() )
        return;

    self.x = 0;
    self.y = 0;
    self.w = GetSystemMetrics( SM_CXSCREEN );
    self.h = GetSystemMetrics( SM_CYSCREEN );

    client.place( self );
    client.color = GraphicInterface::makeColor( 0, 0, 0, 1 );

    POINT p;
    if( !GetCursorPos( &p ) )
        return;

    int x = p.x - storage[0][0]->x;
    int y = p.y - storage[0][0]->y;

    for( auto & items : storage )
    {
        for( auto& item : items )
        {
            item->x += x;
            item->y += y;
        }
    }

    Window::run();
}

FileManager::FileManager( bool write )
{
    add( &confirm );
    confirm.desc = write ? L"Save" : L"Open";
    if( write )
    {
        confirm.use = [this]( bool release )
        {
            if( !release )
                return;

            std::filesystem::path candidate = file.value;
            if( std::filesystem::exists( candidate ) )
            {
                if( std::filesystem::is_regular_file( candidate ) )
                {
                    Popup question( Popup::Type::Question, confirm.desc + L" file", L"File already exists, do you want to overwrite it?" );
                    question.run();
                    if( question.answer && *question.answer )
                    {
                        choice = candidate;
                        closeButton.use( true );
                    }
                }
                else
                {
                    Popup warning( Popup::Type::Warning, confirm.desc + L" file", L"It's not a file." );
                    warning.run();
                }
            }
            else
            {
                choice = candidate;
                closeButton.use( true );
            }
        };
    }
    else
    {
        confirm.use = [this]( bool release )
        {
            if( !release )
                return;

            std::filesystem::path candidate = file.value;
            if( std::filesystem::exists( candidate ) && std::filesystem::is_regular_file( candidate ) )
            {
                choice = candidate;
                closeButton.use( true );
            }
            else
            {
                Popup warning( Popup::Type::Warning, confirm.desc + L" file", L"File with such path does not exist." );
                warning.run();
            }
        };
    }

    add( &reject );
    reject.desc = L"Cancel";
    reject.use = [this]( bool release )
    {
        closeButton.use( release );
    };

    self.w = 512;
    self.h = 1024;
    root = L"C:\\Users\\User\\Downloads";

    add( &file );
    file.value = root->wstring();
    file.prepare();

    select();
}

FileManager::~FileManager()
{}

void FileManager::select()
{
    if( !root )
        return;

    for( auto& path : paths )
        remove( path.get() );
    paths.clear();

    auto addButton = [this]( const std::filesystem::path & path )
    {
        auto button = paths.emplace_back( std::make_shared<GraphicInterface::TextButton>() ).get();
        add( button );

        button->desc = path.filename().wstring();
        if( button->desc.empty() )
            button->desc = path.wstring();

        button->centerX = false;

        if( std::filesystem::is_directory( path ) )
        {
            button->use = [this, p = path.lexically_normal()]( bool release )
            {
                if( !release )
                    return;

                root = p;
                file.value = p.wstring();
                file.prepare();
            };
        }
        else if( std::filesystem::is_regular_file( path ) )
        {
            button->use = [this, p = path.lexically_normal()]( bool release )
            {
                if( !release )
                    return;

                file.value = p.wstring();
                file.prepare( true );
            };
        }
    };

    try
    {
        if( *root == root->parent_path() )
        {
            wchar_t drives[MAX_PATH];
            if( GetLogicalDriveStringsW( MAX_PATH, drives ) )
            {
                wchar_t* drive = drives;
                while( *drive )
                {
                    addButton( drive );
                    drive += wcslen( drive ) + 1;
                }
            }
            else
            {
                Popup( Popup::Type::Error, L"Error", L"Failed to get list of drives." ).run();
            }
        }
        else
        {
            addButton( *root / L".." );
        }
        for( const auto &directory : std::filesystem::directory_iterator{*root} )
            addButton( directory.path() );
    }
    catch( const Exception &e )
    {
        Popup( Popup::Type::Error, L"Error", e.message() ).run();
    }
    catch( const std::exception &e )
    {
        Popup( Popup::Type::Error, L"Error", Exception::extract( e.what() ) ).run();
    }
    catch( ... )
    {
        Popup( Popup::Type::Error, L"Error", L"Program failed!" ).run();
    }

    root.reset();
}

void FileManager::update()
{
    select();

    Window::update();

    auto px = client.x + 8;
    auto py = client.y + 8;
    auto pw = client.w - 16;
    auto ph = 16;

    file.x = px;
    file.y = py;
    file.w = pw;
    file.h = ph;
    py += ph + 8;

    confirm.x = px;
    confirm.y = py;
    confirm.w = pw / 2 - 8;
    confirm.h = ph;

    reject.x = px + confirm.w + 16;
    reject.y = py;
    reject.w = confirm.w;
    reject.h = ph;
    py += ph + 16;

    for( auto& path : paths )
    {
        path->x = px;
        path->y = py;
        path->w = pw;
        path->h = ph;
        py += ph + 8;
    }
}

std::optional<std::filesystem::path> savePath()
{
    FileManager fm( true );
    fm.run();
    return fm.choice;
}

std::optional<std::filesystem::path> openPath()
{
    FileManager fm( false );
    fm.run();
    return fm.choice;
}

static void updateWindowContent( GraphicInterface::Window &desc, HWND hwnd )
{
    RECT rect;
    GetWindowRect( hwnd, &rect );

    auto width = rect.right - rect.left;
    auto height = rect.bottom - rect.top;

    BITMAPINFO bmi;
    clear( &bmi, sizeof( bmi ) );
    bmi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC( nullptr );
    HDC hdcMem    = CreateCompatibleDC( hdcScreen );
    void *pBits  = nullptr;
    HBITMAP hBitmap = CreateDIBSection( hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0 );
    if( !hBitmap )
    {
        DeleteDC( hdcMem );
        ReleaseDC( nullptr, hdcScreen );
        return;
    }
    HBITMAP hOldBmp = ( HBITMAP )SelectObject( hdcMem, hBitmap );

    auto oldX = desc.self.x;
    auto oldY = desc.self.y;

    desc.self.x = 0;
    desc.self.y = 0;
    desc.self.w = width;
    desc.self.h = height;
    desc.update();

    auto *pixels = ( uint32_t * )pBits;
    desc.draw( pixels, width, height );

    BLENDFUNCTION blend;
    clear( &blend, sizeof( blend ) );
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255; // Use per-pixel alpha.
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT ptZero = {0, 0};
    SIZE sizeWindow = {width, height};
    UpdateLayeredWindow( hwnd, hdcScreen, nullptr, &sizeWindow, hdcMem, &ptZero, 0, &blend, ULW_ALPHA );

    SelectObject( hdcMem, hOldBmp );
    DeleteObject( hBitmap );
    DeleteDC( hdcMem );
    ReleaseDC( nullptr, hdcScreen );

    desc.self.x = oldX;
    desc.self.y = oldY;
}

ChangedValue<bool> &GenericWindow::Keys::letter( char symbol )
{
    makeException( 'A' <= symbol && symbol <= 'Z' );
    return letters[symbol - 'A'];
}

const ChangedValue<bool> &GenericWindow::Keys::letter( char symbol ) const
{
    makeException( 'A' <= symbol && symbol <= 'Z' );
    return letters[symbol - 'A'];
}

ChangedValue<bool> &GenericWindow::Keys::digit( unsigned short symbol )
{
    makeException( 0 <= symbol && symbol <= 9 );
    return letters[symbol];
}

const ChangedValue<bool> &GenericWindow::Keys::digit( unsigned short symbol ) const
{
    makeException( 0 <= symbol && symbol <= 9 );
    return letters[symbol];
}

void GenericWindow::Keys::reset()
{
    for( auto &letter : letters )
        letter.reset();
    for( auto &digit : digits )
        digit.reset();
}

void GenericWindow::Keys::release()
{
    for( auto &letter : letters )
        letter = false;
    for( auto &digit : digits )
        digit = false;
}

class GenericWindow::Implementation
{
public:
    GenericWindow *window;

    std::wstring className;
    ATOM windowClass;
    HWND hwnd;

    Implementation( WNDPROC windowProc, GenericWindow *w ) : window( w )
    {
        static long long unsigned index = 0;

        className = L"GenericWindowWinApiImplementation" + std::to_wstring( index++ );

        WNDCLASSEXW wc;
        clear( &wc, sizeof( wc ) );
        wc.cbSize = sizeof( wc );
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleW( nullptr );
        wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
        wc.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
        wc.lpszClassName = className.c_str();

        windowClass = RegisterClassExW( &wc );
        makeException( windowClass );

        hwnd = nullptr;
    }

    ~Implementation()
    {
        UnregisterClassW( className.c_str(), GetModuleHandleW( nullptr ) );
    }
};

GenericWindow::OutputData::OutputData( GraphicInterface::Window &desc ) : image( desc.content )
{}

GenericWindow::GenericWindow( GraphicInterface::Window &d, HandleMsg h )
    : handleMsg( std::move( h ) ), outputData( d ), desc( d )
{
    implementation = nullptr;

    // This code is positioned in lambda to accesses private members of GenericWindow
    auto windowProc = []( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) -> LRESULT
    {
        auto impl = ( Implementation * )GetWindowLongPtr( hwnd, GWLP_USERDATA );

        auto inputReset = [&impl]()
        {
            if( impl )
                impl->window->inputReset();
        };

        auto inputRelease = [&impl]()
        {
            if( impl )
                impl->window->inputRelease();
        };

        auto quit = [hwnd]()
        {
            DestroyWindow( hwnd );
        };

        auto getPos = [hwnd]( int &x, int &y )
        {
            POINT point;
            GetCursorPos( &point );

            RECT rect;
            GetWindowRect( hwnd, &rect );

            x = point.x - rect.left;
            y = point.y - rect.top;
        };

        auto handle = [&]()
        {
            if( !impl || !impl->window )
                return;

            try
            {
                if( impl->window->handleMsg )
                    impl->window->handleMsg( impl->window->inputData, impl->window->outputData );
            }
            catch( const Exception &e )
            {
                Popup( Popup::Type::Error, L"Error", e.message() ).run();
                quit();
                return;
            }
            catch( const std::exception &e )
            {
                Popup( Popup::Type::Error, L"Error", Exception::extract( e.what() ) ).run();
                quit();
                return;
            }
            catch( ... )
            {
                Popup( Popup::Type::Error, L"Error", L"Program failed!" ).run();
                quit();
                return;
            }

            if( impl->window->outputData.quit )
            {
                quit();
                return;
            }

            inputReset();

            auto &img = impl->window->outputData.image;
            if( img.changed() )
            {
                updateWindowContent( impl->window->desc, hwnd );
                img.reset();
            }

            auto &popup = impl->window->outputData.popup;
            if( popup.changed() )
            {
                popup.get().run();
                popup.reset();
            }

            impl->window->inputData.init = false;
        };

        switch( message )
        {
        case WM_CREATE:
            impl = ( Implementation * )( ( LPCREATESTRUCT )lParam )->lpCreateParams;
            SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )impl );
            impl->window->inputData.init = true;
            handle();
            break;
        case WM_DESTROY:
            impl->hwnd = nullptr;
            impl = nullptr;
            SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )impl );
            // PostQuitMessage( 0 );
            return 0;
        case WM_APP:
            break;
        case WM_NCHITTEST:
            {
                RECT rect;
                GetWindowRect( hwnd, &rect );
                int x = LOWORD( lParam ) - rect.left;
                int y = HIWORD( lParam ) - rect.top;

                bool left   = impl->window->desc.leftTrigger.contains( x, y );
                bool right  = impl->window->desc.rightTrigger.contains( x, y );
                bool top    = impl->window->desc.topTrigger.contains( x, y );
                bool bottom = impl->window->desc.bottomTrigger.contains( x, y );

                if( top && left ) return HTTOPLEFT;
                if( top && right ) return HTTOPRIGHT;
                if( bottom && left ) return HTBOTTOMLEFT;
                if( bottom && right ) return HTBOTTOMRIGHT;

                if( left ) return HTLEFT;
                if( right ) return HTRIGHT;
                if( top ) return HTTOP;
                if( bottom ) return HTBOTTOM;

                for( auto active : impl->window->desc.interactive )
                {
                    if( active->contains( x, y ) )
                        return HTCLIENT;
                }

                if( impl->window->desc.titleBar.contains( x, y ) )
                    return HTCAPTION;

                return HTCLIENT;
            }
        case WM_GETMINMAXINFO:
            {
                if( impl )
                {
                    MINMAXINFO *p = ( MINMAXINFO * )lParam;
                    p->ptMinTrackSize.x = impl->window->desc.minWidth();
                    p->ptMinTrackSize.y = impl->window->desc.minHeight();
                    return 0;
                }
            }
            break;
        case WM_KILLFOCUS:
            inputRelease();
            inputReset();
            break;
        case WM_SETFOCUS:
            inputRelease();
            inputReset();
            break;
        case WM_SETCURSOR:
            // Set the cursor to the default arrow
            SetCursor( LoadCursorW( nullptr, IDC_ARROW ) );
            break;
        case WM_SIZE:
            {
                updateWindowContent( impl->window->desc, hwnd );
                break;
            }
        case WM_MOVE:
            {
                RECT rect;
                GetWindowRect( hwnd, &rect );
                impl->window->desc.self.x = rect.left;
                impl->window->desc.self.y = rect.top;
                break;
            }
        case WM_PAINT:
            break;
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
            {
                int x, y;
                getPos( x, y );

                impl->window->desc.hover( x, y );

                if( impl->window->desc.content.contains( x, y ) )
                {
                    impl->window->inputData.mouseX = x - impl->window->desc.content.x;
                    impl->window->inputData.mouseY = y - impl->window->desc.content.y;
                    handle();
                }

                updateWindowContent( impl->window->desc, hwnd );
            }
            return 0;
        case WM_LBUTTONDOWN:
            {
                int x, y;
                getPos( x, y );

                impl->window->desc.click( false, x, y );

                if( impl->window->desc.content.contains( x, y ) )
                {
                    impl->window->inputData.leftMouse = true;
                    handle();
                }

                updateWindowContent( impl->window->desc, hwnd );
            }
            return 0;
        case WM_LBUTTONUP:
            {
                int x, y;
                getPos( x, y );

                impl->window->desc.click( true, x, y );

                if( impl->window->desc.content.contains( x, y ) )
                {
                    impl->window->inputData.leftMouse = false;
                    handle();
                }

                updateWindowContent( impl->window->desc, hwnd );
            }
            return 0;
        case WM_RBUTTONDOWN:
            {
                int x, y;
                getPos( x, y );
                if( impl->window->desc.content.contains( x, y ) )
                {
                    impl->window->inputData.rightMouse = true;
                    handle();
                }
            }
            return 0;
        case WM_RBUTTONUP:
            {
                int x, y;
                getPos( x, y );
                if( impl->window->desc.content.contains( x, y ) )
                {
                    impl->window->inputData.rightMouse = false;
                    handle();
                }
            }
            return 0;
        case WM_MBUTTONDOWN:
            {
                int x, y;
                getPos( x, y );
                if( impl->window->desc.content.contains( x, y ) )
                {
                    impl->window->inputData.middleMouse = true;
                    handle();
                }
            }
            return 0;
        case WM_MBUTTONUP:
            {
                int x, y;
                getPos( x, y );
                if( impl->window->desc.content.contains( x, y ) )
                {
                    impl->window->inputData.middleMouse = false;
                    handle();
                }
            }
            return 0;
        case WM_CHAR:
            {
                if( wParam == '-' || wParam == '/' || wParam == '\\' || wParam == '+' || wParam == '.' || wParam == '_' || wParam == '\b' || wParam == ' ' )
                {
                    impl->window->desc.input( wParam );
                    updateWindowContent( impl->window->desc, hwnd );
                    return 0;
                }

                if( ( '0' <= wParam && wParam <= '9' ) || ( 'a' <= wParam && wParam <= 'z' ) || ( 'A' <= wParam && wParam <= 'Z' ) )
                {
                    impl->window->desc.input( wParam );
                    updateWindowContent( impl->window->desc, hwnd );
                    return 0;
                }
            }
            return 0;
        default:
            break;
        }

        bool pressed = message == WM_KEYDOWN;
        bool pressedSystem = message == WM_SYSKEYDOWN;
        pressed = pressed || pressedSystem;

        bool released = message == WM_KEYUP;
        bool releasedSystem = message == WM_SYSKEYUP;
        released = released || releasedSystem;

        bool system = pressedSystem || releasedSystem;

        if( pressed || released )
        {
            auto &input = impl->window->inputData;
            switch( wParam )
            {
            case VK_UP:
                if( !system )
                {
                    input.up = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_DOWN:
                if( !system )
                {
                    input.down = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_LEFT:
                if( !system )
                {
                    input.left = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_RIGHT:
                if( !system )
                {
                    input.right = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_ESCAPE:
                if( !system )
                {
                    input.escape = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_DELETE:
                if( !system )
                {
                    input.del = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_CONTROL:
                if( !system )
                {
                    input.ctrl = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_SHIFT:
                if( !system )
                {
                    input.shift = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_SPACE:
                if( !system )
                {
                    input.space = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_RETURN:
                if( !system )
                {
                    input.enter = pressed;
                    handle();
                    return 0;
                }
                break;
            case VK_F1:
                if( !system )
                {
                    input.f1 = pressed;
                    handle();
                    return 0;
                }
            default:
                break;
            }

            if( !system )
            {
                if( 'A' <= wParam && wParam <= 'Z' )
                {
                    auto &key = impl->window->inputData.keys.letter( wParam );
                    key = pressed;
                    handle();
                    return 0;
                }

                if( '0' <= wParam && wParam <= '9' )
                {
                    auto &key = impl->window->inputData.keys.digit( wParam - '0' );
                    key = pressed;
                    handle();
                    return 0;
                }
            }
        }

        return DefWindowProc( hwnd, message, wParam, lParam );
    };

    implementation = new Implementation( windowProc, this );

    desc.closeButton.use = [this]( bool release )
    {
        if( release )
            close();
    };

    desc.maximizeButton.use = [this]( bool release )
    {
        if( release )
            maximize();
    };

    desc.minimizeButton.use = [this]( bool release )
    {
        if( release )
            minimize();
    };
}

GenericWindow::~GenericWindow()
{
    desc.closeButton.use = nullptr;
    desc.maximizeButton.use = nullptr;
    desc.minimizeButton.use = nullptr;
    delete implementation;
}

void GenericWindow::run()
{
    auto lastWindow = GetForegroundWindow();

    if( desc.self.x < 0 || desc.self.y < 0 )
    {
        RECT screenRect;
        GetClientRect( GetDesktopWindow(), &screenRect );

        float scalar = 0.0625f;
        int width = desc.content.w;
        int height = desc.content.h;
        int screenWidth = screenRect.right - screenRect.left;
        int screenHeight = screenRect.bottom - screenRect.top;
        float shiftX = ( screenWidth - width ) * scalar;
        float shiftY = ( screenHeight - height ) * scalar;
        desc.self.x = desc.self.y = Min( shiftX, shiftY );
    }

    auto hwnd = implementation->hwnd = CreateWindowExW( WS_EX_LAYERED,
                                       implementation->className.c_str(), desc.title.value.c_str(),
                                       WS_POPUP | WS_THICKFRAME | WS_VISIBLE,
                                       desc.self.x, desc.self.y, desc.self.w, desc.self.h,
                                       nullptr, nullptr, GetModuleHandleW( nullptr ), implementation );
    makeException( hwnd );
    updateWindowContent( desc, hwnd );

    MSG msg;
    BOOL result;
    while( implementation->hwnd && ( result = GetMessageW( &msg, hwnd, 0, 0 ) ) != 0 )
    {
        makeException( result != -1 );
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    DWORD windowProcessId = 0;
    GetWindowThreadProcessId( lastWindow, &windowProcessId );
    if( windowProcessId == GetCurrentProcessId() )
        SetForegroundWindow( lastWindow );
}

void GenericWindow::close()
{
    DestroyWindow( implementation->hwnd );
}

void GenericWindow::maximize()
{
    if( IsZoomed( implementation->hwnd ) )
        ShowWindow( implementation->hwnd, SW_RESTORE );
    else
        ShowWindow( implementation->hwnd, SW_MAXIMIZE );
}

void GenericWindow::minimize()
{
    ShowWindow( implementation->hwnd, SW_MINIMIZE );
}

void GenericWindow::inputReset()
{
    inputData.up.reset();
    inputData.down.reset();
    inputData.left.reset();
    inputData.right.reset();
    inputData.escape.reset();
    inputData.del.reset();
    inputData.ctrl.reset();
    inputData.shift.reset();
    inputData.space.reset();
    inputData.enter.reset();
    inputData.leftMouse.reset();
    inputData.rightMouse.reset();
    inputData.middleMouse.reset();
    inputData.mouseX.reset();
    inputData.mouseY.reset();
    inputData.keys.reset();
}

void GenericWindow::inputRelease()
{
    inputData.up = false;
    inputData.down = false;
    inputData.left = false;
    inputData.right = false;
    inputData.escape = false;
    inputData.del = false;
    inputData.ctrl = false;
    inputData.shift = false;
    inputData.space = false;
    inputData.enter = false;
    inputData.leftMouse = false;
    inputData.rightMouse = false;
    inputData.middleMouse = false;
    inputData.keys.release();
}
