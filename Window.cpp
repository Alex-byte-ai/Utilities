#include "Window.h"

#include <windows.h>
#include <commctrl.h>

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

bool Box::inside( int x0, int y0 ) const
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

void Trigger::draw( uint32_t *, int, int ) const
{}

void Rectangle::draw( uint32_t *pixels, int width, int height ) const
{
    Box::fill( pixels, width, height, color );
}

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

void StaticText::prepare( uint32_t background )
{
    w = 0;
    h = 0;
    if( renderTextToBuffer( value, L"DejaVuSansMono", makeColor( 0, 0, 0, 255 ), background, 0, bufferW, bufferH, pixels ) )
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

void DynamicText::prepare( bool write )
{
    auto idleColor = makeColor( 255, 255, 255, 255 );
    auto errorColor = makeColor( 255, 127, 127, 255 );
    auto focusColor = makeColor( 255, 255, 127, 255 );

    if( write )
        valid = setCallback ? setCallback( value ) : false;

    auto color = valid ? ( focused ? focusColor : idleColor ) : errorColor;
    StaticText::prepare( color );
}

void DynamicText::draw( uint32_t *canvas, int width, int height ) const
{
    if( !pixels.empty() )
        Box::fill( canvas, width, height, pixels[0] );
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

void Combobox::open( bool f )
{
    if( !f && isOpen && setCallback )
        setCallback( options[option] );

    isOpen = f;
    h = isOpen ? 16 * options.size() : 16;
}

size_t Combobox::select( int x0, int y0 )
{
    if( !inside( x0, y0 ) )
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
    if( !inside( x0, y0 ) )
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

bool Button::hover( int x0, int y0 )
{
    hovered = inside( x0, y0 );
    return false;
}

bool Button::click( bool release, int, int )
{
    if( release && use )
        use();
    return false;
}

bool Button::input( wchar_t )
{
    return false;
}

void Button::focus( bool )
{}

void TextButton::draw( uint32_t *pixels, int width, int height ) const
{
    auto color = hovered ? makeColor( 220, 220, 60, 255 ) : makeColor( 200, 200, 200, 255 );
    Box::fill( pixels, width, height, color );

    StaticText text;
    text.value = desc;
    text.prepare( color );

    text.x = x + ( w - text.w ) * 0.5;
    text.y = y + ( h - text.h ) * 0.5;

    text.draw( pixels, width, height );
}

void MinimizeButton::draw( uint32_t *pixels, int width, int height ) const
{
    auto color = hovered ? makeColor( 235, 235, 235, 255 ) : makeColor( 255, 255, 255, 255 );
    Box::fill( pixels, width, height, color );

    auto black = makeColor( 0, 0, 0, 255 );

    drawLineR( pixels, width, height, x + 3, y + h / 2 - 1, w - 6, black );
}

void MaximizeButton::draw( uint32_t *pixels, int width, int height ) const
{
    auto color = hovered ? makeColor( 235, 235, 235, 255 ) : makeColor( 255, 255, 255, 255 );
    Box::fill( pixels, width, height, color );

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

void CloseButton::draw( uint32_t *pixels, int width, int height ) const
{
    auto color = hovered ? makeColor( 245, 10, 10, 255 ) : makeColor( 255, 255, 255, 255 );
    Box::fill( pixels, width, height, color );

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

    add( &client );
    add( &content );
    add( &titleBar );
    add( &icon );
    add( &titleOrig );
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
    titleBar.x = x;
    titleBar.y = y;
    titleBar.w = w;
    titleBar.h = titlebarHeight + borderWidth;

    rightTrigger.x = x + w - triggerWidth;
    rightTrigger.y = y;
    rightTrigger.w = triggerWidth;
    rightTrigger.h = h;

    bottomTrigger.x = x;
    bottomTrigger.y = y + h - triggerWidth;
    bottomTrigger.w = w;
    bottomTrigger.h = triggerWidth;

    leftBorder.x = x;
    leftBorder.y = y;
    leftBorder.w = borderWidth;
    leftBorder.h = h;

    rightBorder.x = x + w - borderWidth;
    rightBorder.y = y;
    rightBorder.w = borderWidth;
    rightBorder.h = h;

    topBorder.x = x;
    topBorder.y = y;
    topBorder.w = w;
    topBorder.h = borderWidth;

    bottomBorder.x = x;
    bottomBorder.y = y + h - borderWidth;
    bottomBorder.w = w;
    bottomBorder.h = borderWidth;

    closeButton.x = x + w - buttonSpacingV - buttonSize - borderWidth;
    closeButton.y = y + buttonSpacingV + borderWidth;
    closeButton.w = buttonSize;
    closeButton.h = buttonSize;

    maximizeButton.place( closeButton );
    maximizeButton.x -= buttonSpacingH + buttonSize;

    minimizeButton.place( maximizeButton );
    minimizeButton.x -= buttonSpacingH + buttonSize;

    icon.y = borderWidth + ( titlebarHeight - icon.h ) / 2;
    icon.x = icon.y;

    titleOrig.x = icon.x + icon.w + buttonSpacingV;
    titleOrig.y = closeButton.y;

    titleOrig.w = w - borderWidth - 2 * buttonSpacingV - 3 * buttonSize - 2 * buttonSpacingH - titleOrig.x;
    if( titleOrig.w < titleOrig.bufferW )
        titleOrig.w = 0;

    client.x = x + borderWidth;
    client.y = y + borderWidth + titlebarHeight;
    client.w = w - 2 * borderWidth;
    client.h = h - titlebarHeight - 2 * borderWidth;

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

void Window::draw( uint32_t *pixels, int width, int height ) const
{
    for( auto object : objects )
        object->draw( pixels, width, height );
}

void Window::add( Box *object )
{
    objects.push_back( object );
    if( auto active = dynamic_cast<Active*>( object ) )
        interactive.push_back( active );
}

void Window::run()
{
    auto lastWindow = GetForegroundWindow();

    GenericWindow self( *this, nullptr );
    self.run();

    DWORD windowProcessId = 0;
    GetWindowThreadProcessId( lastWindow, &windowProcessId );
    if( windowProcessId == GetCurrentProcessId() )
        SetForegroundWindow( lastWindow );
}

uint32_t makeColor( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
    return ( a << 24 ) | ( r << 16 ) | ( g << 8 ) | b;
}
}

Settings::Settings( std::wstring tl, Parameters& p ) : parameters( p )
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

            object->use = [s = value.set]()
            {
                s( L"" );
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

    w = 300;
    h = 1024;
    titleOrig.value = tl;
    titleOrig.prepare( titleBar.color );
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

void Settings::update()
{
    Window::update();

    for( auto& field : fields )
    {
        field->w = w - 2 * borderWidth - 2 * 16;
        field->x = x + borderWidth + 16;
    }
}

Settings::~Settings()
{}

Popup::Popup( Type type, std::wstring tl, std::wstring inf )
{
    using namespace GraphicInterface;

    Window::update();

    titleOrig.value = tl;
    titleOrig.prepare( titleBar.color );

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

    info.value = inf;
    info.prepare( client.color );

    add( &info );

    if( type == Type::Question )
    {
        buttons.push_back( &yes );
        buttons.push_back( &no );

        yes.desc = L"yes";
        no.desc = L"no";

        yes.use = [this]()
        {
            answer = true;
            closeButton.use();
        };

        no.use = [this]()
        {
            answer = false;
            closeButton.use();
        };
    }
    else
    {
        buttons.push_back( &cancel );

        cancel.desc = L"ok";

        cancel.use = [this]()
        {
            closeButton.use();
        };
    }

    for( auto button : buttons )
        add( button );
}

void Popup::update()
{
    w = 320;
    h = 240;

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


class ContextMenu::Implementation
{
public:
    Parameters& parameters;
    ATOM windowClass;
    WNDCLASSW wc;
    HWND menu;

    std::vector<std::tuple<int, std::function<void()>>> instances;
    std::vector<HMENU> menus;

    Implementation( WNDPROC windowProc, Parameters& p ) : parameters( p )
    {
        memset( &wc, 0, sizeof( wc ) );
        wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleW( nullptr );
        wc.lpszClassName = L"DropDownMenuHolder";
        wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
        windowClass = RegisterClassW( &wc );
        menu = nullptr;
    }

    ~Implementation()
    {
        if( windowClass )
            UnregisterClassW( wc.lpszClassName, GetModuleHandleW( nullptr ) );
    }
};

static HMENU dropDown( ContextMenu::Implementation& impl, const ContextMenu::Parameters& parameters )
{
    static int index = 1001;

    if( parameters.empty() )
        return nullptr;

    HMENU popupMenu = CreatePopupMenu(), subMenu;
    for( auto& p : parameters )
    {
        if( p.name.empty() )
        {
            AppendMenuW( popupMenu, MF_SEPARATOR, 0, nullptr );
            continue;
        }

        if( p.active && ( subMenu = dropDown( impl, p.parameters ) ) )
        {
            AppendMenuW( popupMenu, MF_POPUP, ( UINT_PTR )subMenu, p.name.c_str() );
            continue;
        }

        AppendMenuW( popupMenu, p.active ? MF_STRING : MF_GRAYED, ( UINT_PTR )index, p.name.c_str() );
        impl.instances.emplace_back( index, p.callback );
        ++index;
    }

    impl.menus.emplace_back( popupMenu );
    return popupMenu;
}

ContextMenu::ContextMenu( Parameters p ) : parameters( std::move( p ) )
{
    auto windowProc = []( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) -> LRESULT
    {
        auto impl = ( Implementation * )GetWindowLongPtr( hwnd, GWLP_USERDATA );

        switch( message )
        {
        case WM_CREATE:
            {
                impl = ( Implementation * )( ( LPCREATESTRUCT )lParam )->lpCreateParams;
                SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )impl );
                dropDown( *impl, impl->parameters );
                return 0;
            }
        case WM_APP_SHOWMENU:
            {
                if( impl->menus.empty() )
                    break;

                POINT pt;
                if( lParam == -1 ) GetCursorPos( &pt );
                else
                {
                    pt.x = LOWORD( lParam );
                    pt.y = HIWORD( lParam );
                }

                ShowWindow( hwnd, SW_SHOWNOACTIVATE );
                SetWindowPos( hwnd, HWND_TOPMOST, pt.x, pt.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE );
                SetForegroundWindow( hwnd );

                auto tracked = TrackPopupMenuEx( impl->menus.back(), TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, hwnd, nullptr );
                for( auto& [index, callback] : impl->instances )
                {
                    if( callback && tracked == index )
                        callback();
                }
                return 0;
            }
        case WM_EXITMENULOOP:
            {
                DestroyWindow( hwnd );
                return 0;
            }
        case WM_DESTROY:
            for( auto menu : impl->menus )
            {
                DestroyMenu( menu );
            }
            impl->menus.clear();
            impl->menu = nullptr;
            impl = nullptr;
            // PostQuitMessage( 0 );
            return 0;
        default:
            break;
        }
        return DefWindowProcW( hwnd, message, wParam, lParam );
    };

    implementation = new Implementation( windowProc, parameters );
}

ContextMenu::~ContextMenu()
{
    delete implementation;
}

void ContextMenu::run()
{
    POINT point;
    GetCursorPos( &point );

    auto menu = implementation->menu = CreateWindowExW( WS_EX_TOOLWINDOW | WS_EX_TOPMOST, implementation->wc.lpszClassName, L"",
                                       WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 8, 8, nullptr, nullptr, implementation->wc.hInstance, implementation );
    makeException( menu );

    LPARAM l = MAKELPARAM( point.x, point.y );
    PostMessageW( menu, WM_APP_SHOWMENU, 0, l );

    MSG msg;
    BOOL result;
    while( implementation->menu && ( result = GetMessage( &msg, nullptr, 0, 0 ) ) != 0 )
    {
        makeException( result != -1 );
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }
}

static std::optional<std::filesystem::path> GetPathToFile( bool open )
{
    Finalizer _;

    wchar_t path[1024], current[1024];
    OPENFILENAMEW ofn;

    memset( path, 0, sizeof( path ) );
    memset( current, 0, sizeof( current ) );
    memset( &ofn, 0, sizeof( ofn ) );

    GetCurrentDirectoryW( sizeof( current ), current );

    std::wstring currentPath = current;
    _.push( [currentPath]()
    {
        SetCurrentDirectoryW( currentPath.c_str() );
    } );

    ofn.lStructSize = sizeof( ofn );
    ofn.lpstrFile = path;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof( path );
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if( open )
    {
        if( GetOpenFileNameW( &ofn ) )
            return path;
    }
    else
    {
        if( GetSaveFileNameW( &ofn ) )
            return path;
    }

    return {};
}

std::optional<std::filesystem::path> SavePath()
{
    return GetPathToFile( false );
}

std::optional<std::filesystem::path> OpenPath()
{
    return GetPathToFile( true );
}

static void updateWindowContent( GraphicInterface::Window &desc, HWND hwnd )
{
    RECT rect;
    GetWindowRect( hwnd, &rect );

    auto oldX = desc.x;
    auto oldY = desc.y;

    desc.x = 0;
    desc.y = 0;
    desc.w = rect.right - rect.left;
    desc.h = rect.bottom - rect.top;
    desc.update();

    BITMAPINFO bmi;
    clear( &bmi, sizeof( bmi ) );
    bmi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
    bmi.bmiHeader.biWidth       = desc.w;
    bmi.bmiHeader.biHeight      = -desc.h;
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

    auto *pixels = ( uint32_t * )pBits;
    desc.draw( pixels, desc.w, desc.h );

    BLENDFUNCTION blend;
    clear( &blend, sizeof( blend ) );
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255; // Use per-pixel alpha.
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT ptZero = {0, 0};
    SIZE sizeWindow = {desc.w, desc.h};
    UpdateLayeredWindow( hwnd, hdcScreen, nullptr, &sizeWindow, hdcMem, &ptZero, 0, &blend, ULW_ALPHA );

    SelectObject( hdcMem, hOldBmp );
    DeleteObject( hBitmap );
    DeleteDC( hdcMem );
    ReleaseDC( nullptr, hdcScreen );

    desc.x = oldX;
    desc.y = oldY;
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

        auto focus = [impl]( GraphicInterface::Active * active, bool refocus )
        {
            if( !refocus )
                return;

            auto& target = impl->window->desc.focus;
            if( target )
                target->focus( false );
            target = active;
            target->focus( true );
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

                bool left   = impl->window->desc.leftTrigger.inside( x, y );
                bool right  = impl->window->desc.rightTrigger.inside( x, y );
                bool top    = impl->window->desc.topTrigger.inside( x, y );
                bool bottom = impl->window->desc.bottomTrigger.inside( x, y );

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
                    if( active->inside( x, y ) )
                        return HTCLIENT;
                }

                if( impl->window->desc.titleBar.inside( x, y ) )
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
                impl->window->desc.x = rect.left;
                impl->window->desc.y = rect.top;
                break;
            }
        case WM_PAINT:
            break;
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
            {
                int x, y;
                getPos( x, y );

                for( auto& active : impl->window->desc.interactive )
                    focus( active, active->hover( x, y ) );

                if( impl->window->desc.content.inside( x, y ) )
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

                auto& list = impl->window->desc.interactive;
                for( auto i = list.rbegin(); i != list.rend(); ++i )
                {
                    auto& active = * i;
                    if( active->inside( x, y ) )
                    {
                        focus( active, active->click( false, x, y ) );
                        break;
                    }
                }

                if( impl->window->desc.content.inside( x, y ) )
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

                auto& list = impl->window->desc.interactive;
                for( auto i = list.rbegin(); i != list.rend(); ++i )
                {
                    auto& active = * i;
                    if( active->inside( x, y ) )
                    {
                        focus( active, active->click( true, x, y ) );
                        break;
                    }
                }

                if( impl->window->desc.content.inside( x, y ) )
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
                if( impl->window->desc.content.inside( x, y ) )
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
                if( impl->window->desc.content.inside( x, y ) )
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
                if( impl->window->desc.content.inside( x, y ) )
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
                if( impl->window->desc.content.inside( x, y ) )
                {
                    impl->window->inputData.middleMouse = false;
                    handle();
                }
            }
            return 0;
        case WM_CHAR:
            {
                auto textInput = [&]( wchar_t c )
                {
                    auto target = impl->window->desc.focus;
                    if( target )
                    {
                        target->input( c );
                        updateWindowContent( impl->window->desc, hwnd );
                    }
                };

                if( wParam == '-' || wParam == '+' || wParam == '.' || wParam == '_' || wParam == '\b' )
                {
                    textInput( wParam );
                    return 0;
                }

                if( ( '0' <= wParam && wParam <= '9' ) || ( 'a' <= wParam && wParam <= 'z' ) || ( 'A' <= wParam && wParam <= 'Z' ) )
                {
                    textInput( wParam );
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

    desc.closeButton.use = [this]()
    {
        close();
    };

    desc.maximizeButton.use = [this]()
    {
        maximize();
    };

    desc.minimizeButton.use = [this]()
    {
        minimize();
    };
}

GenericWindow::~GenericWindow()
{
    delete implementation;
}

void GenericWindow::run()
{
    if( desc.x <= 0 || desc.y <= 0 )
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
        desc.x = desc.y = Min( shiftX, shiftY );
    }

    auto hwnd = implementation->hwnd = CreateWindowExW( WS_EX_LAYERED,
                                       implementation->className.c_str(), desc.titleOrig.value.c_str(),
                                       WS_POPUP | WS_THICKFRAME | WS_VISIBLE,
                                       desc.x, desc.y, desc.w, desc.h,
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
