#include "Window.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <memory>

#include "UnicodeString.h"
#include "Exception.h"
#include "Lambda.h"
#include "Basic.h"

namespace GraphicInterface
{
static void drawLineR( uint32_t *pixels, int width, int height, int x, int y, int size, uint32_t color )
{
    if( y < 0 || height <= y )
        return;

    int x0 = x;
    int x1 = x + size;

    if( x0 < 0 )
        x0 = 0;

    if( x1 > width )
        x1 = width;

    for( x = x0; x < x1; ++x )
        pixels[y * width + x] = color;
}

static void drawLineD( uint32_t *pixels, int width, int height, int x, int y, int size, uint32_t color )
{
    if( x < 0 || width <= x )
        return;

    int y0 = y;
    int y1 = y + size;

    if( y0 < 0 )
        y0 = 0;

    if( y1 > height )
        y1 = height;

    for( y = y0; y < y1; ++y )
        pixels[y * width + x] = color;
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
    const std::wstring& text, const std::wstring& fontName, uint32_t color, int padding,
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
            uint8_t b = bytes[idx * 4 + 0];
            uint8_t g = bytes[idx * 4 + 1];
            uint8_t r = bytes[idx * 4 + 2];

            // Derive alpha from luminance (white background, black text)
            int lum = ( 77 * r + 150 * g + 29 * b ) >> 8;
            uint32_t a = 255 - lum;

            outBuffer[idx] = makeColor( getR( color ), getG( color ), getB( color ), a );
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

Object::Object() : visible( true ), x( 0 ), y( 0 )
{}

Object::Object( const Object& other ) : visible( other.visible ), x( other.x ), y( other.y )
{}

Object::~Object()
{}

void Object::inner( int& x0, int& y0 ) const
{
    x0 -= x;
    y0 -= y;
}

void Object::outer( int& dx, int& dy ) const
{
    dx += x;
    dy += y;
}

Group::Group() : Object()
{}

Group::Group( const Group& other ) : Object( other )
{}

Group::~Group()
{}

bool Group::contains( int x0, int y0 ) const
{
    inner( x0, y0 );
    for( auto object : objects )
    {
        if( object->visible && object->contains( x0, y0 ) )
            return true;
    }
    return false;
}

void Group::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    outer( dx, dy );
    for( auto object : objects )
    {
        if( object->visible )
            object->draw( pixels, width, height, dx, dy );
    }
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

bool ActiveGroup::hover( int x0, int y0 )
{
    inner( x0, y0 );

    hovered = false;

    bool absorbed = false;
    for( auto i = interactive.rbegin(); i != interactive.rend(); ++i )
    {
        auto active = *i;
        bool contains = !absorbed && active->visible && active->contains( x0, y0 );
        active->hovered = contains;
        if( contains )
        {
            hovered = true;
            absorbed = false;
        }
    }

    bool needFocus = false;
    for( auto active : interactive )
    {
        if( focusIt( target, active, active->visible && active->hover( x0, y0 ) ) )
            needFocus = true;
    }

    focus( needFocus );
    return needFocus;
}

bool ActiveGroup::click( bool release, int x0, int y0 )
{
    inner( x0, y0 );

    for( auto i = interactive.rbegin(); i != interactive.rend(); ++i )
    {
        auto& active = * i;
        if( active->visible && active->contains( x0, y0 ) )
        {
            if( focusIt( target, active, active->click( release, x0, y0 ) ) )
            {
                focus( true );
                return true;
            }
        }
    }

    focus( false );
    return false;
}

bool ActiveGroup::input( wchar_t c )
{
    bool result = target && target->input( c );
    focus( result );
    return result;
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

bool ActiveGroup::activeContains( int x0, int y0 ) const
{
    inner( x0, y0 );

    for( auto active : interactive )
    {
        if( active->visible && active->contains( x0, y0 ) )
            return true;
    }
    return false;
}

Box::Box() : Object(), w( 0 ), h( 0 )
{}

Box::Box( const Box& other ) : Object( other )
{}

Box::~Box()
{}

bool Box::contains( int x0, int y0 ) const
{
    inner( x0, y0 );
    return 0 <= x0 && x0 < w && 0 <= y0 && y0 < h;
}

Box& Box::place( const Box& other )
{
    x = other.x;
    y = other.y;
    w = other.w;
    h = other.h;
    return *this;
}

static void fill( uint32_t *pixels, int width, int height, uint32_t c, int w, int h, int x, int y )
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

static void gradient( uint32_t *pixels, int width, int height, int w, int h, int x, int y )
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

void Trigger::draw( uint32_t *, int, int, int, int ) const
{}

Rectangle::Rectangle() : color( 0 )
{}

Rectangle::Rectangle( const Rectangle& other ) : Object( other ), Box( other ), color( other.color )
{}

Rectangle::~Rectangle()
{}

void Rectangle::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    outer( dx, dy );
    fill( pixels, width, height, color, w, h, dx, dy );
}

ImageBase::ImageBase() : Box(), bufferW( 0 ), bufferH( 0 )
{}

ImageBase::ImageBase( const ImageBase& other ) : Object( other ), Box( other ), pixels( other.pixels ), bufferW( other.bufferW ), bufferH( other.bufferH )
{}

ImageBase::~ImageBase()
{}

void ImageBase::prepare( int stride, int height )
{
    bufferW = w = Abs( stride );
    bufferH = h = height;

    pixels.resize( w * h );
}

Image::Image() : ImageBase()
{}

Image::Image( const Image& other ) : Object( other ), ImageBase( other )
{}

Image::~Image()
{}

void Image::prepare( const void *data, int stride, int height )
{
    ImageBase::prepare( stride, height );

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

void Image::draw( uint32_t *otherPixels, int width, int height, int dx, int dy ) const
{
    outer( dx, dy );

    int x0 = dx;
    int y0 = dy;
    int x1 = x0 + Min( w, bufferW );
    int y1 = y0 + Min( h, bufferH );

    int bx0 = x0;

    if( x0 < 0 )
        x0 = 0;
    if( y0 < 0 )
        y0 = 0;

    if( x1 > width )
        x1 = width;
    if( y1 > height )
        y1 = height;

    bx0 = x0 - bx0;

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

ImageBlend::ImageBlend() : ImageBase()
{}

ImageBlend::ImageBlend( const ImageBlend& other ) : Object( other ), ImageBase( other )
{}

ImageBlend::~ImageBlend()
{}

void ImageBlend::prepare( const void *data, int stride, int height )
{
    ImageBase::prepare( stride, height );

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
            o.rgbReserved = i.rgbReserved;
        }
        output += bufferW;
        input += stride;
        --height;
    }
}

void ImageBlend::draw( uint32_t *otherPixels, int width, int height, int dx, int dy ) const
{
    outer( dx, dy );

    int x0 = dx;
    int y0 = dy;
    int x1 = x0 + Min( w, bufferW );
    int y1 = y0 + Min( h, bufferH );

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
            auto& result = otherPixels[j * width + i];
            auto& sample = pixels[( j - dy ) * bufferW + ( i - dx )];

            auto r = getR( result );
            auto g = getG( result );
            auto b = getB( result );
            auto a = getA( result );

            auto sr = getR( sample );
            auto sg = getG( sample );
            auto sb = getB( sample );
            auto sa = getA( sample );

            r = r * ( 255 - sa ) / 255 + sr;
            g = g * ( 255 - sa ) / 255 + sg;
            b = b * ( 255 - sa ) / 255 + sb;
            a = a * ( 255 - sa ) / 255 + sa;

            result = makeColor( r, g, b, a );
        }
    }
}

