

add_subdirectory(aout)
add_subdirectory(asmp)

FILE(GLOB c_source_files *.c)

add_library(rh_audio SHARED ${c_source_files} )

install (TARGETS rh_audio DESTINATION lib/)
install (FILES rh_audio.h DESTINATION include/)

target_link_libraries (rh_audio asmp pthread rt -Wl,--start-group aout aout_alsa -Wl,--end-group)

add_executable(rh_audio_test test.c)
target_link_libraries(rh_audio_test rh_audio)


