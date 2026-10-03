# SPDX-License-Identifier: MIT OR Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(OpenHover LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(OPENHOVER_PACKAGE_VERSION "0.1.0" CACHE STRING "Installer package version")
option(OPENHOVER_DESKTOP_PACKAGE "Package the desktop game instead of the race server" OFF)

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE OPENHOVER_SOURCE_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT OPENHOVER_SOURCE_REVISION)
    set(OPENHOVER_SOURCE_REVISION "unknown")
endif()

include(CTest)
find_package(Threads REQUIRED)
find_package(SDL2 2.0 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)

# Hashing, decompression and wire-format helpers: standard library only.
add_library(openhover_util STATIC src/util/CrashReport.cpp src/util/Sha256.cpp src/util/Gunzip.cpp)
target_include_directories(openhover_util PUBLIC src/util)

# Track listing, name safety, and verified on-demand downloads.
add_library(openhover_content STATIC src/content/TrackCatalog.cpp src/content/TrackDownloader.cpp)
target_include_directories(openhover_content PUBLIC src/content)
target_link_libraries(openhover_content PUBLIC openhover_util Threads::Threads)

add_library(openhover_game STATIC src/game/BoostPad.cpp src/game/CameraRig.cpp src/game/Championship.cpp src/game/Course.cpp src/game/CraftClass.cpp src/game/FixedStepClock.cpp src/game/GhostLibrary.cpp src/game/Ground.cpp src/game/HazardZone.cpp src/game/Hovercraft.cpp src/game/InputRecording.cpp src/game/LapTiming.cpp src/game/Mine.cpp src/game/Missile.cpp src/game/PracticeGuide.cpp src/game/Race.cpp src/game/RaceMode.cpp src/game/RacePosition.cpp src/game/RaceStart.cpp src/game/RacerCollision.cpp src/game/RaisedSection.cpp src/game/RecoveryAssist.cpp src/game/RivalController.cpp src/game/RivalNames.cpp src/game/RouteGuidance.cpp src/game/RouteTracker.cpp src/game/SteeringAssist.cpp src/game/StallDetector.cpp src/game/TrackBuilder.cpp src/game/TrackDefinition.cpp src/game/TrackFile.cpp src/game/TrackHash.cpp src/game/TrackLoader.cpp src/game/WallCollision.cpp)
target_include_directories(openhover_game PUBLIC src/game)
target_link_libraries(openhover_game PUBLIC openhover_util)

# Keyboard bindings use SDL scancodes, so they live apart from the SDL-free game and server code.
add_library(openhover_input STATIC src/game/KeyBindings.cpp src/game/PadBindings.cpp src/game/PadMenuInput.cpp)
target_include_directories(openhover_input PUBLIC src/game)
target_link_libraries(openhover_input PUBLIC SDL2::SDL2)

add_library(openhover_lobby STATIC src/net/AuthoritativeRace.cpp src/net/Lobby.cpp src/net/ReconnectPolicy.cpp src/net/TcpLobbyClient.cpp)
target_include_directories(openhover_lobby PUBLIC src/net)
target_link_libraries(openhover_lobby PUBLIC openhover_game)

if(UNIX)
    add_executable(OpenHoverServer src/server/main.cpp)
    target_link_libraries(OpenHoverServer PRIVATE openhover_lobby)
    target_compile_definitions(OpenHoverServer PRIVATE
        OPENHOVER_VERSION="${OPENHOVER_PACKAGE_VERSION}"
        OPENHOVER_SOURCE_REVISION="${OPENHOVER_SOURCE_REVISION}")
endif()

add_executable(OpenHoverTrackCheck src/tools/TrackCheck.cpp)
target_link_libraries(OpenHoverTrackCheck PRIVATE openhover_game)

# Reading and converting room-based track files from another game (for tracks whose author has the
# rights to them); no SDL or OpenGL here, so it is easy to test.
add_library(openhover_convert STATIC src/convert/TrkReader.cpp src/convert/TrkConverter.cpp)
target_include_directories(openhover_convert PUBLIC src/convert)
target_link_libraries(openhover_convert PUBLIC openhover_game)

