add_library( tinyperfstats STATIC Snapshot.cpp PerfGroup.cpp )
target_link_libraries( tinyperfstats armadillo pfm )

set( HEADER_LIST Allocators.h BitUtils.h DateUtils.h Histogram.h KahanSum.h MicroStats.h PerfCounter.h Snapshot.h StringUtils.h Ticker.h TimingHelpers.h )
foreach( HEADER ${HEADER_LIST} )
  file( REAL_PATH ${HEADER} ABSHEADER BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
  list( APPEND ALLHEADERS ${ABSHEADER} )
endforeach()
set_target_properties( tinyperfstats 
PROPERTIES 
  PUBLIC_HEADER "${ALLHEADERS}" 
  INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR} )

