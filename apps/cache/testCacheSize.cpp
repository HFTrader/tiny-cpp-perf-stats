
#include "TimingHelpers.h"
#include "Histogram.h"
#include <x86intrin.h>

#include <cstdlib>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>


std::string str( std::uint32_t size ) {
    char buf[64];
    if ( size < 1<<10 ) {
        int nb = sprintf( buf, "%d", size );
        return std::string( buf, nb );
    }
    if ( size < 1<<20 ) {
        int nb = sprintf( buf, "%.1fk", double(size)/1024 );
        return std::string( buf, nb );
    }
    if ( size < 1<<30 ) {
        int nb = sprintf( buf, "%.1fM", double(size)/(1<<20) );
        return std::string( buf, nb );
    }
    int nb = sprintf( buf, "%.1fG", double(size)/(1<<30) );
    return std::string( buf, nb );
}

template< typename T >
void fillStridePattern( std::vector<T>& v, std::uint32_t stride ) {
    std::uint32_t pos = 0;
    for ( std::uint32_t j=0; j<v.size(); ++j ) {
        std::uint32_t newpos = pos + stride;
        if ( newpos >= v.size() ) newpos = (newpos+1) % stride;
        v[pos] = newpos;
        pos = newpos;
    }
}

template< typename T >
void fillRandomPattern( std::vector<T>& v, std::uint32_t stride ) {
    std::random_device rd;
    std::mt19937 gen(rd());
    gen.seed( time(NULL) );

    std::uniform_int_distribution<T> dis(0,v.size()-1);
    std::uint32_t pos = 0;
    for ( std::uint32_t j=0; j<v.size(); ++j ) {
        std::uint32_t newpos = dis(gen);
        while ( ((newpos>=pos) && (newpos<=pos+stride)) || ((newpos<=pos)&&(newpos>=pos-stride)) ) {
            newpos = dis(gen);
        }
        v[pos] = newpos;
        pos = newpos;
    }
}

struct Axis {
    std::string name;
    std::vector< std::uint32_t > points;
};

struct Results {
    std::vector< double > values;
    std::vector< Axis > axis;
};


#ifdef CairoMM_FOUND
#include <cairommconfig.h>
#include <cairomm/context.h>
#include <cairomm/surface.h>
#endif

#ifdef CAIRO_HAS_PNG_FUNCTIONS

struct Pixel {
    std::uint8_t r,g,b,a;
    operator std::uint32_t () {
        return std::uint32_t(r) | (std::uint32_t(g)<<8) | (std::uint32_t(b)<<16) | (std::uint32_t(a)<<24);
    }
};

struct ColorMap {
    ColorMap( double min, double max ) :_min(min), _max(max) {}
    Pixel operator()( double value ) {
        double intensity = (value-_min)/(_max-_min);
        if ( intensity<0 ) intensity = 0;
        if ( intensity>1 ) intensity = 1;
        std::uint32_t r = 256*intensity;
        if ( r>255 ) r = 255;
        Pixel px;
        px.r = r;
        px.g = r;
        px.b = r;
        px.a = 0xFF;
        return px;
    }
    double _min, _max;
};

template< typename Policy >
void saveGraphs( const std::vector<uint32_t>& sizes,
                 const std::vector<uint32_t>& strides,
                 const std::vector<uint32_t>& values )
{
    std::uint32_t width = sizes.size(); // sizes
    std::uint32_t height = strides.size(); // strides
    std::uint32_t numgraphs = Policy::Percentiles.size(); // percent

    for ( std::uint32_t np = 0; np<numgraphs; ++np ) {
        ColorMap colormap(0,500);
        std::vector< std::uint32_t > pixels( width*height );
        for ( std::uint32_t row = 0; row<height; ++row ) {
            for ( std::uint32_t col = 0; col<width; ++col ) {
                double value = values[ (row*width + col)*numgraphs + np ];
                pixels[row*width+col] = colormap( value );
            }
        }
        int pixstride = Cairo::ImageSurface::format_stride_for_width( Cairo::FORMAT_ARGB32, height );
        Cairo::RefPtr<Cairo::ImageSurface> surface =
            Cairo::ImageSurface::create( (std::uint8_t*)pixels.data(), Cairo::FORMAT_ARGB32, height, width, pixstride );

        char filename[256];
        ::sprintf( filename, "%s_pct%02d.png", Policy::Name, Policy::Percentiles[np] );
        surface->write_to_png(filename);
        std::cout << "Wrote png file \"" << filename << "\"" << '\n';
    }
}

#endif


struct LogGenerator {
    LogGenerator( uint32_t start, double ratio ) : _counter(start), _ratio(ratio) {}
    std::uint32_t operator()() { std::uint32_t tmp = _counter; _counter = (_counter*_ratio); return tmp; }
    double _counter;
    double _ratio;
};

