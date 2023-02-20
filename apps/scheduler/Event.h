#pragma once
#include <vector>
#include <cstdint>

class Event
{
public:
    virtual ~Event();
    virtual void exec( void* data ) =0;
};

void createEvents( uint32_t numevents,
                   uint32_t numclasses,
                   std::vector<Event*>& events );
