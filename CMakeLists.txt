cmake_minimum_required(VERSION 3.5)

project(dwmstatus)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

list(APPEND LIBS m)

find_package(X11 REQUIRED)
include_directories(${X11_INCLUDE_DIRS})
list(APPEND LIBS ${X11_LIBRARIES})

find_package(ALSA REQUIRED)
include_directories(${ALSA_INCLUDE_DIRS})
list(APPEND LIBS ${ALSA_LIBRARIES})

if (USE_MPD)
	find_package(LibMPDClient REQUIRED)
	include_directories(${LIBMPDCLIENT_INCLUDE_DIR})
	list(APPEND LIBS ${LIBMPDCLIENT_LIBRARY})
	add_definitions(-DUSE_MPD)
endif(USE_MPD)

set(SOURCES
	dwmstatus.cxx
)

set(CMAKE_CXX_FLAGS
	"${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wpedantic -Werror")
set(CMAKE_CXX_FLAGS
	"${CMAKE_CXX_FLAGS} -O3")

add_executable(dwmstatus ${SOURCES})
target_link_libraries(dwmstatus ${LIBS})

install(TARGETS dwmstatus DESTINATION bin)
