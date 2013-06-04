
# HACK!!!
ADD_DEFINITIONS(-fPIC)

FILE(GLOB c_source_files *.c)

add_library(aout_alsa STATIC ${c_source_files} )

target_link_libraries (aout_alsa asound)

