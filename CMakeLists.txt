cmake_minimum_required(VERSION 3.16)
project(OpenHover LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

# Hashing, decompression and wire-format helpers: standard library only.
add_library(openhover_util STATIC src/util/Sha256.cpp src/util/Gunzip.cpp)
target_include_directories(openhover_util PUBLIC src/util)

# Track listing, name safety, and verified on-demand downloads.
add_library(openhover_content STATIC src/content/TrackCatalog.cpp src/content/TrackDownloader.cpp)
target_include_directories(openhover_content PUBLIC src/content)
target_link_libraries(openhover_content PUBLIC openhover_util Threads::Threads)

if(BUILD_TESTING AND NOT WIN32)
    add_executable(TrackCatalogSmoke tests/TrackCatalogSmoke.cpp)
    target_link_libraries(TrackCatalogSmoke PRIVATE openhover_content)
    add_test(NAME TrackCatalogSmoke COMMAND TrackCatalogSmoke)

    add_executable(TrackDownloadSmoke tests/TrackDownloadSmoke.cpp)
    target_link_libraries(TrackDownloadSmoke PRIVATE openhover_content)
    add_test(NAME TrackDownloadSmoke COMMAND TrackDownloadSmoke)
endif()
