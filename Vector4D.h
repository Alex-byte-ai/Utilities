#pragma once

class Vector4D
{
public:
    static double epsilon;

    double x, y, z, w;

    Vector4D();
    Vector4D( double x, double y, double z, double w );

    Vector4D operator+() const;
    Vector4D operator+( const Vector4D &a ) const;
    Vector4D &operator+=( const Vector4D &a );

    Vector4D operator-() const;
    Vector4D operator-( const Vector4D &a ) const;
    Vector4D &operator-=( const Vector4D &a );

    Vector4D operator*( double k ) const;
    Vector4D &operator*=( double k );

    Vector4D operator/( double k ) const;
    Vector4D &operator/=( double k );

    double operator*( const Vector4D &a ) const;

    bool operator==( const Vector4D &a ) const;
    bool operator!=( const Vector4D &a ) const;

    double Sqr() const;
    double Abs() const;

    Vector4D Normal() const;
    double Ang( const Vector4D &a ) const;
};

Vector4D operator*( double k, const Vector4D &a );
