# SPDX-License-Identifier: MIT OR Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(OpenHover LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)
find_package(SDL2 2.0 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)

# Hashing, decompression and wire-format helpers: standard library only.
add_library(openhover_util STATIC src/util/Sha256.cpp src/util/Gunzip.cpp)
target_include_directories(openhover_util PUBLIC src/util)

# Track listing, name safety, and verified on-demand downloads.
add_library(openhover_content STATIC src/content/TrackCatalog.cpp src/content/TrackDownloader.cpp)
target_include_directories(openhover_content PUBLIC src/content)
target_link_libraries(openhover_content PUBLIC openhover_util Threads::Threads)

add_library(openhover_game STATIC src/game/BoostPad.cpp src/game/Championship.cpp src/game/Course.cpp src/game/CraftClass.cpp src/game/FixedStepClock.cpp src/game/HazardZone.cpp src/game/Hovercraft.cpp src/game/InputRecording.cpp src/game/LapTiming.cpp src/game/Race.cpp src/game/RaceMode.cpp src/game/RacePosition.cpp src/game/RaceStart.cpp src/game/RacerCollision.cpp src/game/RaisedSection.cpp src/game/RecoveryAssist.cpp src/game/RivalController.cpp src/game/RouteGuidance.cpp src/game/SteeringAssist.cpp src/game/TrackDefinition.cpp src/game/WallCollision.cpp)
target_include_directories(openhover_game PUBLIC src/game)

add_executable(OpenHover src/main.cpp)
target_link_libraries(OpenHover PRIVATE openhover_game SDL2::SDL2 OpenGL::GL)

if(BUILD_TESTING AND NOT WIN32)
    add_executable(TrackCatalogSmoke tests/TrackCatalogSmoke.cpp)
    target_link_libraries(TrackCatalogSmoke PRIVATE openhover_content)
    add_test(NAME TrackCatalogSmoke COMMAND TrackCatalogSmoke)

    add_executable(TrackDownloadSmoke tests/TrackDownloadSmoke.cpp)
    target_link_libraries(TrackDownloadSmoke PRIVATE openhover_content)
    add_test(NAME TrackDownloadSmoke COMMAND TrackDownloadSmoke)

    add_executable(HovercraftSmoke tests/HovercraftSmoke.cpp)
    target_link_libraries(HovercraftSmoke PRIVATE openhover_game)
    add_test(NAME HovercraftSmoke COMMAND HovercraftSmoke)

    add_executable(CraftClassSmoke tests/CraftClassSmoke.cpp)
    target_link_libraries(CraftClassSmoke PRIVATE openhover_game)
    add_test(NAME CraftClassSmoke COMMAND CraftClassSmoke)

    add_executable(ChampionshipSmoke tests/ChampionshipSmoke.cpp)
    target_link_libraries(ChampionshipSmoke PRIVATE openhover_game)
    add_test(NAME ChampionshipSmoke COMMAND ChampionshipSmoke)

    add_executable(RouteGuidanceSmoke tests/RouteGuidanceSmoke.cpp)
    target_link_libraries(RouteGuidanceSmoke PRIVATE openhover_game)
    add_test(NAME RouteGuidanceSmoke COMMAND RouteGuidanceSmoke)

    add_executable(SteeringAssistSmoke tests/SteeringAssistSmoke.cpp)
    target_link_libraries(SteeringAssistSmoke PRIVATE openhover_game)
    add_test(NAME SteeringAssistSmoke COMMAND SteeringAssistSmoke)

    add_executable(RecoveryAssistSmoke tests/RecoveryAssistSmoke.cpp)
    target_link_libraries(RecoveryAssistSmoke PRIVATE openhover_game)
    add_test(NAME RecoveryAssistSmoke COMMAND RecoveryAssistSmoke)

    add_executable(InputRecordingSmoke tests/InputRecordingSmoke.cpp)
    target_link_libraries(InputRecordingSmoke PRIVATE openhover_game)
    add_test(NAME InputRecordingSmoke COMMAND InputRecordingSmoke)

    add_executable(HazardZoneSmoke tests/HazardZoneSmoke.cpp)
    target_link_libraries(HazardZoneSmoke PRIVATE openhover_game)
    add_test(NAME HazardZoneSmoke COMMAND HazardZoneSmoke)

    add_executable(RaceSmoke tests/RaceSmoke.cpp)
    target_link_libraries(RaceSmoke PRIVATE openhover_game)
    add_test(NAME RaceSmoke COMMAND RaceSmoke)

    add_executable(RacePositionSmoke tests/RacePositionSmoke.cpp)
    target_link_libraries(RacePositionSmoke PRIVATE openhover_game)
    add_test(NAME RacePositionSmoke COMMAND RacePositionSmoke)

    add_executable(RivalControllerSmoke tests/RivalControllerSmoke.cpp)
    target_link_libraries(RivalControllerSmoke PRIVATE openhover_game)
    add_test(NAME RivalControllerSmoke COMMAND RivalControllerSmoke)

    add_executable(RaceStartSmoke tests/RaceStartSmoke.cpp)
    target_link_libraries(RaceStartSmoke PRIVATE openhover_game)
    add_test(NAME RaceStartSmoke COMMAND RaceStartSmoke)

    add_executable(CourseSmoke tests/CourseSmoke.cpp)
    target_link_libraries(CourseSmoke PRIVATE openhover_game)
    add_test(NAME CourseSmoke COMMAND CourseSmoke)

    add_executable(RacerCollisionSmoke tests/RacerCollisionSmoke.cpp)
    target_link_libraries(RacerCollisionSmoke PRIVATE openhover_game)
    add_test(NAME RacerCollisionSmoke COMMAND RacerCollisionSmoke)

    add_executable(BoostPadSmoke tests/BoostPadSmoke.cpp)
    target_link_libraries(BoostPadSmoke PRIVATE openhover_game)
    add_test(NAME BoostPadSmoke COMMAND BoostPadSmoke)

    add_executable(FixedStepClockSmoke tests/FixedStepClockSmoke.cpp)
    target_link_libraries(FixedStepClockSmoke PRIVATE openhover_game)
    add_test(NAME FixedStepClockSmoke COMMAND FixedStepClockSmoke)

    add_executable(TrackDefinitionSmoke tests/TrackDefinitionSmoke.cpp)
    target_link_libraries(TrackDefinitionSmoke PRIVATE openhover_game)
    add_test(NAME TrackDefinitionSmoke COMMAND TrackDefinitionSmoke)

    add_executable(RaceModeSmoke tests/RaceModeSmoke.cpp)
    target_link_libraries(RaceModeSmoke PRIVATE openhover_game)
    add_test(NAME RaceModeSmoke COMMAND RaceModeSmoke)

    add_executable(WallCollisionSmoke tests/WallCollisionSmoke.cpp)
    target_link_libraries(WallCollisionSmoke PRIVATE openhover_game)
    add_test(NAME WallCollisionSmoke COMMAND WallCollisionSmoke)

    add_executable(LapTimingSmoke tests/LapTimingSmoke.cpp)
    target_link_libraries(LapTimingSmoke PRIVATE openhover_game)
    add_test(NAME LapTimingSmoke COMMAND LapTimingSmoke)

    add_executable(RaisedSectionSmoke tests/RaisedSectionSmoke.cpp)
    target_link_libraries(RaisedSectionSmoke PRIVATE openhover_game)
    add_test(NAME RaisedSectionSmoke COMMAND RaisedSectionSmoke)
endif()
