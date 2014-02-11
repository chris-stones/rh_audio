cmake_minimum_required(VERSION 2.8)

add_definitions(-Wall)

add_subdirectory(aout)
add_subdirectory(asmp)

FILE(GLOB c_source_files *.c)

add_library(rh_audio SHARED ${c_source_files} )

install (TARGETS rh_audio DESTINATION lib/)
install (FILES rh_audio.h DESTINATION include/)

set (LIBS asmp pthread rt)

if($ENV{RH_AOUT} STREQUAL "ALSA")
	set (LIBS ${LIBS} aout_alsa)
endif()

if($ENV{RH_AOUT} STREQUAL "OpenSLES")
	set (LIBS ${LIBS} aout_opensles)
endif()

if($ENV{RH_AOUT} STREQUAL "Embedded")
	set (LIBS ${LIBS} aout_embedded)
endif()

target_link_libraries (rh_audio ${LIBS})

add_executable(rh_audio_test test.c)
target_link_libraries(rh_audio_test rh_audio)


