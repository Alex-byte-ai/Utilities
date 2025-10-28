#pragma once

#include "Matrix3D.h"
#include "Vector3D.h"

class Affine3D
{
public:
    Matrix3D t;
    Vector3D s;

    Affine3D();
    Affine3D( const Affine3D &other );

    Affine3D( const Vector3D &shift );
    Affine3D( const Matrix3D &transformation );
    Affine3D( const Matrix3D &transformation, const Vector3D &shift );

    Affine3D &operator=( const Affine3D &other );

    Affine3D operator*( const Affine3D &a ) const;
    Affine3D &operator*=( const Affine3D &a );

    Vector3D operator()( const Vector3D &a ) const;

    bool operator==( const Affine3D &a ) const;
    bool operator!=( const Affine3D &a ) const;

    Affine3D inv() const;
};
