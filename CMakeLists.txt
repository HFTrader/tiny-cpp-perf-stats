if ( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: release, debug, relwithdebinfo" FORCE )
endif()

project( tiny-perf-stats )
cmake_minimum_required( VERSION 3.2 )
include( FindPkgConfig )

find_package( Boost REQUIRED )
find_package( Armadillo REQUIRED )
pkg_check_modules( CairoMM cairomm-1.0 cairomm-png-1.0 cairomm-svg-1.0 )
set( REQUIRED_LIBS rt armadillo )
if ( CairoMM_FOUND )
  add_definitions( -DCairoMM_FOUND )
  list( APPEND REQUIRED_LIBS ${CairoMM_LIBRARIES} )
  include_directories( ${CairoMM_INCLUDE_DIRS} )
endif()

if ( UNIX )

add_definitions( -std=c++11 -march=native )

add_library( tinyperfstats SHARED Snapshot.cpp PerfCounter.cpp )
target_link_libraries( tinyperfstats armadillo )

add_library( scheduler_lib SHARED Event.cpp )
target_link_libraries( scheduler_lib tinyperfstats )

add_executable( scheduler  scheduler.cpp )
target_link_libraries( scheduler scheduler_lib ${REQUIRED_LIBS} )

add_executable( memorymap memorymap.cpp MMapFile.cpp )
target_link_libraries( memorymap tinyperfstats ${REQUIRED_LIBS} )

add_executable( cachevec  cachevec.cpp )
target_link_libraries( cachevec tinyperfstats ${REQUIRED_LIBS} )

add_executable( testMicroStats testMicroStats.cpp )
target_link_libraries( testMicroStats ${REQUIRED_LIBS} )

add_executable( testCacheSize testCacheSize.cpp )
target_link_libraries( testCacheSize ${REQUIRED_LIBS} )

else()
   message( FATAL_ERROR "Your cat fell from the roof..." )
endif()

install( TARGETS tinyperfstats scheduler_lib scheduler memorymap cachevec testMicroStats testCacheSize
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin )
