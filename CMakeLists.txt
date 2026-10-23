cmake_minimum_required(VERSION 3.25)
project(nikque_gui VERSION 0.2.1 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "nikque GUI is built for x64")
endif()

add_compile_options(/utf-8 /W4 /permissive- /Zc:__cplusplus /EHsc)
add_compile_definitions(UNICODE _UNICODE NOMINMAX)
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF")

# Timings of the frames and the start written to a file: never in the default build.
option(NG_MEASURE "build the measuring version (not for release)" OFF)

set(NG_CORE
	src/rules.cpp
	src/game.cpp
	src/gamelist.cpp
	src/linefind.cpp
)

# the icons are drawn by a program at build time (no image files in the sources)
add_executable(ng_mkicon tools/mkicon.cpp src/icon_draw.cpp)
target_link_libraries(ng_mkicon PRIVATE d2d1 windowscodecs ole32)
set(NG_ICON ${CMAKE_BINARY_DIR}/gen/app.ico ${CMAKE_BINARY_DIR}/gen/paw.ico)
add_custom_command(
	OUTPUT ${NG_ICON}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gen
	COMMAND ng_mkicon ${CMAKE_BINARY_DIR}/gen
	DEPENDS ng_mkicon
	COMMENT "drawing the icon")
add_custom_target(ng_icon DEPENDS ${NG_ICON})

# the GUI without its entry point: the program and the tests of the app (tests/app_test.cpp) are built from it
add_library(ng_app OBJECT
	${NG_CORE}
	src/app.cpp
	src/app_input.cpp
	src/app_engine.cpp
	src/analysis.cpp
	src/openings.cpp
	src/app_learn.cpp
	src/app_tools.cpp
	src/paths.cpp
	src/app_board.cpp
	src/app_list.cpp
	src/app_script.cpp
	src/app_play.cpp
	src/app_tree.cpp
	src/app_settings.cpp
	src/form.cpp
	src/engine.cpp
	src/ini.cpp
	src/lang.cpp
	src/keys.cpp
	src/theme.cpp
	src/gfx.cpp
	src/draw_board.cpp
	src/draw_panel.cpp
	src/icon_draw.cpp
)
target_include_directories(ng_app PUBLIC src)
target_link_libraries(ng_app PUBLIC d2d1 d3d11 dxgi dxguid dwrite windowscodecs dwmapi shcore comctl32 ole32 shell32 uuid comdlg32)
target_compile_definitions(ng_app PRIVATE NG_VERSION="${PROJECT_VERSION}")
if(NG_MEASURE)
	target_compile_definitions(ng_app PRIVATE NG_MEASURE=1)
endif()

add_executable(nikque-gui WIN32
	src/main.cpp
	res/app.rc
)
add_dependencies(nikque-gui ng_icon)
target_include_directories(nikque-gui PRIVATE ${CMAKE_BINARY_DIR}/gen)
set_source_files_properties(res/app.rc PROPERTIES OBJECT_DEPENDS "${NG_ICON};${CMAKE_SOURCE_DIR}/lang/ja.txt;${CMAKE_SOURCE_DIR}/res/app.manifest")
target_link_libraries(nikque-gui PRIVATE ng_app)
target_link_options(nikque-gui PRIVATE /MANIFEST:NO)
# the language files and the readme next to the exe
add_custom_command(TARGET nikque-gui POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lang $<TARGET_FILE_DIR:nikque-gui>/lang
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/readme.txt $<TARGET_FILE_DIR:nikque-gui>/readme.txt)

enable_testing()
add_executable(rules_test tests/rules_test.cpp ${NG_CORE})
add_test(NAME rules COMMAND rules_test
	${CMAKE_SOURCE_DIR}/_test/books/book1.games
	${CMAKE_SOURCE_DIR}/_test/books/book2.games
	${CMAKE_SOURCE_DIR}/_test/books/book3.games)
add_executable(parse_test tests/parse_test.cpp src/analysis.cpp ${NG_CORE})
target_include_directories(parse_test PRIVATE src)
add_test(NAME parse COMMAND parse_test)
add_executable(path_test tests/path_test.cpp src/paths.cpp res/app.manifest)  # the manifest of the GUI: its code page is UTF-8
add_test(NAME path COMMAND path_test ${CMAKE_SOURCE_DIR}/_test)
add_executable(lang_test tests/lang_test.cpp)
add_test(NAME lang COMMAND lang_test ${CMAKE_SOURCE_DIR}/lang)
add_executable(list_test tests/list_test.cpp ${NG_CORE})
target_include_directories(list_test PRIVATE src)
add_test(NAME list COMMAND list_test
	${CMAKE_SOURCE_DIR}/_test/books/book1.games
	${CMAKE_SOURCE_DIR}/_test/books/book2.games
	${CMAKE_SOURCE_DIR}/_test/books/book3.games)
add_executable(app_test tests/app_test.cpp)
target_link_libraries(app_test PRIVATE ng_app)
add_test(NAME app COMMAND app_test ${CMAKE_SOURCE_DIR})
