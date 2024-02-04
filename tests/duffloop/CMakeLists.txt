add_library( DuffLoopHelpers DuffLoopHelpers.cpp )
add_executable( testDuffLoop testDuffLoop.cpp )
target_link_libraries( testDuffLoop ${REQUIRED_LIBS} DuffLoopHelpers )

list( APPEND TARGETS testDuffLoop )
