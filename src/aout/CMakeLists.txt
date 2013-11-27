
add_definitions(-fPIC)

add_subdirectory(alsa)
add_subdirectory(OpenSLES)

#FILE(GLOB c_source_files *.c)
#add_library(aout STATIC ${c_source_files} )
#install (TARGETS aout  DESTINATION lib/rockhopper)
#target_link_libraries (aout pthread rt)
