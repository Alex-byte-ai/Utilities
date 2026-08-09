#include "Window.h"

#include <windows.h>
#include <dwmapi.h>

#include <algorithm>
#include <cwctype>
#include <memory>

#include "UnicodeString.h"
#include "Exception.h"
#include "Lambda.h"
#include "Basic.h"

class GenericWindow
{
public:
    GenericWindow( GraphicInterface::Window &desc, bool lock );
    ~GenericWindow();

    // Creates and runs an interactive window
    // Returns true, on an initial call, that sets up window system and hangs in message loop
    // Windows created before call with 'lock' = true, will stop responding
    static bool create( GraphicInterface::Window &desc, bool lock = true );

    // Request removal of this window
    void close();

    void maximize();
    void minimize();

    // Returns true, if a message was processed
    bool handle();

    // Focuses this window
    void focus();

    static void update();

    static size_t count();
private:
    void inputReset();
    void inputRelease();

    void createTab();
    void releaseData();

    HBITMAP imageScale( int width, int height );
    HBITMAP image();

    static void setupBitmap( int width, int height, HBITMAP& bitmap, uint32_t*& pixels );

    static void killFocus();
    static void setFocus();

    static bool focus( int x, int y );

    // Processes an action for a focused window
    static bool process( const std::function<bool( GenericWindow& )> & action );

    // Removes windows, that requested removal
    static void cleanup();

    GraphicInterface::InputData inputData;
    GraphicInterface::OutputData outputData;
    GraphicInterface::Window& desc;

    int originalX, originalY, originalW, originalH;
    bool maximized;

    std::optional<Popup> popup;

    HBITMAP original, preview;
    HWND proxy;
    bool lock;

    static std::vector<std::shared_ptr<GenericWindow>> stack;
    static GenericWindow *active;
    static bool needCleanup;
    static HWND hndwnd;
};

namespace GraphicInterface
{

ConstCanvas::ConstCanvas() : pixels( nullptr ), width( 0 ), height( 0 ), stride( 0 )
{}

ConstCanvas::ConstCanvas( const uint32_t *p, int w, int h, int s ) : pixels( p ), width( w ), height( h ), stride( s )
{}

const uint32_t *ConstCanvas::pixel( int x, int y ) const
{
    return pixels + ( y * stride + x );
}

Canvas::Canvas() : pixels( nullptr ), width( 0 ), height( 0 ), stride( 0 )
{}

Canvas::Canvas( uint32_t *p, int w, int h, int s ) : pixels( p ), width( w ), height( h ), stride( s )
{}

uint32_t *Canvas::pixel( int x, int y )
{
    return pixels + ( y * stride + x );
}

const uint32_t *Canvas::pixel( int x, int y ) const
{
    return pixels + ( y * stride + x );
}

void Canvas::stain()
{
    for( int j = 0; j < height; ++j )
    {
        for( int i = 0; i < width; ++i )
        {
            auto& p = *pixel( i, j );
            if( getA( p ) == 0 )
                p = makeColor( 0, 0, 0, 1 );
        }
    }
}

void Canvas::fill( uint32_t color )
{
    for( int j = 0; j < height; ++j )
    {
        for( int i = 0; i < width; ++i )
        {
            *pixel( i, j ) = color;
        }
    }
}

void Canvas::diamond( uint32_t inner, uint32_t outer )
{
    auto inR = getR( inner );
    auto inG = getG( inner );
    auto inB = getB( inner );
    auto inA = getA( inner );

    auto outR = getR( outer );
    auto outG = getG( outer );
    auto outB = getB( outer );
    auto outA = getA( outer );

    for( int j = 0; j < height; ++j )
    {
        for( int i = 0; i < width; ++i )
        {
            float u = ( i + 0.5 ) / width - 0.5;
            float v = ( j + 0.5 ) / height - 0.5;
            float k = Abs( u - v ) + Abs( u + v );
            if( k < 0 || k > 1 )
            {
                *pixel( i, j ) = makeColor( 255, 0, 255, 255 );
                continue;
            }

            float r = Round( ( outR - inR ) * k + inR );
            float g = Round( ( outG - inG ) * k + inG );
            float b = Round( ( outB - inB ) * k + inB );
            float a = Round( ( outA - inA ) * k + inA );
            *pixel( i, j ) = makeColor( r, g, b, a );
        }
    }
}

void Canvas::draw( const ConstCanvas& other, int skipX, int skipY )
{
    auto line = width * sizeof( pixels[0] );
    auto otherPointer = other.pixel( skipX, skipY );
    auto pointer = pixels;

    for( int j = 0; j < height; ++j )
    {
        copy( pointer, otherPointer, line );
        otherPointer += other.stride;
        pointer += stride;
    }
}

void Canvas::drawBlend( const ConstCanvas& other, int skipX, int skipY )
{
    auto otherPointer = other.pixel( skipX, skipY );
    auto pointer = pixels;

    for( int j = 0; j < height; ++j )
    {
        for( int i = 0; i < width; ++i )
        {
            auto& result = pointer[i];
            auto& sample = otherPointer[i];

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
        otherPointer += other.stride;
        pointer += stride;
    }
}

void Canvas::drawLineR( int x, int y, int size, uint32_t color )
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
        *pixel( x, y ) = color;
}

void Canvas::drawLineD( int x, int y, int size, uint32_t color )
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
        *pixel( x, y ) = color;
}

void Canvas::drawLineRD( int x, int y, int size, uint32_t color )
{
    int b = y - x;

    int y0 = y;
    int y1 = y + size;

    int x0 = x;
    int x1 = x + size;

    if( x0 < 0 )
        x0 = 0;
    if( x1 > width )
        x1 = width;

    if( y0 < 0 )
        y0 = 0;
    if( y1 > height )
        y1 = height;

    y0 -= b;
    y1 -= b;

    x0 = Max( x0, y0 );
    x1 = Min( x1, y1 );

    x = x0;
    while( x < x1 )
    {
        y = x + b;
        *pixel( x, y ) = color;
        ++x;
    }
}

void Canvas::drawLineRU( int x, int y, int size, uint32_t color )
{
    int b = y + x;

    int y0 = y - size + 1;
    int y1 = y + 1;

    int x0 = x;
    int x1 = x + size;

    if( x0 < 0 )
        x0 = 0;
    if( x1 > width )
        x1 = width;

    if( y0 < 0 )
        y0 = 0;
    if( y1 > height )
        y1 = height;

    y0 = b - y0;
    y1 = b - y1;

    x0 = Max( x0, y1 + 1 );
    x1 = Min( x1, y0 + 1 );

    x = x0;
    while( x < x1 )
    {
        y = b - x;
        *pixel( x, y ) = color;
        ++x;
    }
}

