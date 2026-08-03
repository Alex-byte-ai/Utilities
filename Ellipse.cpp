#include "Ellipse.h"

// Helper: queue up the first‐quadrant point ( x, y ) into all needed symmetric pixels
void DrawEllipse::fillPending( int x, int y )
{
    // Clear any leftovers
    pending = {};

    // One point
    if( x == 0 && y == 0 )
    {
        pending.emplace( cx, cy );
    }

    // Two points on vertical axis
    else if( x == 0 )
    {
        pending.emplace( cx, cy + y );
        pending.emplace( cx, cy - y );
    }

    // Two points on horizontal axis
    else if( y == 0 )
    {
        pending.emplace( cx + x, cy );
        pending.emplace( cx - x, cy );
    }

    // The general four‐quadrant case
    else
    {
        pending.emplace( cx + x, cy + y );
        pending.emplace( cx - x, cy + y );
        pending.emplace( cx - x, cy - y );
        pending.emplace( cx + x, cy - y );
    }
}

DrawEllipse::DrawEllipse( int xCenter, int yCenter, int xRadius, int yRadius ) :
    cx( xCenter ), cy( yCenter ), rx( xRadius ), ry( yRadius ),
    twoASq( 2 * rx * rx ),
    twoBSq( 2 * ry * ry ),
    xPending( rx ), yPending( 0 ),
    xChange( ry * ry * ( 1 - 2 * rx ) ),
    yChange( rx * rx ),
    ellipseError( 0 ),
    stoppingX( twoBSq * rx ),
    stoppingY( 0 ),
    region( 1 ),
    started( false ),
    finished( false ),
    currX( 0 ), currY( 0 )
{
    nextPixel();
}

int DrawEllipse::x() const
{
    return currX;
}

int DrawEllipse::y() const
{
    return currY;
}

bool DrawEllipse::isFinished() const
{
    return finished;
}

bool DrawEllipse::nextPixel()
{
    if( finished )
        return false;

    // Seed the initial point
    if( !started )
    {
        fillPending( xPending, yPending );
        started = true;
    }

    // Emit any buffered symmetric pixels first
    if( !pending.empty() )
    {
        auto [xx, yy] = pending.front();
        pending.pop();
        currX = xx;
        currY = yy;
        return true;
    }

    // Advance the ellipse algorithm
    if( region == 1 )
    {
        if( stoppingX > stoppingY )
        {
            // Region 1 stepping
            ++yPending;
            stoppingY += twoASq;
            ellipseError += yChange;
            yChange += twoASq;

            if( ( 2 * ellipseError + xChange ) > 0 )
            {
                --xPending;
                stoppingX -= twoBSq;
                ellipseError += xChange;
                xChange += twoBSq;
            }
        }
        else
        {
            // Switch to Region 2
            region = 2;
            xPending = 0;
            yPending = ry;
            xChange = ry * ry;
            yChange = rx * rx * ( 1 - 2 * ry );
            ellipseError = 0;
            stoppingX = 0;
            stoppingY = twoASq * ry;
        }
    }
    else
    {
        if( stoppingX < stoppingY )
        {
            // Region 2 stepping
            ++xPending;
            stoppingX += twoBSq;
            ellipseError += xChange;
            xChange += twoBSq;

            if( ( 2 * ellipseError + yChange ) > 0 )
            {
                --yPending;
                stoppingY -= twoASq;
                ellipseError += yChange;
                yChange += twoASq;
            }
        }
        else
        {
            // Completed both regions
            finished = true;
            return false;
        }
    }

    // Buffer and emit the next symmetric pixel
    fillPending( xPending, yPending );
    auto [xx2, yy2] = pending.front();
    pending.pop();
    currX = xx2;
    currY = yy2;
    return true;
}
