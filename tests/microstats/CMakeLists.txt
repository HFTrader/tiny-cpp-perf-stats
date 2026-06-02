
add_executable( testMicroStats testMicroStats.cpp )
target_link_libraries( testMicroStats ${REQUIRED_LIBS} )

# Assertion-based regression test for the bin/range arithmetic. Header-only,
# no external dependencies, returns nonzero on failure so ctest catches it.
add_executable( testMicroStatsRange testMicroStatsRange.cpp )
add_test( NAME testMicroStatsRange COMMAND testMicroStatsRange )

list( APPEND TARGETS testMicroStats testMicroStatsRange )