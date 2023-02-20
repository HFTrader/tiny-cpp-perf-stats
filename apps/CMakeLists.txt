add_subdirectory( scheduler )
add_subdirectory( memorymap )
add_subdirectory( cache )
add_subdirectory( duffloop )

add_executable( testMicroStats testMicroStats.cpp )
target_link_libraries( testMicroStats ${REQUIRED_LIBS} )

