#pragma once

#include <optional>

class Complex
{
public:
    static double epsilon;

    double a, b;
    // a + ib

    Complex();
    Complex( double x, double y );

    Complex operator+() const;
    Complex operator+( const Complex &x ) const;
    Complex &operator+=( const Complex &x );

    Complex operator-() const;
    Complex operator-( const Complex &x ) const;
    Complex &operator-=( const Complex &x );

    Complex operator*( double k ) const;
    Complex &operator*=( double k );

    Complex operator/( double k ) const;
    Complex &operator/=( double k );

    Complex operator*( const Complex &x ) const;

    bool operator==( const Complex &x ) const;
    bool operator!=( const Complex &x ) const;

    double Abs() const;
    double Arg() const;

    Complex Normal() const;
};

Complex operator*( double k, const Complex &x );
