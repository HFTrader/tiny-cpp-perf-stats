#include "Event.h"
#include <random>
#include <type_traits>

Event::~Event() {}



template< unsigned N >
class DerivedEvent : public Event
{
public:
    virtual void exec( void* data ) override {
        uint64_t* val = static_cast<uint64_t*>( data );
        *val += N;
    }
};

template< uint32_t N1, uint32_t N2 > Event* createOneEvent( uint32_t n );

template< uint32_t N1, uint32_t N2 > Event* createOneEvent( uint32_t n, std::false_type ) {
    constexpr uint32_t NMID = (N1+N2)/2;
    if ( N1==NMID ) return new DerivedEvent<NMID>();
    if ( n < NMID ) return createOneEvent< N1, NMID >( n );
    if ( n > NMID ) return createOneEvent< NMID, N2 >( n );
    return new DerivedEvent<NMID>();
}

template< uint32_t N1, uint32_t N2 > Event* createOneEvent( uint32_t n, std::true_type ) {
    return new DerivedEvent<N1>();
}

template< uint32_t N1, uint32_t N2 > Event* createOneEvent( uint32_t n ) {
    return createOneEvent<N1,N2 >( n, std::integral_constant<bool,N1==N2>() );
}




void createEvents( uint32_t numevents,
                   uint32_t numclasses,
                   std::vector<Event*>& events )
{
    events.resize( numevents );
    std::uniform_int_distribution<uint64_t> d(0,numclasses);
    std::mt19937 gen;

    for ( unsigned j=0; j<numevents; ++j ) {
        uint32_t index = d(gen);
        events[j] = createOneEvent<0,100>( index );
    }
}
