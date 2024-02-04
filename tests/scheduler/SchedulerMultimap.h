#pragma once

#include <cstdint>
#include <queue>
#include <map>
#include "Event.h"

class SchedulerMultimap
{
public:
    SchedulerMultimap() {};
    void schedule( uint64_t time, Event* evt, void* user ) {
        events.emplace( time, Item(evt,user) );
    }
    bool check( uint64_t time ) {
        bool found = false;
        while ( not events.empty() ) {
            EventMap::iterator it = events.begin();
            if ( it->first > time ) break;
            found = true;
            it->second.event->exec( it->second.user );
            events.erase( it );
        }
        return found;
    }
private:
    struct Item {
        Item( Event* ev, void* data ) : event(ev), user(data) {}
        Event* event; void* user;
    };
    using EventMap = std::multimap< uint64_t, Item >;
    EventMap events;
};
