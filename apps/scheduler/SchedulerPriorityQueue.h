#pragma once

#include <cstdint>
#include <queue>
#include "Event.h"

class SchedulerPriorityQueue
{
public:
    SchedulerPriorityQueue() {};
    void schedule( uint64_t time, Event* evt, void* user ) {
        events.emplace( Item(evt,user,time) );
    }
    bool check( uint64_t time ) {
        bool found = false;
        while ( not events.empty() ) {
            const Item& it(  events.top() );
            if ( it.time > time ) break;
            found = true;
            it.event->exec( it.user );
            events.pop();
        }
        return found;
    }
private:
    struct Item {
        Item( Event* ev, void* data, uint64_t tm ) : event(ev), user(data), time(tm) {}
        inline bool operator < ( const Item& rhs ) const { return time>rhs.time; }
        Event* event; void* user; uint64_t time;
    };
    using EventMap = std::priority_queue< Item >;
    EventMap events;
};
