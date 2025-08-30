#pragma once

#include <optional>

class Vector3D
{
public:
    static double epsilon;

    double x, y, z;

    Vector3D();
    Vector3D( double x, double y, double z );

    Vector3D operator+() const;
    Vector3D operator+( const Vector3D &a ) const;
    Vector3D &operator+=( const Vector3D &a );

    Vector3D operator-() const;
    Vector3D operator-( const Vector3D &a ) const;
    Vector3D &operator-=( const Vector3D &a );

    Vector3D operator*( double k ) const;
    Vector3D &operator*=( double k );

    Vector3D operator/( double k ) const;
    Vector3D &operator/=( double k );

    double operator*( const Vector3D &a ) const;

    bool operator==( const Vector3D &a ) const;
    bool operator!=( const Vector3D &a ) const;

    Vector3D M( const Vector3D &a ) const;

    double Sqr() const;
    double Abs() const;

    Vector3D Normal() const;
    double Ang( const Vector3D &a ) const;

    std::optional<Vector3D> IJK( const Vector3D &i, const Vector3D &j, const Vector3D &k ) const;
};

Vector3D operator*( double k, const Vector3D &a );
