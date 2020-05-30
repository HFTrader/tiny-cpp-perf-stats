#pragma once

#include "KahanSum.h"

#include <cstdint>
#include <cmath>
#include <iostream>
#include <limits>
#include <array>


static inline double invcdf( double p )
{
    auto approx = []( double t ) {
        // Abramowitz and Stegun formula 26.2.23.
        // The absolute value of the error should be less than 4.5 e-4.
        double c[] = {2.515517, 0.802853, 0.010328};
        double d[] = {1.432788, 0.189269, 0.001308};
        return t - ((c[2]*t + c[1])*t + c[0]) /
            (((d[2]*t + d[1])*t + d[0])*t + 1.0);
    };
    if ( p<0 )
        return -std::numeric_limits<double>::infinity();
    if ( p>1 )
        return std::numeric_limits<double>::infinity();
    if ( p < 0.5 )
        return -approx( std::sqrt( -2.0* std::log( p ) ) );
    return approx( std::sqrt( -2.0* std::log( 1-p ) ) );
}

template< std::uint32_t NUMBINS >
class Histogram {
public:
    Histogram( double min, double max ) :_min(min),_max(max) { clear(); }
    void clear() {
        for ( Bin& bin : _bins ) {
            bin.sum.clear();
            bin.sum2.clear();
            bin.count = 0;
        }
    }
    void add( double value, std::uint64_t count = 1 ) {
        std::int64_t idx = 0;
        if ( value<_min ) idx = 0;
        else if ( value>=_max ) idx = NUMBINS-1;
        else idx = ((value-_min)/(_max-_min))*NUMBINS;
        Bin& bin( _bins[idx] );
        bin.sum.add( value );
        bin.sum2.add( value*value );
        bin.count += count;
    }
    double limit( std::uint32_t idx ) const {
        return idx*(_max-_min)/NUMBINS + _min;
    }
    double pct( double percent ) const {
        std::uint64_t total = 0;
        for ( const Bin& bin : _bins ) {
            total += bin.count;
        }
        if ( total==0 ) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        double pctval = ( percent * total )/ 100;
        std::uint64_t count = 0;
        for ( std::uint32_t j=0; j<_bins.size(); ++j ) {
            const Bin& bin( _bins[j] );
            if ( bin.count==0 ) continue;
            if ( count + bin.count >= pctval ) {
                double excess = pctval - count;
                double ratio = excess/bin.count;
                double avg = bin.sum()/bin.count;
                double avg2 = bin.sum2()/bin.count;
                double stdev = ::sqrt( avg2 - avg*avg );
                double sigma = invcdf( ratio );
                double pctx = avg + sigma * stdev;
                if ( pctx < limit(j) ) return limit(j);
                if ( pctx > limit(j+1) ) return limit(j+1);
                return pctx;
                //return ratio*limit(j+1) + (1-ratio)*limit(j);
            }
            count += bin.count;
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
    friend std::ostream& operator << ( std::ostream& oss, const Histogram<NUMBINS>& h ) {
        h.print( oss );
        return oss;
    }
    void print( std::ostream& oss ) const {
        for ( double v : { 10, 25, 50, 75, 90, 95, 99 } ) {
            oss << v << "%," << pct(v) << ",";
        }
    }

private:
    struct Bin {
        KahanSum<double> sum,sum2;
        std::uint64_t count;
    };
    std::array< Bin, NUMBINS > _bins;
    double _min;
    double _max;
};
