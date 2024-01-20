add_library( tinyperfstats STATIC Snapshot.cpp PerfGroup.cpp )
target_link_libraries( tinyperfstats armadillo pfm )

add_subdirectory( datasets )