StaticText::StaticText() : ImageBlend()
{
    color = makeColor( 0, 0, 0, 255 );
}

StaticText::StaticText( const StaticText& other ) : Object( other ), ImageBlend( other ), color( other.color ), value( other.value )
{}

StaticText::~StaticText()
{}

void StaticText::prepare()
{
    std::vector<uint32_t> outBuffer;
    int outWidth, outHeight;

    if( renderTextToBuffer( value, L"DejaVuSansMono", color, 0, outWidth, outHeight, outBuffer ) )
    {
        ImageBlend::prepare( ( const void* )outBuffer.data(), outWidth, outHeight );
    }
    else
    {
        bufferW = bufferH = 0;
        pixels.clear();
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
    if( write )
        valid = setCallback ? setCallback( value ) : true;
    StaticText::prepare();
}

void DynamicText::draw( uint32_t *canvas, int width, int height, int dx, int dy ) const
{
    auto idleColor = makeColor( 255, 255, 255, 255 );
    auto errorColor = makeColor( 255, 127, 127, 255 );
    auto focusColor = makeColor( 255, 255, 127, 255 );
    auto background = valid ? ( focused ? focusColor : idleColor ) : errorColor;

    int x1 = dx, y1 = dy;
    outer( dx, dy );
    fill( canvas, width, height, background, w, h, dx, dy );
    StaticText::draw( canvas, width, height, x1, y1 );
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

void Combobox::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    auto base = makeColor( 255, 255, 255, 255 );
    auto selection = makeColor( 255, 255, 127, 255 );
    StaticText text;
    Rectangle area;

    outer( dx, dy );

    area.w = w;
    area.h = 16;

    auto drawItem = [&]( std::wstring o, size_t i, uint32_t c )
    {
        area.color = c;
        area.x = 0;
        area.y = i * 16;
        area.draw( pixels, width, height, dx, dy );
        text.value = std::move( o );
        text.prepare();
        text.x = area.x + ( w - text.w ) * 0.5;
        text.y = area.y + ( 16 - text.h ) * 0.5;
        text.draw( pixels, width, height, dx, dy );
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

void ActiveTrigger::draw( uint32_t *, int, int, int, int ) const
{}

TextButton::TextButton() : Button(), centerX( true ), centerY( true )
{}

TextButton::TextButton( const TextButton& other ) : Object( other ), Button( other ), centerX( other.centerX ), centerY( other.centerX ), desc( other.desc )
{}

TextButton::~TextButton()
{}

void TextButton::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    if( w <= 0 || h <= 0 )
        return;

    outer( dx, dy );

    auto color = hovered && !off ? makeColor( 220, 220, 60, 255 ) : makeColor( 200, 200, 200, 255 );
    fill( pixels, width, height, color, w, h, dx, dy );

    StaticText text;
    text.color = off ? makeColor( 128, 128, 128, 255 ) : makeColor( 0, 0, 0, 255 );

    text.value = desc;
    text.prepare();

    text.x = centerX ? ( w - text.w ) * 0.5 : 16;
    text.y = centerY ? ( h - text.h ) * 0.5 : 16;

    text.draw( pixels, width, height, dx, dy );

    if( activateByHovering )
    {
        drawLineRD( pixels, width, height, dx + w - 16, dy + 4, 4, text.color );
        drawLineRU( pixels, width, height, dx + w - 16, dy + h - 5, 4, text.color );
    }
}

MinimizeButton::MinimizeButton() : Button()
{}

MinimizeButton::MinimizeButton( const MinimizeButton& other ) : Object( other ), Button( other )
{}

MinimizeButton::~MinimizeButton()
{}

void MinimizeButton::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    outer( dx, dy );

    auto color = hovered ? makeColor( 235, 235, 235, 255 ) : makeColor( 255, 255, 255, 255 );
    fill( pixels, width, height, color, w, h, dx, dy );

    auto black = makeColor( 0, 0, 0, 255 );

    drawLineR( pixels, width, height, dx + 3, dy + h / 2 - 1, w - 6, black );
}

MaximizeButton::MaximizeButton() : Button()
{}

MaximizeButton::MaximizeButton( const MaximizeButton& other ) : Object( other ), Button( other )
{}

MaximizeButton::~MaximizeButton()
{}

void MaximizeButton::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    outer( dx, dy );

    auto color = hovered ? makeColor( 235, 235, 235, 255 ) : makeColor( 255, 255, 255, 255 );
    fill( pixels, width, height, color, w, h, dx, dy );

    auto black = makeColor( 0, 0, 0, 255 );
    auto half = hovered ? makeColor( 188, 188, 188, 255 ) : makeColor( 204, 204, 204, 255 );

    drawLineR( pixels, width, height, dx + 4, dy + 4, w - 8, half );
    drawLineD( pixels, width, height, dx + 4, dy + 4, w - 8, half );
    drawLineR( pixels, width, height, dx + 4, dy + h - 5, w - 8, half );
    drawLineD( pixels, width, height, dx + w - 5, dy + 4, w - 8, half );

    drawLineR( pixels, width, height, dx + 3, dy + 3, w - 6, black );
    drawLineD( pixels, width, height, dx + 3, dy + 3, w - 6, black );
    drawLineR( pixels, width, height, dx + 3, dy + h - 4, w - 6, black );
    drawLineD( pixels, width, height, dx + w - 4, dy + 3, w - 6, black );
}

