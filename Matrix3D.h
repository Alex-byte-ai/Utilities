#pragma once

#include "Vector3D.h"

class Matrix3D
{
public:
    static double epsilon;

    double a00, a01, a02,
           a10, a11, a12,
           a20, a21, a22;

    Matrix3D();
    Matrix3D( double a00, double a01, double a02, double a10, double a11, double a12, double a20, double a21, double a22 );

    Matrix3D operator*( const Matrix3D &a ) const;
    Matrix3D &operator*=( const Matrix3D &a );

    Matrix3D operator*( double k ) const;
    Matrix3D &operator*=( double k );

    Vector3D operator*( const Vector3D& v ) const;

    Matrix3D operator/( double k ) const;
    Matrix3D &operator/=( double k );

    Matrix3D operator+() const;
    Matrix3D operator+( const Matrix3D &a ) const;
    Matrix3D &operator+=( const Matrix3D &a );

    Matrix3D operator-() const;
    Matrix3D operator-( const Matrix3D &a ) const;
    Matrix3D &operator-=( const Matrix3D &a );

    bool operator==( const Matrix3D &a ) const;
    bool operator!=( const Matrix3D &a ) const;

    double det() const;
    Matrix3D transpose() const;
    Matrix3D cofactor() const;
    Matrix3D adjugate() const;
    Matrix3D inv() const;

    static Matrix3D Zero();
    static Matrix3D Identity();
    static Matrix3D Scale( double s );
    static Matrix3D Scale( double xs, double ys, double zs );
    static Matrix3D Rotation( const Vector3D &axis, double angle );
};

Matrix3D operator*( double k, const Matrix3D &a );
