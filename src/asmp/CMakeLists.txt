add_definitions(-fPIC)

FILE(GLOB asmp_c_source_files *.c)
FILE(GLOB asmp_cxx_header_files *.hpp)

add_library(asmp STATIC ${asmp_c_source_files} )

#install (TARGETS asmp DESTINATION lib/rockhopper)
#install (FILES ${asmp_cxx_header_files} asmp.h DESTINATION include/rockhopper)

target_link_libraries (asmp sndfile pthread rt)

