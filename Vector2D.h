#pragma once

#include <optional>

class Vector2D
{
public:
    static double epsilon;

    double x, y;

    Vector2D();
    Vector2D( double x, double y );

    Vector2D operator+() const;
    Vector2D operator+( const Vector2D &a ) const;
    Vector2D &operator+=( const Vector2D &a );

    Vector2D operator-() const;
    Vector2D operator-( const Vector2D &a ) const;
    Vector2D &operator-=( const Vector2D &a );

    Vector2D operator*( double k ) const;
    Vector2D &operator*=( double k );

    Vector2D operator/( double k ) const;
    Vector2D &operator/=( double k );

    double operator*( const Vector2D &a ) const;

    bool operator==( const Vector2D &a ) const;
    bool operator!=( const Vector2D &a ) const;

    double M( const Vector2D &a ) const;
    Vector2D L() const;

    double Sqr() const;
    double Abs() const;

    Vector2D Normal() const;
    double Ang( const Vector2D &a ) const;

    std::optional<Vector2D> IJ( const Vector2D &i, const Vector2D &j ) const;
};

Vector2D operator*( double k, const Vector2D &a );
