if ( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: release, debug, relwithdebinfo" FORCE )
endif()


project( tiny-perf-stats )
cmake_minimum_required( VERSION 3.2 )

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

add_executable( memorymap memorymap.cpp )
target_link_libraries( memorymap tinyperfstats rt )

else()
   message( FATAL_ERROR "Your cat fell from the roof..." )
endif()

install( TARGETS tinyperfstats scheduler_lib scheduler
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin )
