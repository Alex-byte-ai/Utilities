#pragma once

#include "Curve.h"

class DrawLine : public DrawCurve
{
private:
    int varX, varY, finalX, finalY, w, h, dx, dy, sx, sy, er, e2;
    bool finished;
public:
    DrawLine();
    DrawLine( int x0, int y0, int x1, int y1 );

    bool nextPixel() override;
    bool isFinished() const override;
    int x() const override;
    int y() const override;
};
