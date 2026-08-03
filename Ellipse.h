#pragma once

#include "Curve.h"

#include <queue>

class DrawEllipse : public DrawCurve
{
private:
    // Center and radii
    int cx, cy, rx, ry;

    int twoASq, twoBSq;

    int xPending, yPending;
    int xChange, yChange;
    int ellipseError;
    int stoppingX, stoppingY;
    int region; // 1 or 2

    bool started;  // Has initial point been queued?
    bool finished; // Have we emitted everything?

    // Current output pixel
    int currX, currY;

    // Buffer for up to four symmetric points
    std::queue<std::pair<int, int>> pending;

    // Fill `pending` with the symmetric points for ( x, y )
    void fillPending( int x, int y );

public:
    DrawEllipse( int cx, int cy, int rx, int ry );

    bool nextPixel() override;
    bool isFinished() const override;
    int x() const override;
    int y() const override;
};
