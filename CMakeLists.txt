if ( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: release, debug, relwithdebinfo" FORCE )
endif()

project( tiny-perf-stats )
cmake_minimum_required( VERSION 3.2 )

find_package( Boost REQUIRED )
find_package( Armadillo REQUIRED )

if ( UNIX )

add_definitions( -std=c++11 )

add_library( tinyperfstats STATIC Snapshot.cpp PerfCounter.cpp )
target_link_libraries( tinyperfstats armadillo )

add_library( scheduler_lib SHARED Event.cpp )
target_link_libraries( scheduler_lib tinyperfstats )

add_executable( testScheduler  testScheduler.cpp )
target_link_libraries( testScheduler scheduler_lib rt )

add_executable( testMemoryMap testMemoryMap.cpp MMapFile.cpp )
target_link_libraries( testMemoryMap tinyperfstats rt )

add_executable( testCacheVec  testCacheVec.cpp )
target_link_libraries( testCacheVec tinyperfstats rt )

add_executable( testMicroStats testMicroStats.cpp )
target_link_libraries( testMicroStats rt )

add_executable( testCacheSize testCacheSize.cpp )
target_link_libraries( testCacheSize tinyperfstats rt )

else()
   message( FATAL_ERROR "Your cat fell from the roof..." )
endif()

install( TARGETS tinyperfstats scheduler_lib testScheduler testMemoryMap testCacheVec
  testMicroStats testCacheSize
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin )
