#include "Line.h"

#include "Basic.h"

DrawLine::DrawLine()
{}

DrawLine::DrawLine( int x0, int y0, int x1, int y1 ) : varX( x0 ), varY( y0 ), finalX( x1 ), finalY( y1 )
{
    w = x1 - x0;
    h = y1 - y0;
    dx = Abs( w );
    dy = Abs( h );
    sx = 0 < w ? 1 : -1;
    sy = 0 < h ? 1 : -1;
    er = dx - dy;

    finished = false;
}

bool DrawLine::nextPixel()
{
    if( !finished )
    {
        e2 = er * 2;
        if( e2 > -dy )
        {
            er -= dy;
            varX += sx;
        }
        if( e2 < dx )
        {
            er += dx;
            varY += sy;
        }

        if( varX == finalX && varY == finalY )
            finished = true;

        return true;
    }

    return false;
}

bool DrawLine::isFinished() const
{
    return finished;
}

int DrawLine::x() const
{
    return varX;
}

int DrawLine::y() const
{
    return varY;
}