CloseButton::CloseButton() : Button()
{}

CloseButton::CloseButton( const CloseButton& other ) : Object( other ), Button( other )
{}

CloseButton::~CloseButton()
{}

void CloseButton::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    outer( dx, dy );

    auto color = hovered ? makeColor( 245, 10, 10, 255 ) : makeColor( 255, 255, 255, 255 );
    fill( pixels, width, height, color, w, h, dx, dy );

    auto black = makeColor( 0, 0, 0, 255 );
    auto half = hovered ? makeColor( 196, 8, 8, 255 ) : makeColor( 204, 204, 204, 255 );

    drawLineRD( pixels, width, height, dx + 4, dy + 3, w - 7, half );
    drawLineRU( pixels, width, height, dx + 4, dy + h - 4, w - 7, half );

    drawLineRD( pixels, width, height, dx + 3, dy + 4, w - 7, half );
    drawLineRU( pixels, width, height, dx + 3, dy + h - 5, w - 7, half );

    drawLineRD( pixels, width, height, dx + 3, dy + 3, w - 6, black );
    drawLineRU( pixels, width, height, dx + 3, dy + h - 4, w - 6, black );
}

PlusButton::PlusButton() : Button(), toggle( false )
{
    w = h = 15;
}

PlusButton::PlusButton( const PlusButton& other ) : Object( other ), Button( other ), desc( other.desc )
{}