Canvas Canvas::trim( int& x0, int& y0, int w, int h, bool change )
{
    Canvas result;

    int baseX = x0;
    int baseY = y0;

    int x1 = x0 + w;
    int y1 = y0 + h;

    if( x0 < 0 )
        x0 = 0;
    if( y0 < 0 )
        y0 = 0;

    if( x1 > width )
        x1 = width;
    if( y1 > height )
        y1 = height;

    int rh = y1 - y0;
    if( rh <= 0 )
        return result;

    int rw = x1 - x0;
    if( rw <= 0 )
        return result;

    result.pixels = pixel( x0, y0 );
    result.width = rw;
    result.height = rh;
    result.stride = stride;

    if( change )
    {
        x0 -= baseX;
        y0 -= baseY;
    }
    else
    {
        x0 = baseX;
        y0 = baseY;
    }
    return result;
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

    HDC hdc = CreateCompatibleDC( nullptr );
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
    HBITMAP hBitmap = CreateDIBSection( nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0 );
    if( !hBitmap || !bits )
    {
        SelectObject( hdc, oldFont );
        DeleteObject( hFont );
        DeleteDC( hdc );
        if( hBitmap )
            DeleteObject( hBitmap );
        return false;
    }

    HDC memDC = CreateCompatibleDC( nullptr );
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

int Group::width() const
{
    int left = std::numeric_limits<int>::max(), right = std::numeric_limits<int>::lowest();
    for( auto object : objects )
    {
        auto& o = *object;
        if( o.visible )
        {
            auto size = o.width();
            if( size > 0 )
            {
                auto p = o.x;
                if( p < left )
                    left = p;
                p += size;
                if( p > right )
                    right = p;
            }
        }
    }

    if( right <= left )
        return 0;

    return right - left;
}

int Group::height() const
{
    int top = std::numeric_limits<int>::max(), bottom = std::numeric_limits<int>::lowest();
    for( auto object : objects )
    {
        auto& o = *object;
        if( o.visible )
        {
            auto size = o.height();
            if( size > 0 )
            {
                auto p = o.y;
                if( p < top )
                    top = p;
                p += size;
                if( p > bottom )
                    bottom = p;
            }
        }
    }

    if( bottom <= top )
        return 0;

    return bottom - top;
}

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

void Group::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );
    for( auto object : objects )
        object->draw( canvas, dx, dy );
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

ActiveGroup::ActiveGroup() : Group(), Active()
{}

ActiveGroup::ActiveGroup( const ActiveGroup& other ) : Object(), Group( other ), Active( other )
{}

ActiveGroup::~ActiveGroup()
{}

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

    for( auto active : interactive )
    {
        if( active->visible && active->hover( x0, y0 ) )
            return true;
    }

    return false;
}

bool ActiveGroup::click( bool release, int x0, int y0 )
{
    inner( x0, y0 );

    for( auto i = interactive.rbegin(); i != interactive.rend(); ++i )
    {
        auto& active = * i;
        if( active->visible && active->contains( x0, y0 ) && active->click( release, x0, y0 ) )
            return true;
    }

    return false;
}

bool ActiveGroup::input( wchar_t c )
{
    for( auto active : interactive )
    {
        if( active->visible && active->input( c ) )
            return true;
    }

    return false;
}

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

Box::Box() : Object(), w( 0 ), h( 0 )
{}

Box::Box( const Box& other ) : Object( other ), w( other.w ), h( other.h )
{}

Box::~Box()
{}

int Box::width() const
{
    return w;
}

int Box::height() const
{
    return h;
}

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

Trigger::Trigger() : Box()
{}

Trigger::Trigger( const Trigger& other ) : Object( other ), Box( other )
{}

Trigger::~Trigger()
{}

void Trigger::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );
    canvas.trim( dx, dy, w, h ).stain();
}

Rectangle::Rectangle() : color( 0 )
{}

Rectangle::Rectangle( const Rectangle& other ) : Object( other ), Box( other ), color( other.color )
{}

Rectangle::~Rectangle()
{}

void Rectangle::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );
    canvas.trim( dx, dy, w, h ).fill( color );
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

void Image::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );

    ConstCanvas self( pixels.data(), bufferW, bufferH, bufferW );
    canvas.trim( dx, dy, bufferW, bufferH, true ).draw( self, dx, dy );
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

void ImageBlend::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );

    ConstCanvas self( pixels.data(), bufferW, bufferH, bufferW );
    canvas.trim( dx, dy, bufferW, bufferH, true ).drawBlend( self, dx, dy );
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

DynamicText *DynamicText::focus = nullptr;

DynamicText::DynamicText() : StaticText(), Active()
{}

DynamicText::DynamicText( const DynamicText& other ) : Object( other ), StaticText( other ), Active( other ), valid( other.valid ), setCallback( other.setCallback ), focused( other.focused )
{}

DynamicText::~DynamicText()
{
    if( focus == this )
        focus = nullptr;
}

void DynamicText::prepare( bool write )
{
    if( write )
        valid = setCallback ? setCallback( value ) : true;
    StaticText::prepare();
}

void DynamicText::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    auto idleColor = makeColor( 255, 255, 255, 255 );
    auto errorColor = makeColor( 255, 127, 127, 255 );
    auto focusColor = makeColor( 255, 255, 127, 255 );
    auto background = valid ? ( focused ? focusColor : idleColor ) : errorColor;

    int x1 = dx, y1 = dy;
    outer( dx, dy );

    canvas.trim( dx, dy, w, h ).fill( background );
    StaticText::draw( canvas, x1, y1 );
}

bool DynamicText::hover( int, int )
{
    return false;
}

bool DynamicText::click( bool release, int, int )
{
    if( release )
    {
        if( focus )
            focus->focused = false;
        focused = true;
        focus = this;
        prepare( false );
        return true;
    }
    return false;
}

bool DynamicText::input( wchar_t c )
{
    if( !focused )
        return false;

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
    return true;
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

void Combobox::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

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
        area.draw( canvas, dx, dy );
        text.value = std::move( o );
        text.prepare();
        text.x = area.x + ( w - text.w ) * 0.5;
        text.y = area.y + ( 16 - text.h ) * 0.5;
        text.draw( canvas, dx, dy );
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
    return true;
}

bool Combobox::click( bool release, int, int )
{
    if( release )
        open( !isOpen );
    return true;
}

bool Combobox::input( wchar_t )
{
    return false;
}

Button::Button() : Box(), Active(), wasHovered( false ), off( false )
{}

Button::Button( const Button& other ) : Object( other ), Box( other ), Active( other ), wasHovered( false ), off( other.off ), onHover( other.onHover ), onClick( other.onClick )
{}

Button::~Button()
{}

bool Button::hover( int, int )
{
    bool result = false;
    if( !off && onHover && wasHovered != hovered )
        result = onHover( hovered );

    wasHovered = hovered;
    return result;
}

bool Button::click( bool release, int, int )
{
    bool result = false;
    if( !off && onClick )
        result = onClick( release );

    return result;
}

bool Button::input( wchar_t )
{
    return false;
}

ActiveTrigger::ActiveTrigger() : Button()
{}

ActiveTrigger::ActiveTrigger( const ActiveTrigger& other ) : Object( other ), Button( other )
{}

ActiveTrigger::~ActiveTrigger()
{}

void ActiveTrigger::draw( Canvas&, int, int ) const
{}

TextButton::TextButton() : Button(), centerX( true ), centerY( true )
{}

TextButton::TextButton( const TextButton& other ) : Object( other ), Button( other ), centerX( other.centerX ), centerY( other.centerX ), desc( other.desc )
{}

TextButton::~TextButton()
{}

void TextButton::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible || w <= 0 || h <= 0 )
        return;

    outer( dx, dy );

    auto color = hovered && !off ? makeColor( 220, 220, 60, 255 ) : makeColor( 200, 200, 200, 255 );
    canvas.trim( dx, dy, w, h ).fill( color );

    StaticText text;
    text.color = off ? makeColor( 128, 128, 128, 255 ) : makeColor( 0, 0, 0, 255 );

    text.value = desc;
    text.prepare();

    text.x = centerX ? ( w - text.w ) * 0.5 : 16;
    text.y = centerY ? ( h - text.h ) * 0.5 : 16;

    text.draw( canvas, dx, dy );

    if( onHover )
    {
        canvas.drawLineRD( dx + w - 16, dy + 4, 4, text.color );
        canvas.drawLineRU( dx + w - 16, dy + h - 5, 4, text.color );
    }
}

