if ( Boost_CONTAINER_FOUND )

add_executable( testTickerLookup  testTickerLookup.cpp )
target_link_libraries( testTickerLookup tinyperfstats ${REQUIRED_LIBS} Boost::container datasets )

add_executable( testWordMap  testWordMap.cpp )
target_link_libraries( testWordMap tinyperfstats ${REQUIRED_LIBS} Boost::container datasets )

list( APPEND TARGETS testTickerLookup testWordMap )

endif()

add_executable( testCacheSize testCacheSize.cpp )
target_link_libraries( testCacheSize ${REQUIRED_LIBS} )

add_executable( testBits testBits.cpp )

list( APPEND TARGETS testCacheSize testBits )
