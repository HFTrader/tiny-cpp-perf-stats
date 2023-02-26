add_library( tinyperfstats STATIC Snapshot.cpp PerfCounter.cpp )
target_link_libraries( tinyperfstats armadillo )

if ( TINYPERF_BUILD_TESTS )
  add_subdirectory( datasets )
endif()
