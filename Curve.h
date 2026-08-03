#pragma once

class DrawCurve
{
public:
    virtual bool nextPixel() = 0;
    virtual bool isFinished() const = 0;
    virtual int x() const = 0;
    virtual int y() const = 0;

    virtual ~DrawCurve() {}
};