# The built-in pixel font, shared by the game and the tools that draw their own screens.
add_library(openhover_ui STATIC src/ui/PixelFont.cpp)
target_include_directories(openhover_ui PUBLIC src/ui)
target_link_libraries(openhover_ui PUBLIC SDL2::SDL2 OpenGL::GL)

# A separate application that converts room-based tracks and shows the layout it will write.
add_executable(OpenHoverTrkConvert src/convert/TrkConvertApp.cpp)
target_link_libraries(OpenHoverTrkConvert PRIVATE openhover_convert openhover_ui SDL2::SDL2 OpenGL::GL)

add_executable(OpenHover src/main.cpp src/AudioFeedback.cpp)
target_link_libraries(OpenHover PRIVATE openhover_game openhover_input openhover_lobby openhover_ui SDL2::SDL2 OpenGL::GL)
target_compile_definitions(OpenHover PRIVATE OPENHOVER_VERSION="${OPENHOVER_PACKAGE_VERSION}")

if(WIN32)
    target_link_libraries(OpenHover PRIVATE SDL2::SDL2main ws2_32)
    install(TARGETS OpenHover RUNTIME DESTINATION .)

    set(CPACK_GENERATOR NSIS)
    set(CPACK_PACKAGE_NAME OpenHover)
    set(CPACK_PACKAGE_VERSION "${OPENHOVER_PACKAGE_VERSION}")
    set(CPACK_PACKAGE_CONTACT "OpenHover Developers")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenHover")
    set(CPACK_NSIS_DISPLAY_NAME "OpenHover")
    set(CPACK_NSIS_PACKAGE_NAME "OpenHover")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
    include(CPack)
endif()

