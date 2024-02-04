
add_executable( testMicroStats testMicroStats.cpp )
target_link_libraries( testMicroStats ${REQUIRED_LIBS} )

list( APPEND TARGETS testMicroStats )