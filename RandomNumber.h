#pragma once

class RandomNumber
{
private:
    long long int z, a, m, q, r;
    void init();
    void next();
public:
    class IntegerInterval
    {
    private:
        long long int a, b;
    public:
        IntegerInterval( long long int start, long long int finish ) : a( start ), b( finish )
        {};

        friend RandomNumber;
    };

    class RealInterval
    {
    private:
        double a, b;
    public:
        RealInterval( double start, double finish ) : a( start ), b( finish )
        {};

        friend RandomNumber;
    };

    RandomNumber();
    RandomNumber( long long int seed );
    void setSeed( long long int seed );

    // Generates pseudo-random number in given interval, numbers from a to b, including a and b
    long long int getInteger( long long int start, long long int finish );
    double getReal( double a, double b );
    long long int getInteger( const IntegerInterval &i );
    double getReal( const RealInterval &i );
};
