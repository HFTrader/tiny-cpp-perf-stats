add_library( tinyperfstats STATIC Snapshot.cpp PerfCounter.cpp )
target_link_libraries( tinyperfstats armadillo )