MinimizeButton::MinimizeButton() : Button()
{}

MinimizeButton::MinimizeButton( const MinimizeButton& other ) : Object( other ), Button( other )
{}

MinimizeButton::~MinimizeButton()
{}

void MinimizeButton::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );

    auto color = hovered ? makeColor( 235, 235, 235, 255 ) : makeColor( 255, 255, 255, 255 );
    canvas.trim( dx, dy, w, h ).fill( color );

    auto black = makeColor( 0, 0, 0, 255 );

    canvas.drawLineR( dx + 3, dy + h / 2 - 1, w - 6, black );
}

MaximizeButton::MaximizeButton() : Button()
{}

MaximizeButton::MaximizeButton( const MaximizeButton& other ) : Object( other ), Button( other )
{}

MaximizeButton::~MaximizeButton()
{}

void MaximizeButton::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );

    auto color = hovered ? makeColor( 235, 235, 235, 255 ) : makeColor( 255, 255, 255, 255 );
    canvas.trim( dx, dy, w, h ).fill( color );

    auto black = makeColor( 0, 0, 0, 255 );
    auto half = hovered ? makeColor( 188, 188, 188, 255 ) : makeColor( 204, 204, 204, 255 );

    canvas.drawLineR( dx + 4, dy + 4, w - 8, half );
    canvas.drawLineD( dx + 4, dy + 4, w - 8, half );
    canvas.drawLineR( dx + 4, dy + h - 5, w - 8, half );
    canvas.drawLineD( dx + w - 5, dy + 4, w - 8, half );

    canvas.drawLineR( dx + 3, dy + 3, w - 6, black );
    canvas.drawLineD( dx + 3, dy + 3, w - 6, black );
    canvas.drawLineR( dx + 3, dy + h - 4, w - 6, black );
    canvas.drawLineD( dx + w - 4, dy + 3, w - 6, black );
}

CloseButton::CloseButton() : Button()
{}

CloseButton::CloseButton( const CloseButton& other ) : Object( other ), Button( other )
{}

CloseButton::~CloseButton()
{}

void CloseButton::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );

    auto color = hovered ? makeColor( 245, 10, 10, 255 ) : makeColor( 255, 255, 255, 255 );
    canvas.trim( dx, dy, w, h ).fill( color );

    auto black = makeColor( 0, 0, 0, 255 );
    auto half = hovered ? makeColor( 196, 8, 8, 255 ) : makeColor( 204, 204, 204, 255 );

    canvas.drawLineRD( dx + 4, dy + 3, w - 7, half );
    canvas.drawLineRU( dx + 4, dy + h - 4, w - 7, half );

    canvas.drawLineRD( dx + 3, dy + 4, w - 7, half );
    canvas.drawLineRU( dx + 3, dy + h - 5, w - 7, half );

    canvas.drawLineRD( dx + 3, dy + 3, w - 6, black );
    canvas.drawLineRU( dx + 3, dy + h - 4, w - 6, black );
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
    onClick = [this]( bool release )
    {
        if( release )
        {
            toggle = !toggle;
            return true;
        }
        return false;
    };
}

void PlusButton::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    outer( dx, dy );

    auto color = hovered && !off ? makeColor( 220, 220, 60, 255 ) : makeColor( 200, 200, 200, 255 );
    canvas.trim( dx, dy, w, h ).fill( color );

    StaticText text;
    text.color = off ? makeColor( 128, 128, 128, 255 ) : makeColor( 0, 0, 0, 255 );

    text.value = desc;
    text.prepare();

    text.x = w + 9;
    text.y = ( h - text.h ) * 0.5;

    text.draw( canvas, dx, dy );

    canvas.drawLineR( dx + 2, dy + h / 2, w - 4, makeColor( 0, 0, 0, 255 ) );
    if( !toggle )
        canvas.drawLineD( dx + w / 2, dy + 2, h - 4, makeColor( 0, 0, 0, 255 ) );
}

DropArea::DropArea()
{}

DropArea::DropArea( const DropArea& other ) : Object( other ), Button( other )
{}

DropArea::~DropArea()
{}

void DropArea::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible )
        return;

    if( hovered )
    {
        outer( dx, dy );
        canvas.trim( dx, dy, w, h ).fill( makeColor( 255, 255, 0, 255 ) );
    }
}

Scroller::Scroller() : horizontal( 0.3f ), vertical( 0.3f ), content( nullptr ), size( 8 )
{}

Scroller::Scroller( const Scroller& other ) : Object( other ), Box( other ), Active( other ), horizontal( other.horizontal ), vertical( other.vertical ), content( other.content ), size( other.size )
{}

void Scroller::calculate()
{
    s.tw = content->width();
    s.th = content->height();

    s.ok = true;
    if( s.tw < size || s.th < size )
    {
        s.ok = false;
        return;
    }

    s.xside = w - size;
    s.yside = h - size;

    s.cw = s.tw;
    s.ch = s.th;

    s.sw = w - s.cw;
    s.hscroll = s.sw < 0;

    s.sh = h - s.ch - ( s.hscroll ? size : 0 );
    s.vscroll = s.sh < 0;

    if( s.vscroll )
    {
        s.sw -= size;
        s.hscroll = s.sw < 0;
    }

    s.sw = s.hscroll ? 0 : s.sw;
    s.sh = s.vscroll ? 0 : s.sh;

    s.cw = w - s.sw - ( s.vscroll ? size : 0 );
    s.ch = h - s.sh - ( s.hscroll ? size : 0 );

    s.sw /= 2;
    s.sh /= 2;

    s.sizeh =  RoundDown( s.cw * s.cw / float( s.tw ) );
    s.posh = s.hscroll ? RoundDown( horizontal * ( s.cw - s.sizeh ) ) : 0;

    s.sizev =  RoundDown( s.ch * s.ch / float( s.th ) );
    s.posv = s.vscroll ? RoundDown( vertical * ( s.ch - s.sizev ) ) : 0;
}

void Scroller::scroll( int& dx, int& dy ) const
{
    dx += s.hscroll ? Round( horizontal * ( s.tw - s.cw ) ) : 0;
    dy += s.vscroll ? Round( vertical * ( s.th - s.ch ) ) : 0;
}

bool Scroller::contains( int x0, int y0 ) const
{
    inner( x0, y0 );

    if( x0 < 0 || w <= x0 || y0 < 0 || h <= y0 )
        return false;

    if( !s.ok )
        return false;

    if( x0 < s.cw && y0 < s.ch )
    {
        scroll( x0, y0 );
        return content->contains( x0, y0 );
    }

    return true;
}

bool Scroller::hover( int x0, int y0 )
{
    if( !s.ok )
        return false;

    inner( x0, y0 );

    if( holdx )
    {
        horizontal = ( x0 - *holdx ) / float( s.cw - s.sizeh );
        if( horizontal < 0.0f )
            horizontal = 0.0f;
        else if( horizontal > 1.0f )
            horizontal = 1.0f;
        return true;
    }
    if( holdy )
    {
        vertical = ( y0 - *holdy ) / float( s.ch - s.sizev ) ;
        if( vertical < 0.0f )
            vertical = 0.0f;
        else if( vertical > 1.0f )
            vertical = 1.0f;
        return true;
    }

    return false;
}

