#pragma once

#include "Vector2D.h"

class Matrix2D
{
public:
    static double epsilon;

    double a00, a01,
           a10, a11;

    Matrix2D();
    Matrix2D( double a00, double a01, double a10, double a11 );

    Matrix2D operator*( const Matrix2D &a ) const;
    Matrix2D &operator*=( const Matrix2D &a );

    Matrix2D operator*( double k ) const;
    Matrix2D &operator*=( double k );

    Matrix2D operator/( double k ) const;
    Matrix2D &operator/=( double k );

    Matrix2D operator+() const;
    Matrix2D operator+( const Matrix2D &a ) const;
    Matrix2D &operator+=( const Matrix2D &a );

    Matrix2D operator-() const;
    Matrix2D operator-( const Matrix2D &a ) const;
    Matrix2D &operator-=( const Matrix2D &a );

    bool operator==( const Matrix2D &a ) const;
    bool operator!=( const Matrix2D &a ) const;

    double det() const;
    Matrix2D transpose() const;
    Matrix2D cofactor() const;
    Matrix2D adjugate() const;
    Matrix2D inv() const;

    static Matrix2D Zero();
    static Matrix2D Identity();
    static Matrix2D Scale( double s );
    static Matrix2D Scale( double xs, double ys );
    static Matrix2D Rotation( double angle );
};

Matrix2D operator*( double k, const Matrix2D &a );
