
cmake_minimum_required(VERSION 2.8)

add_subdirectory(aout)
add_subdirectory(asmp)

FILE(GLOB linux_c_source_files *linux.c)

add_library(rh_audio SHARED ${linux_c_source_files} rh_audio_common.c bucket.c)
#add_library(rh_audio SHARED ${c_source_files} )
#add_library(rh_audio SHARED "rh_audio_linux.c" )

install (TARGETS rh_audio DESTINATION lib/)
install (FILES rh_audio.h DESTINATION include/)

target_link_libraries (rh_audio asmp pthread rt -Wl,--start-group aout aout_alsa -Wl,--end-group)

add_executable(rh_audio_test test.c)
target_link_libraries(rh_audio_test rh_audio)