PlusButton::~PlusButton()
{}

void PlusButton::setDefaultCallback()
{
    use = [this]( bool release )
    {
        if( release )
            toggle = !toggle;
    };
}

void PlusButton::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    outer( dx, dy );

    auto color = hovered && !off ? makeColor( 220, 220, 60, 255 ) : makeColor( 200, 200, 200, 255 );
    fill( pixels, width, height, color, w, h, dx, dy );

    StaticText text;
    text.color = off ? makeColor( 128, 128, 128, 255 ) : makeColor( 0, 0, 0, 255 );

    text.value = desc;
    text.prepare();

    text.x = w + 9;
    text.y = ( h - text.h ) * 0.5;

    text.draw( pixels, width, height, dx, dy );

    drawLineR( pixels, width, height, dx + 2, dy + h / 2, w - 4, makeColor( 0, 0, 0, 255 ) );
    if( !toggle )
        drawLineD( pixels, width, height, dx + w / 2, dy + 2, h - 4, makeColor( 0, 0, 0, 255 ) );
}

DropArea::DropArea()
{}

DropArea::DropArea( const DropArea& other ) : Object( other ), Button( other )
{}

DropArea::~DropArea()
{}

void DropArea::draw( uint32_t *pixels, int width, int height, int dx, int dy ) const
{
    if( hovered )
    {
        outer( dx, dy );
        fill( pixels, width, height, makeColor( 255, 255, 0, 255 ), w, h, dx, dy );
    }
}

static bool isPostfix( const std::vector<size_t>& postfix, const std::vector<size_t>& vector )
{
    if( postfix.size() > vector.size() )
        return false;
    return std::equal( postfix.begin(), postfix.end(), vector.begin() + ( vector.size() - postfix.size() ) );
}

Node::Node( ActionData &d, Node *r, bool f ) : data( d ), root( r ), id( -1 )
{
    using namespace GraphicInterface;

    add( &space );
    add( &button );
    add( &wrapper );

    button.use = [this]( bool release )
    {
        auto path = getPath();

        if( release )
        {
            if( path == data.path )
            {
                data.action = wrapper.visible ? Action::Close : Action::Open;
            }
            else
            {
                data.action = Action::None;
            }
        }
        else
        {
            data.path = std::move( path );
            data.action = Action::None;
        }
    };

    space.x = 0;
    space.y = -6;
    space.w = 128;
    space.h = 4;

    space.use = [this]( bool release )
    {
        if( release && data.path )
        {
            auto path = getPath();
            if( !isPostfix( *data.path, path ) )
            {
                data.secondary = path;
                data.action = Action::Move;
            }
        }
        else
        {
            data.action = Action::None;
        }
    };

    wrapper.visible = button.toggle = f;
    wrapper.y = wrapper.x = height();
}

Node::Node( ActionData &d, const Parameter& parameter, Node *r ) : Node( d, r, parameter.open )
{
    update( parameter );
}

Node::~Node()
{}

void Node::update( const Parameter& parameter )
{
    using namespace GraphicInterface;

    for( auto& node : nodes )
        wrapper.remove( node.get() );
    nodes.clear();

    button.desc = parameter.name;

    for( auto& param : parameter.parameters )
    {
        auto node = std::make_shared<Node>( data, param, this );
        nodes.push_back( node );
        wrapper.add( node.get() );
    }

    auto node = std::make_shared<Node>( data, this, false );
    nodes.push_back( node );
    wrapper.add( node.get() );
    node->button.visible = false;

    reposition();
    recount();
}

