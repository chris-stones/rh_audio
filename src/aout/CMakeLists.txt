
add_definitions(-fPIC)

if($ENV{RH_AOUT} STREQUAL "ALSA")
	add_subdirectory(alsa)
endif()

if($ENV{RH_AOUT} STREQUAL "OpenSLES")
	add_subdirectory(OpenSLES)
endif()

if($ENV{RH_AOUT} STREQUAL "Embedded")
	add_subdirectory(embedded)
endif()

