set( LIBRARY_CPP_FILES Snapshot.cpp PerfGroup.cpp PerfUtils.cpp CpuUtils.cpp ) 
if ( HAVE_LIBPFM )
set( LIBRARY_DEPENDENCIES pfm )
endif()
if ( Armadillo_FOUND AND Boost_CONTAINER_FOUND ) 
  list( APPEND LIBRARY_CPP_FILES Regression.cpp )
  list( APPEND LIBRARY_DEPENDENCIES  armadillo Boost::container )
endif()
add_library( tinyperfstats SHARED  ${LIBRARY_CPP_FILES} )
target_link_libraries( tinyperfstats ${LIBRARY_DEPENDENCIES} )

set( HEADER_LIST Allocators.h BitUtils.h CpuUtils.h DateUtils.h Histogram.h KahanSum.h MicroStats.h PerfCounter.h Snapshot.h StringUtils.h Ticker.h TimingUtils.h Regression.h )
foreach( header ${HEADER_LIST} )
  list( APPEND ALLHEADERS "${CMAKE_CURRENT_SOURCE_DIR}/${header}" )
endforeach()

set_target_properties( tinyperfstats PROPERTIES 
  PUBLIC_HEADER "${ALLHEADERS}" ) 
target_include_directories( tinyperfstats PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" )
