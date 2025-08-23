#pragma once

#include "Matrix2D.h"
#include "Vector2D.h"

class Affine2D
{
public:
    Matrix2D t;
    Vector2D s;

    Affine2D();
    Affine2D( const Affine2D &a );

    Affine2D( const Vector2D &shift );
    Affine2D( const Matrix2D &transformation );
    Affine2D( const Matrix2D &transformation, const Vector2D &shift );

    Affine2D &operator=( const Affine2D &other );

    Affine2D operator*( const Affine2D &a ) const;
    Affine2D &operator*=( const Affine2D &a );

    Vector2D operator()( const Vector2D &a ) const;

    bool operator==( const Affine2D &a ) const;
    bool operator!=( const Affine2D &a ) const;

    Affine2D inv() const;
};

Vector2D operator*( const Matrix2D &a, const Vector2D &b );