std::vector<uint32_t> genLogRange( uint32_t start, uint32_t finish, double ratio ) {
    std::vector<uint32_t> result;
    uint32_t value = start;
    while ( value <= finish ) {
        result.push_back( value );
        uint32_t newvalue = double(value)*ratio;
        if ( newvalue==value ) newvalue++;
        value = newvalue;
    }
    return result;
}

enum FillPattern : std::uint8_t {
    Sequential,
        Random
        };

template< typename Policy >
void calcResults()
{
    const std::vector< uint32_t > sizes = genLogRange( Policy::SizeRange[0], Policy::SizeRange[1], Policy::SizeRatio );
    const std::vector< uint32_t > strides = genLogRange( Policy::StrideRange[0], Policy::StrideRange[1], Policy::StrideRatio );
    std::vector< uint32_t > values( sizes.size()*strides.size()*Policy::Percentiles.size() );

    std::uint32_t idx = 0;
    std::uint32_t pos = 0;
    std::uint32_t sum = 0;
    for ( std::uint32_t size : sizes )
    {
        for ( std::uint32_t stride : strides )
        {
            Histogram<Policy::HistogramBins> hist(Policy::HistogramRange[0],Policy::HistogramRange[1]);
            hist.clear();
            if ( size > 2*stride ) {
                std::vector<std::uint32_t> vpos( size );
                std::iota( vpos.begin(), vpos.end(), 0 );
                if ( Policy::Pattern == FillPattern::Sequential ) {
                    fillStridePattern( vpos, stride );
                } else if ( Policy::Pattern == FillPattern::Random ) {
                    fillRandomPattern( vpos, stride );
                }
                std::uint32_t nloops = std::max(1U,Policy::MAXLOOPS/size);

                pos = 0;
                double elapsed = 0;
                while ( elapsed<Policy::MaxTime ) {
                    std::uint32_t maxsize = std::min(size,Policy::MAXLOOPS);
                    elapsed += timeit( hist, [&sum,&maxsize,&pos,&vpos]() {
                            for ( std::uint32_t j=0; j<maxsize; ++j ) {
                                std::uint32_t tmp = vpos[pos];
                                if ( Policy::ReadWrite ) vpos[pos] = pos;
                                pos = tmp;
                            }
                            return maxsize;
                        });
                }
                DoNotOptimize( pos );
            }
            std::cout << "Policy," << Policy::Name << ",Stride," << stride << ",Size," << size << ",";
            for ( std::uint32_t pct : Policy::Percentiles ) {
                double val = hist.pct( pct );
                std::cout << pct << "," << val << ",";
                values[ idx++ ] = val;
            }
            std::cout << '\n';
        }
    }
#ifdef CAIRO_HAS_PNG_FUNCTIONS
    saveGraphs<Policy>( sizes, strides, values );
#endif
}

struct Defaults {
    static constexpr uint32_t MAXLOOPS = 1ULL << 24;
    static constexpr double   MaxTime = 0.5E9; /*cycles*/
    static constexpr uint32_t HistogramBins = 100;
    static constexpr uint32_t HistogramRange[2]  = {0,1000};
    static constexpr uint32_t StrideRange[2] = {1,32000000};
    static constexpr double   StrideRatio = 4/3.;
    static constexpr uint32_t SizeRange[2] = {8,1000000000};
    static constexpr double   SizeRatio = 4/3.;
    static const     std::array<uint32_t,5> Percentiles;
};

const std::array<uint32_t,5> Defaults::Percentiles = { 1, 10, 50, 75, 99 };

struct SequentialReadOnly : public Defaults {
    static constexpr const char* Name  = "SequentialReadOnly";
    static constexpr bool ReadWrite = false;
    static constexpr FillPattern Pattern = FillPattern::Sequential;
};
struct SequentialReadWrite  : public Defaults {
    static constexpr const char* Name = "SequentialReadWrite";
    static constexpr bool ReadWrite = true;
    static constexpr FillPattern Pattern = FillPattern::Sequential;
};
struct RandomReadOnly  : public Defaults {
    static constexpr const char* Name = "RandomReadOnly";
    static constexpr bool ReadWrite = false;
    static constexpr FillPattern Pattern = FillPattern::Random;
};
struct RandomReadWrite  : public Defaults {
    static constexpr const char* Name = "RandomReadWrite";
    static constexpr bool ReadWrite = true;
    static constexpr FillPattern Pattern = FillPattern::Random;
};



int main( int argc, char* argv[] )
{
    calcResults<SequentialReadOnly>();
    calcResults<SequentialReadWrite>();
    calcResults<RandomReadOnly>();
    calcResults<RandomReadWrite>();

    return 0;
}
