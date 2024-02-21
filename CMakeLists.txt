cmake_minimum_required( VERSION 3.13 )

###########################################################################
# Pre-project settings
###########################################################################

if ( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: release, debug, relwithdebinfo" )
endif()
set( TINYPERF_BUILD_TESTS OFF CACHE BOOL "Builds tests" )
set( TINYPERF_ENABLE_LIBPFM ON CACHE BOOL "Enable libpfm" )

###########################################################################
# Project start
###########################################################################
project( tiny-perf-stats )

set( CMAKE_CXX_STANDARD 17 )

###########################################################################
# Dependencies
###########################################################################
list( APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake )
include( FindPkgConfig )
find_package( Boost COMPONENTS container )
if ( Boost_CONTAINER_FOUND ) 
  add_definitions( -DBOOST_CONTAINER_FOUND )
endif()

if ( TINYPERF_ENABLE_LIBPFM )
find_package( Libpfm ) 
if ( HAVE_LIBPFM ) 
  add_definitions(-DLIBPFM_FOUND)
else()
  message( "LIBPFM not found. Only basic events will be available.")
endif()
endif()

find_package( Armadillo )
if ( Armadillo_FOUND )
  add_definitions( -DARMADILLO_FOUND )
  include_directories( ${ARMADILLO_INCLUDE_DIR} )
  set( REQUIRED_LIBS rt ${ARMADILLO_LIBRARY} ${LAPACK_LIBRARY} arpack )
endif()

pkg_check_modules( CairoMM cairomm-1.0 cairomm-png-1.0 cairomm-svg-1.0 )
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

if ( TINYPERF_BUILD_TESTS AND Armadillo_FOUND )
  add_subdirectory( datasets )
  add_subdirectory( tests )
endif()

install( TARGETS ${TARGETS}
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib
         RUNTIME DESTINATION bin
         PUBLIC_HEADER DESTINATION include )
