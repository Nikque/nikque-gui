# libnikquegui.so: the shared C++ of nikque GUI (src) with the parts of Android (src/android) and of POSIX (src/posix),
# built by Gradle for each ABI of the app.
cmake_minimum_required(VERSION 3.22)
project(nikquegui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(NG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../.. ABSOLUTE)
set(NG_SRC ${NG_ROOT}/src)

# the Japanese words in the program (lang/ja.txt as the bytes of a C++ array: src/lang.cpp)
set(NG_LANG_INC ${CMAKE_BINARY_DIR}/gen/lang_ja.inc)
add_custom_command(
	OUTPUT ${NG_LANG_INC}
	COMMAND ${CMAKE_COMMAND} -DIN=${NG_ROOT}/lang/ja.txt -DOUT=${NG_LANG_INC} -P ${NG_ROOT}/tools/bytes_inc.cmake
	DEPENDS ${NG_ROOT}/lang/ja.txt ${NG_ROOT}/tools/bytes_inc.cmake
	COMMENT "the Japanese words")

add_library(nikquegui SHARED
	${NG_SRC}/app.cpp
	${NG_SRC}/app_input.cpp
	${NG_SRC}/app_engine.cpp
	${NG_SRC}/analysis.cpp
	${NG_SRC}/bookpeek.cpp
	${NG_SRC}/openings.cpp
	${NG_SRC}/app_learn.cpp
	${NG_SRC}/app_tools.cpp
	${NG_SRC}/app_board.cpp
	${NG_SRC}/app_list.cpp
	${NG_SRC}/app_record.cpp
	${NG_SRC}/app_script.cpp
	${NG_SRC}/app_play.cpp
	${NG_SRC}/app_tree.cpp
	${NG_SRC}/app_settings.cpp
	${NG_SRC}/form.cpp
	${NG_SRC}/lang.cpp
	${NG_SRC}/keys.cpp
	${NG_SRC}/theme.cpp
	${NG_SRC}/draw_board.cpp
	${NG_SRC}/draw_panel.cpp
	${NG_SRC}/ini.cpp
	${NG_SRC}/rules.cpp
	${NG_SRC}/game.cpp
	${NG_SRC}/gamelist.cpp
	${NG_SRC}/linefind.cpp
	${NG_SRC}/posix/files_posix.cpp
	${NG_SRC}/posix/paths_posix.cpp
	${NG_SRC}/posix/engine_posix.cpp
	${NG_SRC}/android/android_bridge.cpp
	${NG_SRC}/android/platform_android.cpp
	${NG_SRC}/android/gfx_android.cpp
	${NG_SRC}/android/app_android.cpp
	${NG_SRC}/android/form_android.cpp
	${NG_LANG_INC}
)
target_include_directories(nikquegui PRIVATE ${NG_SRC} ${CMAKE_BINARY_DIR}/gen)
target_compile_definitions(nikquegui PRIVATE NG_VERSION="${NG_VERSION}")
target_compile_options(nikquegui PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(nikquegui PRIVATE log)