void Node::open( bool f )
{
    wrapper.visible = button.toggle = f;
    repositionRecursive();
}

void Node::reposition()
{
    int yOffset = 0;
    for( auto& node : nodes )
    {
        node->y = yOffset;
        yOffset += node->height();
    }
}

void Node::repositionRecursive()
{
    auto r = this;
    while( r )
    {
        r->reposition();
        r = r->root;
    }
}

void Node::recount()
{
    size_t index = 0;
    for( auto& node : nodes )
    {
        node->id = index;
        ++index;
    }
}

int Node::height() const
{
    int result = button.visible ? button.h + 9 : 0;
    if( wrapper.visible )
    {
        result += 8;
        for( auto& node : nodes )
            result += node->height();
    }
    return result;
}

std::vector<size_t> Node::getPath() const
{
    std::vector<size_t> result;
    const Node *r = this, *next = root;
    while( next )
    {
        result.push_back( r->id );
        r = next;
        next = next->root;
    }
    return result;
}

Node *Node::getObject( const std::vector<size_t>& p )
{
    Node *result = this;
    for( auto i = p.rbegin(); i != p.rend(); ++i )
    {
        auto index = *i;
        if( index >= result->nodes.size() )
            return nullptr;

        result = result->nodes[index].get();
    }
    return result;
}

Node *Node::addNode( std::shared_ptr<Node> node )
{
    nodes.push_back( node );
    wrapper.add( node.get() );
    node->root = this;
    recount();
    repositionRecursive();
    return node.get();
}

Node *Node::addNode( std::shared_ptr<Node> node, size_t index )
{
    if( index > nodes.size() )
        return nullptr;

    nodes.insert( nodes.begin() + index, node );
    wrapper.add( node.get() );
    node->root = this;
    recount();
    repositionRecursive();
    return node.get();
}

std::shared_ptr<Node> Node::detach()
{
    if( !root )
        return nullptr;

    auto& rnodes = root->nodes;
    auto result = rnodes[id];

    rnodes.erase( rnodes.begin() + id );
    root->wrapper.remove( result.get() );
    root->recount();
    root->repositionRecursive();
    root = nullptr;

    makeException( result.get() == this );
    return result;
}

ChangedValue<bool> &Keys::letter( char symbol )
{
    makeException( 'A' <= symbol && symbol <= 'Z' );
    return letters[symbol - 'A'];
}

const ChangedValue<bool> &Keys::letter( char symbol ) const
{
    makeException( 'A' <= symbol && symbol <= 'Z' );
    return letters[symbol - 'A'];
}

ChangedValue<bool> &Keys::digit( unsigned short symbol )
{
    makeException( 0 <= symbol && symbol <= 9 );
    return letters[symbol];
}

const ChangedValue<bool> &Keys::digit( unsigned short symbol ) const
{
    makeException( 0 <= symbol && symbol <= 9 );
    return letters[symbol];
}

void Keys::reset()
{
    for( auto &letter : letters )
        letter.reset();
    for( auto &digit : digits )
        digit.reset();
}

void Keys::release()
{
    for( auto &letter : letters )
        letter = false;
    for( auto &digit : digits )
        digit = false;
}

OutputData::OutputData( GraphicInterface::Window &desc ) : image( desc.content )
{}

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
    add( &mouseTrigger );
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

    icon.y = borderWidth + ( icon.h > 0 ? ( titlebarHeight - icon.h ) / 2 : 0 );
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

    mouseTrigger.place( content );
}

void Window::run()
{
    GenericWindow g( *this );
    g.run();
}

uint32_t makeColor( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
    return ( a << 24 ) | ( r << 16 ) | ( g << 8 ) | b;
}

uint8_t getR( uint32_t color )
{
    return ( color >> 16 ) & 0xff;
}

uint8_t getG( uint32_t color )
{
    return ( color >> 8 ) & 0xff;
}

uint8_t getB( uint32_t color )
{
    return color & 0xff;
}

uint8_t getA( uint32_t color )
{
    return ( color >> 24 ) & 0xff;
}
}

