
add_executable( testMemoryMap testMemoryMap.cpp MMapFile.cpp )
target_link_libraries( testMemoryMap tinyperfstats ${REQUIRED_LIBS} )