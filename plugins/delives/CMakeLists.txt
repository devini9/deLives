cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Widgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSECRET REQUIRED libsecret-1)

add_library(delives MODULE)
add_library(OBS::delives ALIAS delives)

target_sources(
  delives
  PRIVATE
    delives.c
    delives-dock.hpp
    delives-dock.cpp
)

target_include_directories(delives PRIVATE ${LIBSECRET_INCLUDE_DIRS})

target_link_libraries(
  delives
  PRIVATE
    OBS::frontend-api
    OBS::libobs
    Qt::Widgets
    ${LIBSECRET_LIBRARIES}
)

target_link_directories(delives PRIVATE ${LIBSECRET_LIBRARY_DIRS})
target_compile_options(delives PRIVATE ${LIBSECRET_CFLAGS_OTHER})

set_target_properties_obs(delives PROPERTIES FOLDER plugins/delives PREFIX "")