bool Scroller::click( bool release, int x0, int y0 )
{
    if( release )
    {
        release = holdx || holdy;
        holdx.reset();
        holdy.reset();
        return release;
    }

    if( !s.ok )
        return false;

    inner( x0, y0 );

    if( s.posh <= x0 && x0 < s.posh + s.sizeh && s.yside <= y0 && y0 < s.yside + size )
    {
        holdx = x0 - s.posh;
        release = true;
    }
    if( s.xside <= x0 && x0 < s.xside + size && s.posv <= y0 && y0 < s.posv + s.sizev )
    {
        holdy = y0 - s.posv;
        release = true;
    }

    return release;
}

bool Scroller::input( wchar_t )
{
    return false;
}

void Scroller::draw( Canvas& canvas, int dx, int dy ) const
{
    if( !visible || !content || !s.ok || w <= 0 || h <= 0 )
        return;

    outer( dx, dy );

    auto c = s;
    c.xside += dx;
    c.yside += dy;
    c.posh += dx;
    c.posv += dy;

    if( s.hscroll )
    {
        canvas.trim( dx, c.yside, c.cw, size ).fill( makeColor( 63, 63, 63, 255 ) );
        canvas.trim( c.posh, c.yside, c.sizeh, size ).fill( makeColor( 127, 127, 127, 255 ) );
    }

    if( s.vscroll )
    {
        canvas.trim( c.xside, dy, size, c.ch ).fill( makeColor( 63, 63, 63, 255 ) );
        canvas.trim( c.xside, c.posv, size, c.sizev ).fill( makeColor( 127, 127, 127, 255 ) );
    }

    if( s.hscroll && s.vscroll )
        canvas.trim( c.xside, c.yside, size, size ).diamond( makeColor( 255, 255, 255, 255 ), makeColor( 0, 0, 0, 255 ) );

    dx += c.sw;
    dy += c.sh;

    auto part = canvas.trim( dx, dy, c.cw, c.ch, true );
    scroll( dx, dy );
    content->draw( part, -dx, -dy );
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

    button.onClick = [this]( bool release )
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
        return true;
    };

    space.x = 0;
    space.y = -6;
    space.w = 128;
    space.h = 4;

    space.onClick = [this]( bool release )
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
        return true;
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

    scroller.content = &content;
    add( &self );
    add( &client );
    add( &scroller );
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
    titleBar( other.titleBar ), leftBorder( other.leftBorder ), rightBorder( other.rightBorder ), topBorder( other.topBorder ), bottomBorder( other.bottomBorder ),
    client( other.client ), icon( other.icon ), content( other.content ), scroller( other.scroller ), title( other.title ),
    minimizeButton( other.minimizeButton ), maximizeButton( other.maximizeButton ), closeButton( other.closeButton )
{
    minimizeButton.onClick = nullptr;
    maximizeButton.onClick = nullptr;
    closeButton.onClick = nullptr;

    scroller.content = &content;
    add( &self );
    add( &client );
    add( &scroller );
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
    auto minWidth = 2 * borderWidth;

    if( titleBarMinWidth > minWidth )
        return titleBarMinWidth;

    return minWidth;
}

int Window::minHeight() const
{
    return titlebarHeight + 2 * borderWidth;
}

void Window::update()
{
    titleBar.x = self.x;
    titleBar.y = self.y;
    titleBar.w = self.w;
    titleBar.h = titlebarHeight + borderWidth;

    leftTrigger.x = self.x - triggerWidth;
    leftTrigger.y = self.y - triggerWidth;
    leftTrigger.w = triggerWidth;
    leftTrigger.h = self.h + 2 * triggerWidth;

    rightTrigger.x = self.x + self.w;
    rightTrigger.y = self.y - triggerWidth;
    rightTrigger.w = triggerWidth;
    rightTrigger.h = self.h + 2 * triggerWidth;

    topTrigger.x = self.x - triggerWidth;
    topTrigger.y = self.y - triggerWidth;
    topTrigger.w = self.w + 2 * triggerWidth;
    topTrigger.h = triggerWidth;

    bottomTrigger.x = self.x - triggerWidth;
    bottomTrigger.y = self.y + self.h;
    bottomTrigger.w = self.w + 2 * triggerWidth;
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
    client.color = content.bufferW > 0 && content.bufferH > 0 ? makeColor( 60, 70, 200, 255 ) : makeColor( 170, 170, 170, 255 );

    scroller.place( client );
    scroller.calculate();

    mouseTrigger.x = scroller.x + scroller.s.sw;
    mouseTrigger.y = scroller.y + scroller.s.sh;
    mouseTrigger.w = scroller.s.cw;
    mouseTrigger.h = scroller.s.ch;
}