Settings::Settings( std::wstring tl, const Parameters& parameters )
{
    using namespace GraphicInterface;

    std::vector<std::shared_ptr<Combobox>> comboboxes;

    auto make = [this, &comboboxes]( const Settings::Parameter & value, int position, int width, int height )
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
            object->prepare();
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
    title.prepare();
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

Settings::~Settings()
{}

void Settings::update()
{
    Window::update();

    for( auto& field : fields )
    {
        field->w = self.w - 2 * borderWidth - 2 * 16;
        field->x = borderWidth + 16;
    }
}

Popup::Popup( Type t, std::wstring tl, std::wstring inf ) : type( t )
{
    using namespace GraphicInterface;

    self.w = 320;
    self.h = 240;

    title.value = tl;
    title.prepare();

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
    info.prepare();
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

    info.prepare();
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

static std::shared_ptr<GraphicInterface::ActiveGroup> sidedrop(
    ContextMenu& root, const ContextMenu::Parameters& parameters, int& x, int& y, GraphicInterface::ActiveGroup *rootMenu )
{
    using namespace GraphicInterface;

    int width = 256, maxX = x, maxY = y;

    auto menu = std::make_shared<ActiveGroup>();
    root.storage.push_back( menu );

    auto trigger = std::make_shared<ActiveTrigger>();
    root.storage.push_back( trigger );
    menu->add( trigger.get() );

    trigger->x = x - 1;
    trigger->y = y;
    trigger->activateByHovering = true;

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
            root.storage.push_back( separator );
            menu->add( separator.get() );
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
            button->use = [&root, callback = p.callback]( bool release )
            {
                if( release )
                {
                    callback();
                    root.closeButton.use( true );
                }
            };
        }

        if( p.active && !p.parameters.empty() )
        {
            int nextX = x + width + 1, nextY = y;
            auto subMenu = sidedrop( root, p.parameters, nextX, nextY, menu.get() );
            menu->add( subMenu.get() );
            subMenu->visible = false;

            if( maxX < nextX )
                maxX = nextX;

            if( maxY < nextY )
                maxY = nextY;

            // Sub-menu
            button->activateByHovering = true;
            button->use = [next = subMenu.get()]( bool inside )
            {
                if( inside )
                {
                    next->visible = true;
                }
                else if( !next->hovered )
                {
                    next->visible = false;
                }
            };
        }

        button->off = button->off && button->use;

        y += button->h;
        root.storage.push_back( button );
        menu->add( button.get() );
    }

    trigger->w = width + 1;
    trigger->h = y - trigger->y;
    trigger->use = [&root, rootMenu, self = menu.get()]( bool inside )
    {
        if( inside )
        {
            if( rootMenu )
                rootMenu->visible = true;
        }
        else
        {
            self->visible = false;
        }
    };

    if( x < maxX )
        x = maxX;

    if( y < maxY )
        y = maxY;

    return menu;
}

ContextMenu::ContextMenu( const Parameters& parameters )
{
    int xOffset = 0, yOffset = 0;
    add( sidedrop( *this, parameters, xOffset, yOffset, nullptr ).get() );
}

ContextMenu::~ContextMenu()
{}

bool ContextMenu::hover( int x0, int y0 )
{
    auto result = ActiveGroup::hover( x0, y0 );
    if( !hovered )
    {
        closeButton.use( true );
        return false;
    }
    return result;
}

void ContextMenu::update()
{}

