#pragma once

#include "Vector4D.h"

class Matrix4D
{
public:
    static double epsilon;

    double a00, a01, a02, a03,
           a10, a11, a12, a13,
           a20, a21, a22, a23,
           a30, a31, a32, a33;

    Matrix4D();
    Matrix4D( double a00, double a01, double a02, double a03,
              double a10, double a11, double a12, double a13,
              double a20, double a21, double a22, double a23,
              double a30, double a31, double a32, double a33 );

    Matrix4D operator*( const Matrix4D &a ) const;
    Matrix4D &operator*=( const Matrix4D &a );

    Matrix4D operator*( double k ) const;
    Matrix4D &operator*=( double k );

    Vector4D operator*( const Vector4D& v ) const;

    Matrix4D operator/( double k ) const;
    Matrix4D &operator/=( double k );

    Matrix4D operator+() const;
    Matrix4D operator+( const Matrix4D &a ) const;
    Matrix4D &operator+=( const Matrix4D &a );

    Matrix4D operator-() const;
    Matrix4D operator-( const Matrix4D &a ) const;
    Matrix4D &operator-=( const Matrix4D &a );

    bool operator==( const Matrix4D &a ) const;
    bool operator!=( const Matrix4D &a ) const;

    double det() const;
    Matrix4D transpose() const;
    Matrix4D cofactor() const;
    Matrix4D adjugate() const;
    Matrix4D inv() const;

    static Matrix4D Zero();
    static Matrix4D Identity();
    static Matrix4D Perspective( double fovY, double aspect, double zNear, double zFar );
    static Matrix4D Orthographic( double left, double right, double bottom, double top, double zNear, double zFar );
};

Matrix4D operator*( double k, const Matrix4D &a );
