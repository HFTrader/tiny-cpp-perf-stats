cmake_minimum_required( VERSION 3.13 )

###########################################################################
# Pre-project settings
###########################################################################

if ( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: release, debug, relwithdebinfo" )
endif()
set( TINYPERF_BUILD_TESTS ON CACHE BOOL "Builds tests" )

###########################################################################
# Project start
###########################################################################
project( tiny-perf-stats )
include( FindPkgConfig )

set( CMAKE_CXX_STANDARD 17 )

###########################################################################
# Dependencies
###########################################################################

find_package( Boost COMPONENTS container REQUIRED )
find_package( Armadillo REQUIRED )
pkg_check_modules( CairoMM cairomm-1.0 cairomm-png-1.0 cairomm-svg-1.0 )
set( REQUIRED_LIBS rt ${ARMADILLO_LIBRARY} ${LAPACK_LIBRARY} arpack )
include_directories( ${ARMADILLO_INCLUDE_DIR} )
if ( CairoMM_FOUND )
  add_definitions( -DCairoMM_FOUND )
  list( APPEND REQUIRED_LIBS ${CairoMM_LIBRARIES} )
  include_directories( ${CairoMM_INCLUDE_DIRS} )
endif()

###########################################################################
# Libraries and applications
###########################################################################

add_subdirectory( libs )
include_directories( libs )

set( TARGETS  tinyperfstats )

if ( TINYPERF_BUILD_TESTS )
  add_subdirectory( apps )
  list( APPEND TARGETS
    scheduler_lib  testScheduler
    testMemoryMap testTickerLookup testWordMap
    testMicroStats testCacheSize
    testDuffLoop )
endif()

install( TARGETS ${TARGETS}
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib
         RUNTIME DESTINATION bin
         PUBLIC_HEADER DESTINATION include )
