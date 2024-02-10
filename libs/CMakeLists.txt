set( LIBRARY_CPP_FILES Snapshot.cpp PerfGroup.cpp CpuUtils.cpp ) 
set( LIBRARY_DEPENDENCIES pfm )
if ( Armadillo_FOUND ) 
  list( APPEND LIBRARY_CPP_FILES Regression.cpp )
  list( APPEND LIBRARY_DEPENDENCIES  armadillo )
endif()
add_library( tinyperfstats SHARED  ${LIBRARY_CPP_FILES} )
target_link_libraries( tinyperfstats ${LIBRARY_DEPENDENCIES} )

set( HEADER_LIST Allocators.h BitUtils.h CpuUtils.h DateUtils.h Histogram.h KahanSum.h MicroStats.h PerfCounter.h Snapshot.h StringUtils.h Ticker.h TimingUtils.h Regression.h )
foreach( HEADER ${HEADER_LIST} )
  file( REAL_PATH ${HEADER} ABSHEADER BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
  list( APPEND ALLHEADERS ${ABSHEADER} )
endforeach()

set_target_properties( tinyperfstats PROPERTIES 
  PUBLIC_HEADER "${ALLHEADERS}" ) 
target_include_directories( tinyperfstats PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" )