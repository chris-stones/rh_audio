
# HACK!!!
ADD_DEFINITIONS(-fPIC)

FILE(GLOB c_source_files *.c)

include_directories("${PROJECT_SOURCE_DIR}")

add_library(aout_embedded STATIC ${c_source_files} )

