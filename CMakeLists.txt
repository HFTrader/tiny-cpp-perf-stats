project( tiny-perf-stats )
cmake_minimum_required( VERSION 2.8 )

find_package( Boost REQUIRED )
find_package( Armadillo REQUIRED )

if ( UNIX )

add_definitions( -std=c++11 )

add_library( tinyperfstats SHARED Snapshot.cpp PerfCounter.cpp )
target_link_libraries( tinyperfstats armadillo )

add_library( scheduler_lib SHARED Event.cpp )
target_link_libraries( scheduler_lib tinyperfstats )

add_executable( scheduler  scheduler.cpp )
target_link_libraries( scheduler scheduler_lib rt )

else()
   message( FATAL_ERROR "Your cat fell from the roof..." )
endif()
