

add_subdirectory(aout)
add_subdirectory(asmp)

FILE(GLOB c_source_files *linux.c)

add_library(my_rh_audio STATIC ${c_source_files} )
#add_library(rh_audio SHARED ${c_source_files} )
#add_library(rh_audio SHARED "rh_audio_linux.c" )

install (TARGETS my_rh_audio DESTINATION lib/rockhopper)
#install (FILES ${asmp_cxx_header_files} asmp.h DESTINATION include/rockhopper)

target_link_libraries (my_rh_audio asmp aout bucket pthread rt)

