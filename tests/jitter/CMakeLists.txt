add_executable( jitter jitter.cpp )
target_link_libraries( jitter tinyperfstats pthread )

list( APPEND TARGETS jitter )