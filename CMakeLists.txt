cmake_minimum_required(VERSION 3.16)
project(ProjectAsobi C CXX)

set(CMAKE_CXX_STANDARD 20)

# raylib viene scaricata e compilata automaticamente al primo caricamento del progetto
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

add_executable(ProjectAsobi
    src/main.cpp
    src/Game.cpp
    src/EnemyAI.cpp
    src/GameMaster.cpp
    src/Nav.cpp
    src/Dungeon.cpp
    src/Artifacts.cpp
    src/Input.cpp
    src/PadRaw.cpp
    src/Props.cpp
    src/Sky.cpp
    src/Weapons.cpp
    src/PauseMenu.cpp
    src/MatchMenu.cpp
    src/Guide.cpp
    src/Cutscene.cpp
    src/Tutorial.cpp
    src/ControllerMenu.cpp
    # Ricerca e abbinamento controller: ogni file si compila solo sulla sua piattaforma
    src/link/ControllerLink_Windows.cpp
    src/link/ControllerLink_Unsupported.cpp
)
target_link_libraries(ProjectAsobi PRIVATE raylib)
# GLFW (dentro raylib) serve a PadRaw.cpp per leggere i tasti che raylib non conosce, come il touchpad PS
target_include_directories(ProjectAsobi PRIVATE "${raylib_SOURCE_DIR}/src/external/glfw/include")

if (WIN32)
    # runtimeobject: API moderna di Windows (WinRT); bthprops: rilevamento dell'adattatore Bluetooth
    target_link_libraries(ProjectAsobi PRIVATE runtimeobject bthprops)
endif()

# Con MinGW include nel .exe le librerie standard di C/C++
# (-static completo non funziona col MinGW incluso in CLion)
if (MINGW)
    target_link_options(ProjectAsobi PRIVATE -static-libgcc -static-libstdc++)

    # Copia accanto all'.exe l'unica DLL di MinGW che serve ancora
    get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET ProjectAsobi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MINGW_BIN}/libwinpthread-1.dll" "$<TARGET_FILE_DIR:ProjectAsobi>")
endif()
