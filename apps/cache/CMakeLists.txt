
add_executable( testCacheVec  testCacheVec.cpp )
target_link_libraries( testCacheVec tinyperfstats ${REQUIRED_LIBS} Boost::container )

add_executable( testCacheSize testCacheSize.cpp )
target_link_libraries( testCacheSize ${REQUIRED_LIBS} )

add_executable( testBits testBits.cpp )