bool Window::run( bool lock )
{
    return GenericWindow::create( *this, lock );
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

bool noWindows()
{
    return GenericWindow::count() == 0;
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

            object->onClick = [s = value.set]( bool release )
            {
                if( release )
                {
                    s( L"released" );
                    return true;
                }
                return false;
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
        yesButton.onClick = [this]( bool release )
        {
            if( release )
            {
                answer = true;
                closeButton.onClick( release );
                return true;
            }
            return false;
        };

        buttons.push_back( &noButton );
        noButton.desc = L"no";
        noButton.onClick = [this]( bool release )
        {
            if( release )
            {
                answer = false;
                closeButton.onClick( release );
                return true;
            }
            return false;
        };
    }
    else
    {
        buttons.push_back( &cancelButton );
        cancelButton.desc = L"ok";
        cancelButton.onClick = [this]( bool release )
        {
            return closeButton.onClick( release );
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
            button->onClick = [&root, callback = p.callback]( bool release )
            {
                if( release )
                {
                    callback();
                    root.closeButton.onClick( true );
                    return true;
                }
                return false;
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
            button->onHover = [next = subMenu.get()]( bool inside )
            {
                if( inside )
                {
                    next->visible = true;
                    return true;
                }
                if( !next->hovered )
                {
                    next->visible = false;
                    return true;
                }
                return false;
            };
        }

        button->off = button->off && button->onHover;

        y += button->h;
        root.storage.push_back( button );
        menu->add( button.get() );
    }

    trigger->w = width + 1;
    trigger->h = y - trigger->y;
    trigger->onHover = [&root, rootMenu, self = menu.get()]( bool inside )
    {
        if( inside )
        {
            if( rootMenu )
            {
                rootMenu->visible = true;
                return true;
            }
            return false;
        }
        self->visible = false;
        return false;
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
        closeButton.onClick( true );
        return true;
    }
    return result;
}

int ContextMenu::minWidth() const
{
    return 0;
}

int ContextMenu::minHeight() const
{
    return 0;
}

void ContextMenu::update()
{}

bool ContextMenu::run( bool lock )
{
    if( storage.empty() )
        return true;

    POINT p;
    if( !GetCursorPos( &p ) )
        return true;

    storage[0]->visible = true;
    storage[0]->x = p.x;
    storage[0]->y = p.y;

    return Window::run( lock );
}

FileManager::FileManager( std::filesystem::path initial, bool write )
{
    add( &confirm );
    confirm.desc = write ? L"Save" : L"Open";
    if( write )
    {
        confirm.onClick = [this]( bool release )
        {
            if( !release )
                return false;

            if( std::filesystem::exists( file.value ) )
            {
                if( std::filesystem::is_regular_file( file.value ) )
                {
                    auto& question = popup.emplace( Popup::Type::Question, confirm.desc + L" file", L"File already exists, do you want to overwrite it?" );
                    question.onClose = [this, &question]()
                    {
                        if( question.answer && *question.answer )
                        {
                            choice = file.value;
                            closeButton.onClick( true );
                        }
                    };
                    question.run();
                }
                else
                {
                    popup.emplace( Popup::Type::Warning, confirm.desc + L" file", L"It's not a file." ).run();
                }
            }
            else
            {
                choice = file.value;
                closeButton.onClick( true );
            }
            return true;
        };
    }
    else
    {
        confirm.onClick = [this]( bool release )
        {
            if( !release )
                return false;

            std::filesystem::path candidate = file.value;
            if( std::filesystem::exists( candidate ) && std::filesystem::is_regular_file( candidate ) )
            {
                choice = candidate;
                closeButton.onClick( true );
            }
            else
            {
                popup.emplace( Popup::Type::Warning, confirm.desc + L" file", L"File with such path does not exist." ).run();
            }
            return true;
        };
    }

    add( &reject );
    reject.desc = L"Cancel";
    reject.onClick = [this]( bool release )
    {
        return closeButton.onClick( release );
    };

    self.w = 512;
    self.h = 1024;
    root = std::move( initial );

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
            button->onClick = [this, p = path.lexically_normal()]( bool release )
            {
                if( !release )
                    return false;

                root = p;
                file.value = p.wstring();
                file.prepare();
                return true;
            };
        }
        else if( std::filesystem::is_regular_file( path ) )
        {
            button->onClick = [this, p = path.lexically_normal()]( bool release )
            {
                if( !release )
                    return false;

                file.value = p.wstring();
                file.prepare( true );
                return true;
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
                popup.emplace( Popup::Type::Error, L"Error", L"Failed to get list of drives." ).run();
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
        popup.emplace( Popup::Type::Error, L"Error", e.message() ).run();
    }
    catch( const std::exception &e )
    {
        popup.emplace( Popup::Type::Error, L"Error", Exception::extract( e.what() ) ).run();
    }
    catch( ... )
    {
        popup.emplace( Popup::Type::Error, L"Error", L"Program failed!" ).run();
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

static void filePath( std::function<void( const std::optional<std::filesystem::path>& )> callback, std::filesystem::path path, bool save )
{
    if( path.empty() )
    {
        path = std::getenv( "USERPROFILE" );
        path /= L"Downloads";
    }

    static std::vector<std::shared_ptr<FileManager>> fms;
    auto& fm = *fms.emplace_back( std::make_shared<FileManager>( std::move( path ), save ) );
    fm.onClose = [&fm, call = std::move( callback )]()
    {
        call( fm.choice );

        auto i = fms.begin();
        while( i != fms.end() )
        {
            if( i->get() == &fm )
            {
                fms.erase( i );
                break;
            }
            ++i;
        }
    };
    fm.run();
}

void savePath( std::function<void( const std::optional<std::filesystem::path>& )> callback, std::filesystem::path path )
{
    filePath( std::move( callback ), std::move( path ), true );
}

void openPath( std::function<void( const std::optional<std::filesystem::path>& )> callback, std::filesystem::path path )
{
    filePath( std::move( callback ), std::move( path ), false );
}

void GenericWindow::update()
{
    RECT rect;
    GetWindowRect( hndwnd, &rect );

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

    auto *pixels = ( uint32_t * )pBits;
    for( auto& window : stack )
    {
        GraphicInterface::Canvas canvas( pixels, width, height, width );
        auto& desc = window->desc;

        desc.update();
        desc.draw( canvas, 0, 0 );
    }

    BLENDFUNCTION blend;
    clear( &blend, sizeof( blend ) );
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT point = {0, 0};
    SIZE sizeWindow = {width, height};
    UpdateLayeredWindow( hndwnd, hdcScreen, nullptr, &sizeWindow, hdcMem, &point, 0, &blend, ULW_ALPHA );

    SelectObject( hdcMem, hOldBmp );
    DeleteObject( hBitmap );
    DeleteDC( hdcMem );
    ReleaseDC( nullptr, hdcScreen );
}

size_t GenericWindow::count()
{
    return stack.size();
}

std::vector<std::shared_ptr<GenericWindow>> GenericWindow::stack;
GenericWindow *GenericWindow::active = nullptr;
bool GenericWindow::needCleanup = false;
HWND GenericWindow::hndwnd = nullptr;

GenericWindow::GenericWindow( GraphicInterface::Window &d, bool l ) : outputData( d ), desc( d ), original( nullptr ), preview( nullptr ), proxy( nullptr ), lock( l )
{
    desc.closeButton.onClick = [this]( bool release )
    {
        if( release )
        {
            close();
            return true;
        }
        return false;
    };

    desc.maximizeButton.onClick = [this]( bool release )
    {
        if( release )
        {
            maximize();
            return true;
        }
        return false;
    };

    desc.minimizeButton.onClick = [this]( bool release )
    {
        if( release )
        {
            minimize();
            return true;
        }
        return false;
    };

    originalX = originalY = originalW = originalH = 0;
    maximized = false;

    createTab();
}

GenericWindow::~GenericWindow()
{
    desc.closeButton.onClick = nullptr;
    desc.maximizeButton.onClick = nullptr;
    desc.minimizeButton.onClick = nullptr;

    releaseData();
}

bool GenericWindow::create( GraphicInterface::Window &desc, bool lock )
{
    // This code is positioned in lambda to accesses private members of GenericWindow
    auto windowProc = []( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) -> LRESULT
    {
        static bool left = false, right = false, top = false, bottom = false;
        static GenericWindow *moving = nullptr, *scaling = nullptr;
        static bool focus = false;
        static POINT pin = {};

        struct Cursors
        {
            HCURSOR arrow = nullptr;
            HCURSOR sizeHor = nullptr;
            HCURSOR sizeVer = nullptr;
            HCURSOR sizePD = nullptr;
            HCURSOR sizeSD = nullptr;
            HCURSOR sizeAll = nullptr;
            HCURSOR custom = nullptr;
        };

        static Cursors cursors;

        switch( message )
        {
        case WM_CREATE:
            {
                cursors.arrow   = LoadCursorW( nullptr, IDC_ARROW );
                cursors.sizeHor = LoadCursorW( nullptr, IDC_SIZEWE );
                cursors.sizeVer = LoadCursorW( nullptr, IDC_SIZENS );
                cursors.sizePD  = LoadCursorW( nullptr, IDC_SIZENWSE );
                cursors.sizeSD  = LoadCursorW( nullptr, IDC_SIZENESW );
                cursors.sizeAll = LoadCursorW( nullptr, IDC_SIZEALL );

                //AND XOR → Result
                //  0   0 → Black
                //  0   1 → White
                //  1   0 → Screen (transparent)
                //  1   1 → Reverse-screen (invert)
                // Bytes per row = (width + 7) / 8
                // Each byte holds 8 pixels

                BYTE andMask[] =
                {
                    0xF7, 0x80, 0xF7, 0x80, 0xF7, 0x80, 0xFF, 0xFF, 0x1C, 0x00, 0xFF, 0xFF, 0xF7, 0x80, 0xF7, 0x80, 0xF7, 0x80
                };

                BYTE xorMask[] =
                {
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEB, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                };

                cursors.custom = CreateCursor( nullptr, 4, 4, 9, 9, andMask, xorMask );

                SetCursor( cursors.arrow );
            }
            break;
        case WM_DESTROY:
            moving = scaling = nullptr;
            if( cursors.custom )
            {
                DestroyCursor( cursors.custom );
                cursors.custom = nullptr;
            }
            PostQuitMessage( 0 );
            return 0;
        case WM_APP:
            break;
        case WM_KILLFOCUS:
            left = right = top = bottom = false;
            moving = scaling = nullptr;
            focus = false;
            break;
        case WM_SETFOCUS:
            focus = true;
            break;
        case WM_SETCURSOR:
            return TRUE;
        case WM_PAINT:
            break;
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
            if( focus )
            {
                POINT point;
                GetCursorPos( &point );

                if( moving )
                {
                    moving->desc.x = pin.x + point.x;
                    moving->desc.y = pin.y + point.y;
                    GenericWindow::update();
                    return 0;
                }

                if( scaling )
                {
                    if( top )
                    {
                        auto height = pin.y - point.y;
                        auto minHeight = scaling->desc.minHeight();
                        if( height >= minHeight )
                        {
                            scaling->desc.y = point.y;
                            scaling->desc.self.h = height;
                        }
                        else
                        {
                            scaling->desc.y = pin.y - minHeight;
                            scaling->desc.self.h = minHeight;
                        }
                    }
                    if( left )
                    {
                        auto width = pin.x - point.x;
                        auto minWidth = scaling->desc.minWidth();
                        if( width >= minWidth )
                        {
                            scaling->desc.x = point.x;
                            scaling->desc.self.w = width;
                        }
                        else
                        {
                            scaling->desc.x = pin.x - minWidth;
                            scaling->desc.self.w = minWidth;
                        }
                    }
                    if( bottom )
                    {
                        auto height = point.y - pin.y;
                        auto minHeight = scaling->desc.minHeight();
                        if( height >= minHeight )
                        {
                            scaling->desc.self.h = height;
                        }
                        else
                        {
                            scaling->desc.self.h = minHeight;
                        }
                    }
                    if( right )
                    {
                        auto width = point.x - pin.x;
                        auto minWidth = scaling->desc.minWidth();
                        if( width >= minWidth )
                        {
                            scaling->desc.self.w = width;
                        }
                        else
                        {
                            scaling->desc.self.w = minWidth;
                        }
                    }

                    GenericWindow::process( [&]( auto & w )
                    {
                        w.desc.update();
                        w.inputData.width = w.desc.client.w;
                        w.inputData.height = w.desc.client.h;
                        w.inputData.scale = true;
                        bool result = w.handle();
                        w.inputData.scale = false;
                        return result;
                    } );

                    GenericWindow::cleanup();
                    GenericWindow::update();
                    return 0;
                }

                bool response = GenericWindow::process( [&]( auto & w )
                {
                    int x = point.x, y = point.y;

                    w.desc.inner( x, y );

                    left   = w.desc.leftTrigger.contains( x, y );
                    right  = w.desc.rightTrigger.contains( x, y );
                    top    = w.desc.topTrigger.contains( x, y );
                    bottom = w.desc.bottomTrigger.contains( x, y );

                    if( top && left )
                    {
                        SetCursor( cursors.sizePD );
                        return true;
                    }
                    if( top && right )
                    {
                        SetCursor( cursors.sizeSD );
                        return true;
                    }
                    if( bottom && left )
                    {
                        SetCursor( cursors.sizeSD );
                        return true;
                    }
                    if( bottom && right )
                    {
                        SetCursor( cursors.sizePD );
                        return true;
                    }
                    if( left )
                    {
                        SetCursor( cursors.sizeHor );
                        return true;
                    }
                    if( right )
                    {
                        SetCursor( cursors.sizeHor );
                        return true;
                    }
                    if( top )
                    {
                        SetCursor( cursors.sizeVer );
                        return true;
                    }
                    if( bottom )
                    {
                        SetCursor( cursors.sizeVer );
                        return true;
                    }
                    if( w.desc.hover( point.x, point.y ) )
                    {
                        SetCursor( cursors.arrow );
                        return true;
                    }
                    if( w.desc.mouseTrigger.contains( x, y ) )
                    {
                        SetCursor( cursors.custom );

                        w.desc.mouseTrigger.inner( x, y );
                        w.desc.scroller.scroll( x, y );
                        w.inputData.mouseX = x;
                        w.inputData.mouseY = y;
                        return w.handle();
                    }
                    SetCursor( cursors.arrow );
                    return w.desc.self.contains( x, y );
                } );
                if( !response )
                    break;

                GenericWindow::cleanup();
                GenericWindow::update();
                return 0;
            }
            return 0;
        case WM_CHAR:
            if( focus )
            {
                auto& p = wParam;
                bool sign = p == L' ' || p == L'\b' || p == L'_' || p == L'-' || p == L'+' || p == L'/' || p == L'\\' || p == L'.' || p == L',' || p == L':' || p == L';' || p == L'!' || p == L'?';
                bool letter = ( L'a' <= p && p <= L'z' ) || ( L'A' <= p && p <= L'Z' );
                bool number = L'0' <= p && p <= L'9';
                if( sign || letter || number )
                {
                    bool response = GenericWindow::process( [&]( auto & w )
                    {
                        w.inputData.typed = p;
                        bool f0 = w.desc.input( p );
                        bool f1 = w.handle();
                        w.inputData.typed = L'\0';
                        return f0 || f1;
                    } );
                    if( !response )
                        break;

                    GenericWindow::cleanup();
                    GenericWindow::update();
                    return 0;
                }
            }
            return 0;
        default:
            break;
        }

        bool pressed = message == WM_KEYDOWN;
        bool pressedSystem = message == WM_SYSKEYDOWN;

        bool released = message == WM_KEYUP;
        bool releasedSystem = message == WM_SYSKEYUP;

        bool mouse = false;

        if( message == WM_LBUTTONUP )
        {
            wParam = VK_LBUTTON;
            released = true;
            mouse = true;
        }
        else if( message == WM_RBUTTONUP )
        {
            wParam = VK_RBUTTON;
            released = true;
            mouse = true;
        }
        else if( message == WM_MBUTTONUP )
        {
            wParam = VK_MBUTTON;
            released = true;
            mouse = true;
        }
        else if( message == WM_LBUTTONDOWN )
        {
            wParam = VK_LBUTTON;
            pressed = true;
            mouse = true;
        }
        else if( message == WM_RBUTTONDOWN )
        {
            wParam = VK_RBUTTON;
            pressed = true;
            mouse = true;
        }
        else if( message == WM_MBUTTONDOWN )
        {
            wParam = VK_MBUTTON;
            pressed = true;
            mouse = true;
        }

        pressed = pressed || pressedSystem;
        released = released || releasedSystem;

        bool system = pressedSystem || releasedSystem;

        if( focus && ( pressed || released ) && !system )
        {
            POINT point;
            GetCursorPos( &point );

            if( mouse && released )
            {
                if( GenericWindow::focus( point.x, point.y ) )
                    return 0;
            }

            bool response = GenericWindow::process( [&]( auto & w )
            {
                auto &input = w.inputData;
                switch( wParam )
                {
                case VK_LBUTTON:
                    {
                        if( pressed )
                        {
                            int x = point.x, y = point.y;
                            w.desc.inner( x, y );

                            if( top || left || bottom || right )
                            {
                                scaling = &w;
                                pin.x = w.desc.x + w.desc.self.x;
                                pin.y = w.desc.y + w.desc.self.y;
                                if( top )
                                    pin.y += w.desc.self.h;
                                if( left )
                                    pin.x += w.desc.self.w;
                                SetCapture( hndwnd );
                                return true;
                            }

                            if( w.desc.titleBar.contains( x, y ) && !w.desc.minimizeButton.contains( x, y ) && !w.desc.maximizeButton.contains( x, y ) && !w.desc.closeButton.contains( x, y ) )
                            {
                                moving = &w;
                                pin.x = w.desc.x - point.x;
                                pin.y = w.desc.y - point.y;
                                SetCapture( hndwnd );
                                return true;
                            }

                            if( w.desc.click( false, point.x, point.y ) )
                                return true;

                            if( w.desc.mouseTrigger.contains( x, y ) )
                            {
                                w.inputData.leftMouse = true;
                                return w.handle();
                            }
                        }
                        else
                        {
                            if( moving )
                            {
                                moving = nullptr;
                                ReleaseCapture();
                                return true;
                            }

                            if( scaling )
                            {
                                scaling = nullptr;
                                ReleaseCapture();
                                return true;
                            }

                            int x = point.x, y = point.y;

                            if( w.desc.click( true, x, y ) )
                                return true;

                            w.desc.inner( x, y );
                            if( w.desc.mouseTrigger.contains( x, y ) )
                            {
                                w.inputData.leftMouse = false;
                                return w.handle();
                            }
                        }
                    }
                    return false;
                case VK_RBUTTON:
                    {
                        int x = point.x, y = point.y;
                        w.desc.inner( x, y );
                        if( w.desc.mouseTrigger.contains( x, y ) )
                        {
                            w.inputData.rightMouse = pressed;
                            return w.handle();
                        }
                    }
                    return false;
                case VK_MBUTTON:
                    {
                        int x = point.x, y = point.y;
                        w.desc.inner( x, y );
                        if( w.desc.mouseTrigger.contains( x, y ) )
                        {
                            w.inputData.middleMouse = pressed;
                            return w.handle();
                        }
                    }
                    return false;
                case VK_UP:
                    input.up = pressed;
                    return w.handle();
                case VK_DOWN:
                    input.down = pressed;
                    return w.handle();
                case VK_LEFT:
                    input.left = pressed;
                    return w.handle();
                case VK_RIGHT:
                    input.right = pressed;
                    return w.handle();
                case VK_ESCAPE:
                    input.escape = pressed;
                    return w.handle();
                case VK_DELETE:
                    input.del = pressed;
                    return w.handle();
                case VK_CONTROL:
                    input.ctrl = pressed;
                    return w.handle();
                case VK_SHIFT:
                    input.shift = pressed;
                    return w.handle();
                case VK_SPACE:
                    input.space = pressed;
                    return w.handle();
                case VK_RETURN:
                    input.enter = pressed;
                    return w.handle();
                case VK_F1:
                    input.f1 = pressed;
                    return w.handle();
                default:
                    break;
                }

                if( 'A' <= wParam && wParam <= 'Z' )
                {
                    auto &key = input.keys.letter( wParam );
                    key = pressed;
                    return w.handle();
                }

                if( '0' <= wParam && wParam <= '9' )
                {
                    auto &key = input.keys.digit( wParam - '0' );
                    key = pressed;
                    return w.handle();
                }

                return false;
            } );
            if( response )
            {
                GenericWindow::cleanup();
                GenericWindow::update();
                return 0;
            }
        }

        return DefWindowProc( hwnd, message, wParam, lParam );
    };
    auto proxyProc = []( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) -> LRESULT
    {
        auto window = ( GenericWindow* )GetWindowLongPtrW( hwnd, GWLP_USERDATA );
        auto sync = [&]()
        {
            auto& d = window->desc;
            if( d.visible )
                ShowWindow( hwnd, SW_SHOWNOACTIVATE );
            else
                ShowWindow( hwnd, SW_MINIMIZE );
            SetWindowPos( hwnd, HWND_BOTTOM, d.x, d.y, d.width(), d.height(), 0 );
        };

        switch( message )
        {
        case WM_CREATE:
            break;
        case WM_DESTROY:
            return 0;
        case WM_DWMSENDICONICTHUMBNAIL:
            {
                if( !window )
                    break;

                sync();

                UINT maxWidth  = HIWORD( lParam );
                UINT maxHeight = LOWORD( lParam );
                HBITMAP bitmap = window->imageScale( maxWidth, maxHeight );

                auto hr = DwmSetIconicThumbnail( hwnd, bitmap, 0 );
                if( FAILED( hr ) )
                    MessageBoxW( nullptr, ( L"DwmSetIconicThumbnail: " + std::to_wstring( hr ) ).c_str(), L"Problem", MB_ICONERROR );
            }
            return 0;
        case WM_DWMSENDICONICLIVEPREVIEWBITMAP:
            {
                if( !window )
                    break;

                sync();

                HBITMAP bitmap = window->image();

                POINT pt = {0, 0};
                auto hr = DwmSetIconicLivePreviewBitmap( hwnd, bitmap, &pt, 0 );
                if( FAILED( hr ) )
                    MessageBoxW( nullptr, ( L"DwmSetIconicLivePreviewBitmap: " + std::to_wstring( hr ) ).c_str(), L"Problem", MB_ICONERROR );
            }
            return 0;
        case WM_SYSCOMMAND:
            if( window && ( wParam & 0xfff0 ) == SC_RESTORE )
            {
                auto& d = window->desc;
                if( !d.visible )
                {
                    d.visible = true;
                    window->focus();
                    window->update();
                }
                return 0;
            }
            break;
        case WM_ACTIVATE:
            if( LOWORD( wParam ) != WA_INACTIVE )
            {
                SetForegroundWindow( GenericWindow::hndwnd );
                SetFocus( GenericWindow::hndwnd );
                return 0;
            }
            break;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        default:
            break;
        }
        return DefWindowProc( hwnd, message, wParam, lParam );
    };

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

        desc.update();
    }

    auto& window = *stack.emplace_back( std::make_shared<GenericWindow>( desc, lock ) );
    auto& input = window.inputData;

    killFocus();
    active = &window;

    input.init = true;
    input.width = desc.client.w;
    input.height = desc.client.h;
    window.handle();
    input.init = false;

    if( hndwnd )
        return false;

    WNDCLASSEXW wc;
    std::wstring className = L"GenericWindowImplementationWinAPI", proxyTab = L"ProxyTab";

    clear( &wc, sizeof( wc ) );
    wc.cbSize = sizeof( wc );
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandleW( nullptr );
    wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
    wc.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wc.lpszClassName = className.c_str();
    makeException( RegisterClassExW( &wc ) );

    clear( &wc, sizeof( wc ) );
    wc.cbSize = sizeof( wc );
    wc.lpfnWndProc = proxyProc;
    wc.hInstance = GetModuleHandleW( nullptr );
    wc.lpszClassName = proxyTab.c_str();
    makeException( RegisterClassExW( &wc ) );

    RECT screenRect;
    GetClientRect( GetDesktopWindow(), &screenRect );

    hndwnd = CreateWindowExW( WS_EX_LAYERED | WS_EX_TOOLWINDOW,
                              className.c_str(), desc.title.value.c_str(), WS_POPUP | WS_VISIBLE,
                              screenRect.left, screenRect.top, screenRect.right - screenRect.left, screenRect.bottom - screenRect.top,
                              nullptr, nullptr, GetModuleHandleW( nullptr ), nullptr );
    makeException( hndwnd );

    window.createTab();

    update();

    MSG msg = {};
    BOOL result;

    while( ( result = GetMessageW( &msg, nullptr, 0, 0 ) ) != 0 )
    {
        if( result == -1 )
        {
            // Error
            break;
        }

        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    UnregisterClassW( className.c_str(), GetModuleHandleW( nullptr ) );
    UnregisterClassW( proxyTab.c_str(), GetModuleHandleW( nullptr ) );

    return true;
}

void GenericWindow::close()
{
    outputData.quit = true;
    needCleanup = true;
}

void GenericWindow::maximize()
{
    if( maximized )
    {
        desc.x = originalX;
        desc.y = originalY;
        desc.self.w = originalW;
        desc.self.h = originalH;
        maximized = false;
    }
    else
    {
        originalX = desc.x;
        originalY = desc.y;
        originalW = desc.self.w;
        originalH = desc.self.h;

        RECT screenRect;
        GetClientRect( GetDesktopWindow(), &screenRect );

        desc.x = screenRect.left;
        desc.y = screenRect.top;
        desc.self.w = screenRect.right - desc.x;
        desc.self.h = screenRect.bottom - desc.y;
        maximized = true;
    }
}

void GenericWindow::minimize()
{
    desc.visible = !desc.visible;
}

bool GenericWindow::handle()
{
    if( outputData.quit )
    {
        needCleanup = true;
        return false;
    }

    bool response = false;
    if( desc.handleMsg )
        response = desc.handleMsg( inputData, outputData );

    inputReset();

    auto &img = outputData.image;
    if( img.changed() )
    {
        GenericWindow::update();
        img.reset();
    }

    return response;
}

void GenericWindow::focus()
{
    killFocus();
    active = this;
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

void GenericWindow::killFocus()
{
    if( active )
    {
        active->inputRelease();
        active->inputReset();
        active = nullptr;
    }
}

void GenericWindow::createTab()
{
    if( proxy )
        return;

    proxy = CreateWindowExW( WS_EX_TRANSPARENT, L"ProxyTab", desc.title.value.c_str(), WS_POPUP, desc.x, desc.y, desc.width(), desc.height(), nullptr, nullptr, GetModuleHandleW( nullptr ), nullptr );
    if( !proxy )
        return;

    ShowWindow( proxy, SW_SHOWNOACTIVATE );

    SetWindowLongPtrW( proxy, GWLP_USERDATA, ( LONG_PTR )this );

    BOOL enabled = FALSE;
    DwmIsCompositionEnabled( &enabled );
    makeException( enabled );

    BOOL yes = TRUE;
    makeException( SUCCEEDED( DwmSetWindowAttribute( proxy, DWMWA_FORCE_ICONIC_REPRESENTATION, &yes, sizeof( yes ) ) ) );
    makeException( SUCCEEDED( DwmSetWindowAttribute( proxy, DWMWA_HAS_ICONIC_BITMAP, &yes, sizeof( yes ) ) ) );

    makeException( SUCCEEDED( DwmInvalidateIconicBitmaps( proxy ) ) );
};

void GenericWindow::releaseData()
{
    if( proxy )
        DestroyWindow( proxy );
    if( original )
        DeleteObject( original );
    if( preview )
        DeleteObject( preview );
}

HBITMAP GenericWindow::imageScale( int width, int height )
{
    if( preview )
        DeleteObject( preview );

    uint32_t* pixels = nullptr;
    setupBitmap( width, height, preview, pixels );

    image();

    HDC target = CreateCompatibleDC( nullptr );
    HGDIOBJ tBitmap = SelectObject( target, preview );

    HDC source = CreateCompatibleDC( target );
    HGDIOBJ sBitmap = SelectObject( source, original );

    StretchBlt(
        target, 0, 0, width, height,
        source, 0, 0, desc.width(), desc.height(),
        SRCCOPY
    );

    SelectObject( target, tBitmap );
    SelectObject( source, sBitmap );
    DeleteDC( target );
    DeleteDC( source );

    /*
    for( int x = 0; x < width; ++x )
    {
        for( int y = 0; y < height; ++y )
        {
            uint8_t r = 255 * x / ( width - 1 );
            uint8_t g = 255 * y / ( height - 1 );
            uint8_t b = 128;
            pixels[y * width + x] = b | ( g << 8 ) | ( r << 16 ) | ( 255u << 24 );
        }
    }
    */

    return preview;
};

HBITMAP GenericWindow::image()
{
    if( original )
        DeleteObject( original );

    bool visible = desc.visible;
    desc.visible = true;
    desc.update();

    int width = desc.width();
    int height = desc.height();

    uint32_t* pixels = nullptr;
    setupBitmap( width, height, original, pixels );

    if( original && pixels )
    {
        GraphicInterface::Canvas canvas( pixels, width, height, width );
        desc.draw( canvas, -desc.x, -desc.y );
    }

    desc.visible = visible;

    return original;
};

void GenericWindow::setupBitmap( int width, int height, HBITMAP& bitmap, uint32_t*& pixels )
{
    BITMAPINFO bmi;
    clear( &bmi, sizeof( BITMAPINFO ) );
    bmi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    VOID* data = nullptr;
    HDC hdc = CreateCompatibleDC( nullptr );

    bitmap = CreateDIBSection( hdc, &bmi, DIB_RGB_COLORS, &data, nullptr, 0 );
    pixels = ( uint32_t* )data;

    DeleteDC( hdc );
}

void GenericWindow::setFocus()
{
    killFocus();
    if( !stack.empty() )
        active = stack[0].get();
}

bool GenericWindow::focus( int x0, int y0 )
{
    size_t i = 0;
    while( i < stack.size() )
    {
        auto& w = *stack[stack.size() - i - 1];
        auto& lock = w.lock;

        int x = x0, y = y0;
        w.desc.inner( x, y );

        if( w.desc.self.contains( x, y ) )
        {
            if( active == &w )
                return false;

            killFocus();
            active = &w;
            return true;
        }

        if( lock )
            return false;

        ++i;
    }
    return false;
}

bool GenericWindow::process( const std::function<bool( GenericWindow& )> & action )
{
    if( !active )
        return false;

    try
    {
        if( action( *active ) )
            return true;
    }
    catch( const Exception &e )
    {
        active->popup.emplace( Popup::Type::Error, L"Error", e.message() ).run();
        return true;
    }
    catch( const std::exception &e )
    {
        active->popup.emplace( Popup::Type::Error, L"Error", Exception::extract( e.what() ) ).run();
        return true;
    }
    catch( ... )
    {
        active->popup.emplace( Popup::Type::Error, L"Error", L"Program failed!" ).run();
        return true;
    }
    return false;
}

void GenericWindow::cleanup()
{
    if( needCleanup )
    {
        std::vector<std::shared_ptr<GenericWindow>> left;
        std::vector<std::function<void()>> finalizers;

        for( auto& window : stack )
        {
            if( window->outputData.quit )
            {
                if( window.get() == active )
                    active = nullptr;
                finalizers.emplace_back( window->desc.onClose );
            }
            else
            {
                left.emplace_back( std::move( window ) );
            }
        }

        stack = std::move( left );
        if( stack.empty() )
        {
            DestroyWindow( hndwnd );
            hndwnd = nullptr;
        }

        if( !active )
            setFocus();

        for( const auto& f : finalizers )
        {
            if( f )
                f();
        }
    }
};