if(UNIX)
    include(GNUInstallDirs)
    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGE_VERSION "${OPENHOVER_PACKAGE_VERSION}")
    set(CPACK_PACKAGE_CONTACT "OpenHover Developers")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OpenHover Developers")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
    if(OPENHOVER_DESKTOP_PACKAGE)
        install(TARGETS OpenHover OpenHoverTrackCheck OpenHoverTrkConvert RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        set(CPACK_PACKAGE_NAME openhover)
    else()
        install(TARGETS OpenHoverServer OpenHoverTrackCheck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(FILES packaging/systemd/openhover-raceserver.service
                DESTINATION lib/systemd/system)
        set(CPACK_PACKAGE_NAME openhover-raceserver)
        set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postrm")
    endif()
    include(CPack)
endif()

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

    add_executable(TrkConvertSmoke tests/TrkConvertSmoke.cpp)
    target_link_libraries(TrkConvertSmoke PRIVATE openhover_convert openhover_lobby)
    add_test(NAME TrkConvertSmoke COMMAND TrkConvertSmoke)

    add_executable(CrashReportSmoke tests/CrashReportSmoke.cpp)
    target_link_libraries(CrashReportSmoke PRIVATE openhover_util)
    add_test(NAME CrashReportSmoke COMMAND CrashReportSmoke)

    add_executable(ClassBalanceSmoke tests/ClassBalanceSmoke.cpp)
    target_link_libraries(ClassBalanceSmoke PRIVATE openhover_lobby)
    add_test(NAME ClassBalanceSmoke COMMAND ClassBalanceSmoke)

    add_executable(StallDetectorSmoke tests/StallDetectorSmoke.cpp)
    target_link_libraries(StallDetectorSmoke PRIVATE openhover_game)
    add_test(NAME StallDetectorSmoke COMMAND StallDetectorSmoke)

    add_executable(RouteTrackerSmoke tests/RouteTrackerSmoke.cpp)
    target_link_libraries(RouteTrackerSmoke PRIVATE openhover_game)
    add_test(NAME RouteTrackerSmoke COMMAND RouteTrackerSmoke)

    add_executable(TrackBuilderSmoke tests/TrackBuilderSmoke.cpp)
    target_link_libraries(TrackBuilderSmoke PRIVATE openhover_lobby)
    add_test(NAME TrackBuilderSmoke COMMAND TrackBuilderSmoke)

    add_executable(TrackHashSmoke tests/TrackHashSmoke.cpp)
    target_link_libraries(TrackHashSmoke PRIVATE openhover_game)
    add_test(NAME TrackHashSmoke COMMAND TrackHashSmoke)

    add_executable(TrackLoaderSmoke tests/TrackLoaderSmoke.cpp)
    target_link_libraries(TrackLoaderSmoke PRIVATE openhover_game)
    add_test(NAME TrackLoaderSmoke COMMAND TrackLoaderSmoke)

    add_executable(TrackFileSmoke tests/TrackFileSmoke.cpp)
    target_link_libraries(TrackFileSmoke PRIVATE openhover_game)
    add_test(NAME TrackFileSmoke COMMAND TrackFileSmoke)

    add_executable(TrackWallSmoke tests/TrackWallSmoke.cpp)
    target_link_libraries(TrackWallSmoke PRIVATE openhover_game)
    add_test(NAME TrackWallSmoke COMMAND TrackWallSmoke)

    add_executable(TrackRecoverySmoke tests/TrackRecoverySmoke.cpp)
    target_link_libraries(TrackRecoverySmoke PRIVATE openhover_game)
    add_test(NAME TrackRecoverySmoke COMMAND TrackRecoverySmoke)

    add_executable(RecoveryAssistSmoke tests/RecoveryAssistSmoke.cpp)
    target_link_libraries(RecoveryAssistSmoke PRIVATE openhover_game)
    add_test(NAME RecoveryAssistSmoke COMMAND RecoveryAssistSmoke)

    add_executable(GhostLibrarySmoke tests/GhostLibrarySmoke.cpp)
    target_link_libraries(GhostLibrarySmoke PRIVATE openhover_game)
    add_test(NAME GhostLibrarySmoke COMMAND GhostLibrarySmoke)

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

    add_executable(PracticeGuideSmoke tests/PracticeGuideSmoke.cpp)
    target_link_libraries(PracticeGuideSmoke PRIVATE openhover_game)
    add_test(NAME PracticeGuideSmoke COMMAND PracticeGuideSmoke)

    add_executable(WallCollisionSmoke tests/WallCollisionSmoke.cpp)
    target_link_libraries(WallCollisionSmoke PRIVATE openhover_game)
    add_test(NAME WallCollisionSmoke COMMAND WallCollisionSmoke)

    add_executable(CameraRigSmoke tests/CameraRigSmoke.cpp)
    target_link_libraries(CameraRigSmoke PRIVATE openhover_game)
    add_test(NAME CameraRigSmoke COMMAND CameraRigSmoke)

    add_executable(RivalNamesSmoke tests/RivalNamesSmoke.cpp)
    target_link_libraries(RivalNamesSmoke PRIVATE openhover_game)
    add_test(NAME RivalNamesSmoke COMMAND RivalNamesSmoke)

    add_executable(ReconnectPolicySmoke tests/ReconnectPolicySmoke.cpp)
    target_link_libraries(ReconnectPolicySmoke PRIVATE openhover_lobby)
    add_test(NAME ReconnectPolicySmoke COMMAND ReconnectPolicySmoke)

    add_executable(PadMenuInputSmoke tests/PadMenuInputSmoke.cpp)
    target_link_libraries(PadMenuInputSmoke PRIVATE openhover_input)
    add_test(NAME PadMenuInputSmoke COMMAND PadMenuInputSmoke)

    add_executable(PadBindingsSmoke tests/PadBindingsSmoke.cpp)
    target_link_libraries(PadBindingsSmoke PRIVATE openhover_input)
    add_test(NAME PadBindingsSmoke COMMAND PadBindingsSmoke)

    add_executable(KeyBindingsSmoke tests/KeyBindingsSmoke.cpp)
    target_link_libraries(KeyBindingsSmoke PRIVATE openhover_input)
    add_test(NAME KeyBindingsSmoke COMMAND KeyBindingsSmoke)

    add_executable(LapTimingSmoke tests/LapTimingSmoke.cpp)
    target_link_libraries(LapTimingSmoke PRIVATE openhover_game)
    add_test(NAME LapTimingSmoke COMMAND LapTimingSmoke)

    add_executable(GroundSmoke tests/GroundSmoke.cpp)
    target_link_libraries(GroundSmoke PRIVATE openhover_game)
    add_test(NAME GroundSmoke COMMAND GroundSmoke)

    add_executable(RaisedSectionSmoke tests/RaisedSectionSmoke.cpp)
    target_link_libraries(RaisedSectionSmoke PRIVATE openhover_game)
    add_test(NAME RaisedSectionSmoke COMMAND RaisedSectionSmoke)

    add_executable(AudioFeedbackSmoke tests/AudioFeedbackSmoke.cpp src/AudioFeedback.cpp)
    target_include_directories(AudioFeedbackSmoke PRIVATE src)
    target_link_libraries(AudioFeedbackSmoke PRIVATE SDL2::SDL2)
    add_test(NAME AudioFeedbackSmoke COMMAND AudioFeedbackSmoke)

    add_executable(MissileSmoke tests/MissileSmoke.cpp)
    target_link_libraries(MissileSmoke PRIVATE openhover_game)
    add_test(NAME MissileSmoke COMMAND MissileSmoke)

    add_executable(MineSmoke tests/MineSmoke.cpp)
    target_link_libraries(MineSmoke PRIVATE openhover_game)
    add_test(NAME MineSmoke COMMAND MineSmoke)

    add_executable(LobbySmoke tests/LobbySmoke.cpp)
    target_link_libraries(LobbySmoke PRIVATE openhover_lobby)
    add_test(NAME LobbySmoke COMMAND LobbySmoke)

    if(UNIX)
        add_executable(ServerResilienceSmoke tests/ServerResilienceSmoke.cpp)
        target_link_libraries(ServerResilienceSmoke PRIVATE openhover_lobby)
        add_test(NAME ServerResilienceSmoke COMMAND ServerResilienceSmoke $<TARGET_FILE:OpenHoverServer>)

        add_executable(CustomTrackLobbySmoke tests/CustomTrackLobbySmoke.cpp)
        target_link_libraries(CustomTrackLobbySmoke PRIVATE openhover_lobby)
        add_test(NAME CustomTrackLobbySmoke COMMAND CustomTrackLobbySmoke $<TARGET_FILE:OpenHoverServer>)

        add_executable(LobbyClientSmoke tests/LobbyClientSmoke.cpp)
        target_link_libraries(LobbyClientSmoke PRIVATE openhover_lobby)
        add_test(NAME LobbyClientSmoke COMMAND LobbyClientSmoke $<TARGET_FILE:OpenHoverServer>)

        add_test(NAME ServerVersionSmoke COMMAND OpenHoverServer --version)
        set_tests_properties(ServerVersionSmoke PROPERTIES
            PASS_REGULAR_EXPRESSION "protocol=7 content=2")
    endif()

    add_executable(TrackLapTimeSmoke tests/TrackLapTimeSmoke.cpp)
    target_link_libraries(TrackLapTimeSmoke PRIVATE openhover_lobby)
    add_test(NAME TrackLapTimeSmoke COMMAND TrackLapTimeSmoke)

    add_executable(ImpairedNetworkSmoke tests/ImpairedNetworkSmoke.cpp)
    target_link_libraries(ImpairedNetworkSmoke PRIVATE openhover_lobby)
    add_test(NAME ImpairedNetworkSmoke COMMAND ImpairedNetworkSmoke)

    add_executable(AuthoritativeRaceSmoke tests/AuthoritativeRaceSmoke.cpp)
    target_link_libraries(AuthoritativeRaceSmoke PRIVATE openhover_lobby)
    add_test(NAME AuthoritativeRaceSmoke COMMAND AuthoritativeRaceSmoke)
endif()
