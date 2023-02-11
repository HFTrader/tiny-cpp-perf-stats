#include <cstdint>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include "Snapshot.h"
#include "SelectiveCacheVector.h"

struct OrderBook {
    uint32_t count;
    uint8_t dummy[1024*128];
};

struct Ticker {
    uint32_t index;
    std::string ticker;
    uint64_t volume;
};

struct Event {
    uint32_t index;
    uint64_t time;
    uint32_t count;
};

using EventVec = std::vector< Event >;
using TickerVec = std::vector< Ticker >;
using TickerMap = std::map< std::string, uint32_t >;

//template< template <typename> class VectorType = std::vector >
template< typename VectorType = std::vector<OrderBook> >
void testme( Snapshot& snap, const EventVec& events, uint32_t numitems )
{
    VectorType vec( numitems );
    for ( OrderBook& ev : vec ) {
        ev.count = 0;
    }
    std::cout << ">> Run: " << numitems << " Events:" << events.size() << std::endl;
    snap.start();
    uint64_t counter = 0;
    for ( const Event& ev : events ) {
        vec[ ev.index ].count += 1;
        counter++;
    }
    snap.stop( "Traverse", numitems, events.size() );
    for ( OrderBook& ev : vec ) {
        counter -= ev.count;
    }
    if ( counter!=0 )
        std::cout << "Counter left:" << counter << std::endl;
}

bool split( const std::string& str, char separator,
               std::string& first, std::string& second ) {
    std::string::size_type p1 = str.find_first_of( separator );
    if ( p1==std::string::npos ) {
        return false;
    }
    std::string::size_type p2 = str.find_first_of( separator, p1+1 );
    if ( p2==std::string::npos ) {
        first = str.substr( 0, p1 );
        second = str.substr( p1+1 );
        return true;
    }
    first = str.substr( 0, p1 );
    second = str.substr( p1+1, p2-p1-1 );
    return true;
}

template< typename Fn >
void getTickers( const std::string& filename, Fn fn ) {
    std::cout << "Opening file " << filename << std::endl;
    std::ifstream ifs( filename );
    while ( ifs.good() ) {
        try {
            std::string line;
            std::getline( ifs, line, '\n' );
            //std::cout << line << std::endl;
            std::string ticker, svolume;
            if ( split( line, ',', ticker, svolume ) ) {
                //std::cout << "    " << ticker << ", " << svolume << std::endl;
                uint64_t volume = std::stol( svolume );
                fn( ticker, volume );
            }
        }
        catch( ... ) {
        }
    }
}


int main( int argc, char* argv[] )
{
    TickerVec tickers;
    auto tickproc = [&tickers](
        const std::string& ticker,
        uint64_t volume )
    {
        uint32_t index = tickers.size();
        Ticker tk;
        tk.index = index;
        tk.ticker = ticker;
        tk.volume = volume;
        tickers.push_back( tk );
    };
    getTickers( "Bats_Volume_2016-05-03.csv", tickproc );

    std::random_shuffle( tickers.begin(), tickers.end() );

    Snapshot snap(2);
    for ( uint32_t numitems : { 10, 50, 100, 250, 500, 1000, 2500, 5000 } ) {

        EventVec events;

        std::random_device rd;
        std::mt19937 gen(rd());
        gen.seed( time(NULL) );

        for ( const auto& tk : tickers ) {
            if ( tk.index>=numitems ) continue;
            Event ev;
            ev.index = tk.index;
            constexpr uint64_t ONEDAY = 86400000000ULL;
            uint64_t avgvolume = tk.volume/50;
            if ( avgvolume<=0 ) continue;
            uint64_t avgtime = ONEDAY/avgvolume;
            if ( avgtime<=0 ) continue;

            std::poisson_distribution<uint64_t> d(avgtime);
            for ( double tm = d(gen); tm<ONEDAY; tm += d(gen) ) {
                ev.time = tm;
                events.push_back( ev );
                //std::cout << " " << tm << "," << ticker << std::endl;
            }
            //std::cout << "." << std::flush;
        };
        std::cout << "\nSorting " << events.size() << " tickers..." << std::endl;
        std::sort( events.begin(), events.end(),
                   []( const Event& a, const Event& b )
                   { return a.time<b.time; } );

        testme< std::vector<OrderBook> >( snap, events, numitems );
    }
    snap.summary( "std::vector" );

    return 0;
}
