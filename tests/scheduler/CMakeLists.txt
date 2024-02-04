add_library( scheduler_lib SHARED Event.cpp )
target_link_libraries( scheduler_lib tinyperfstats )

add_executable( testScheduler  testScheduler.cpp )
target_link_libraries( testScheduler scheduler_lib ${REQUIRED_LIBS} )

list( APPEND TARGETS 
    scheduler_lib  testScheduler
)