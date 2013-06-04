
add_subdirectory(alsa)

FILE(GLOB c_source_files *.c)

add_library(aout SHARED ${c_source_files} )

install (TARGETS aout  DESTINATION lib/rockhopper)

target_link_libraries (aout aout_alsa pthread rt)