void ContextMenu::run()
{
    if( storage.empty() )
        return;

    self.w = GetSystemMetrics( SM_CXSCREEN );
    self.h = GetSystemMetrics( SM_CYSCREEN );

    client.place( self );
    client.color = GraphicInterface::makeColor( 0, 0, 0, 1 );

    POINT p;
    if( !GetCursorPos( &p ) )
        return;

    storage[0]->visible = true;
    storage[0]->x = p.x;
    storage[0]->y = p.y;

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

Hierarchy::Hierarchy( const GraphicInterface::Node::Parameter& p ) : root( data, p )
{
    self.w = 256;
    self.h = 256;

    add( &root );
    root.space.visible = false;
}

Hierarchy::~Hierarchy()
{}

bool Hierarchy::click( bool release, int x0, int y0 )
{
    if( !release )
        return ActiveGroup::click( false, x0, y0 );

    auto result = ActiveGroup::click( true, x0, y0 );

    if( !callback || callback( data ) )
    {
        GraphicInterface::Node *primary = nullptr, *secondary = nullptr;

        if( data.path )
            primary = root.getObject( *data.path );

        if( data.secondary )
            secondary = root.getObject( *data.secondary );

        if( data.action == GraphicInterface::Node::Action::Move )
        {
            auto node = primary->detach();
            secondary->root->addNode( node, secondary->id );
        }
        else if( data.action == GraphicInterface::Node::Action::Open )
        {
            primary->open( true );
        }
        else if( data.action == GraphicInterface::Node::Action::Close )
        {
            primary->open( false );
        }

        data.action = GraphicInterface::Node::Action::None;
    }

    return result;
}

void Hierarchy::update()
{
    Window::update();
    root.x = client.x + 8;
    root.y = client.y + 8;
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

    desc.self.w = width;
    desc.self.h = height;
    desc.update();

    auto *pixels = ( uint32_t * )pBits;
    desc.draw( pixels, width, height, -desc.x, -desc.y );

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

std::vector<GenericWindow::Data> GenericWindow::stack;

GenericWindow::GenericWindow( GraphicInterface::Window &d ) : outputData( d ), desc( d )
{
    implementation = nullptr;

    // This code is positioned in lambda to accesses private members of GenericWindow
    auto windowProc = []( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) -> LRESULT
    {
        Implementation *impl;
        GenericWindow *window;
        GraphicInterface::Window *dsc;

        auto getData = [hwnd, &impl, &window, &dsc]()
        {
            impl = ( Implementation * )GetWindowLongPtr( hwnd, GWLP_USERDATA );
            if( impl )
            {
                window = impl->window;
                dsc = &window->desc;
            }
            else
            {
                window = nullptr;
                dsc = nullptr;
            }
        };

        getData();

        auto inputReset = [&window]()
        {
            window->inputReset();
        };

        auto inputRelease = [&window]()
        {
            window->inputRelease();
        };

        auto quit = [hwnd]()
        {
            DestroyWindow( hwnd );
        };

        auto getPos = [hwnd]( int &x, int &y )
        {
            POINT point;
            GetCursorPos( &point );
            x = point.x;
            y = point.y;
        };

        auto handle = [&]()
        {
            try
            {
                if( dsc->handleMsg )
                    dsc->handleMsg( window->inputData, window->outputData );
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

            if( window->outputData.quit )
            {
                quit();
                return;
            }

            inputReset();

            auto &img = window->outputData.image;
            if( img.changed() )
            {
                updateWindowContent( *dsc, hwnd );
                img.reset();
            }

            window->inputData.init = false;
        };

        switch( message )
        {
        case WM_CREATE:
            SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )( ( LPCREATESTRUCT )lParam )->lpCreateParams );
            getData();
            window->inputData.init = true;
            handle();
            break;
        case WM_DESTROY:
            impl->hwnd = nullptr;
            SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )nullptr );
            getData();
            // PostQuitMessage( 0 );
            return 0;
        case WM_APP:
            break;
        case WM_NCHITTEST:
            {
                int x = LOWORD( lParam ), y = HIWORD( lParam );
                int x1 = x, y1 = y;

                dsc->inner( x, y );

                bool left   = dsc->leftTrigger.contains( x, y );
                bool right  = dsc->rightTrigger.contains( x, y );
                bool top    = dsc->topTrigger.contains( x, y );
                bool bottom = dsc->bottomTrigger.contains( x, y );

                if( top && left ) return HTTOPLEFT;
                if( top && right ) return HTTOPRIGHT;
                if( bottom && left ) return HTBOTTOMLEFT;
                if( bottom && right ) return HTBOTTOMRIGHT;

                if( left ) return HTLEFT;
                if( right ) return HTRIGHT;
                if( top ) return HTTOP;
                if( bottom ) return HTBOTTOM;

                if( dsc->activeContains( x1, y1 ) )
                    return HTCLIENT;

                if( dsc->titleBar.contains( x, y ) )
                    return HTCAPTION;

                return HTCLIENT;
            }
        case WM_GETMINMAXINFO:
            {
                // Can be launched before WM_CREATE
                if( dsc )
                {
                    MINMAXINFO *p = ( MINMAXINFO * )lParam;
                    p->ptMinTrackSize.x = dsc->minWidth();
                    p->ptMinTrackSize.y = dsc->minHeight();
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
                updateWindowContent( *dsc, hwnd );
                break;
            }
        case WM_MOVE:
            {
                RECT rect;
                GetWindowRect( hwnd, &rect );
                dsc->x = rect.left;
                dsc->y = rect.top;
                break;
            }
        case WM_PAINT:
            break;
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
            {
                int x, y;
                getPos( x, y );

                dsc->hover( x, y );

                dsc->inner( x, y );
                if( dsc->mouseTrigger.contains( x, y ) )
                {
                    dsc->mouseTrigger.inner( x, y );
                    impl->window->inputData.mouseX = x;
                    impl->window->inputData.mouseY = y;
                    handle();
                }

                getData();
                if( dsc )
                    updateWindowContent( *dsc, hwnd );
            }
            return 0;
        case WM_LBUTTONDOWN:
            {
                int x, y;
                getPos( x, y );

                dsc->click( false, x, y );

                dsc->inner( x, y );
                if( dsc->mouseTrigger.contains( x, y ) )
                {
                    window->inputData.leftMouse = true;
                    handle();
                }

                getData();
                if( dsc )
                    updateWindowContent( *dsc, hwnd );
            }
            return 0;
        case WM_LBUTTONUP:
            {
                int x, y;
                getPos( x, y );

                dsc->click( true, x, y );

                dsc->inner( x, y );
                if( dsc->mouseTrigger.contains( x, y ) )
                {
                    window->inputData.leftMouse = false;
                    handle();
                }

                getData();
                if( dsc )
                    updateWindowContent( *dsc, hwnd );
            }
            return 0;
        case WM_RBUTTONDOWN:
            {
                int x, y;
                getPos( x, y );
                dsc->inner( x, y );
                if( dsc->mouseTrigger.contains( x, y ) )
                {
                    window->inputData.rightMouse = true;
                    handle();
                }
            }
            return 0;
        case WM_RBUTTONUP:
            {
                int x, y;
                getPos( x, y );
                dsc->inner( x, y );
                if( dsc->mouseTrigger.contains( x, y ) )
                {
                    window->inputData.rightMouse = false;
                    handle();
                }
            }
            return 0;
        case WM_MBUTTONDOWN:
            {
                int x, y;
                getPos( x, y );
                dsc->inner( x, y );
                if( dsc->mouseTrigger.contains( x, y ) )
                {
                    window->inputData.middleMouse = true;
                    handle();
                }
            }
            return 0;
        case WM_MBUTTONUP:
            {
                int x, y;
                getPos( x, y );
                dsc->inner( x, y );
                if( dsc->mouseTrigger.contains( x, y ) )
                {
                    impl->window->inputData.middleMouse = false;
                    handle();
                }
            }
            return 0;
        case WM_CHAR:
            {
                auto process = [&]( auto value )
                {
                    if( dsc->input( value ) )
                        dsc->focus( true );
                    updateWindowContent( *dsc, hwnd );
                };

                if( wParam == '-' || wParam == '/' || wParam == '\\' || wParam == '+' || wParam == '.' || wParam == '_' || wParam == '\b' || wParam == ' ' )
                {
                    process( wParam );
                    return 0;
                }

                if( ( '0' <= wParam && wParam <= '9' ) || ( 'a' <= wParam && wParam <= 'z' ) || ( 'A' <= wParam && wParam <= 'Z' ) )
                {
                    process( wParam );
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
            auto &input = window->inputData;
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
                    auto &key = window->inputData.keys.letter( wParam );
                    key = pressed;
                    handle();
                    return 0;
                }

                if( '0' <= wParam && wParam <= '9' )
                {
                    auto &key = window->inputData.keys.digit( wParam - '0' );
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

void GenericWindow::run( bool lock )
{
    auto lastWindow = GetForegroundWindow();

    if( desc.x < 0 || desc.y < 0 || desc.self.w < desc.minWidth() || desc.self.h < desc.minHeight() )
    {
        RECT screenRect;
        GetClientRect( GetDesktopWindow(), &screenRect );

        int screenWidth = screenRect.right - screenRect.left;
        int screenHeight = screenRect.bottom - screenRect.top;

        float scalar = 0.65f;
        int width = Round( screenWidth * scalar );
        int height = Round( screenHeight * scalar );

        scalar = 0.0625f;
        float shiftX = ( screenWidth - width ) * scalar;
        float shiftY = ( screenHeight - height ) * scalar;

        desc.x = desc.y = Round( Min( shiftX, shiftY ) );
        desc.self.w = width;
        desc.self.h = height;
    }

    stack.push_back( {&desc, lock} );

    auto hwnd = implementation->hwnd = CreateWindowExW( WS_EX_LAYERED,
                                       implementation->className.c_str(), desc.title.value.c_str(),
                                       WS_POPUP | WS_THICKFRAME | WS_VISIBLE,
                                       desc.x, desc.y, desc.self.w, desc.self.h,
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

    stack.pop_back();

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
