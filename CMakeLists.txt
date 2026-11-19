cmake_minimum_required(VERSION 3.25)
project(nikque_gui VERSION 0.4.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "nikque GUI is built for 64-bit systems")
endif()

# Timings of the frames and the start written to a file: never in the default build (Windows only).
option(NG_MEASURE "build the measuring version (not for release)" OFF)

# The shared code (src) and the system's own parts: src/win (Win32, Direct2D), src/mac (Cocoa, Core Graphics) or
# src/linux (GTK 3, Cairo, Pango) with src/posix (files, the engines as child processes).
set(NG_SHARED
	src/app.cpp
	src/app_input.cpp
	src/app_engine.cpp
	src/analysis.cpp
	src/bookpeek.cpp
	src/openings.cpp
	src/app_learn.cpp
	src/app_tools.cpp
	src/app_board.cpp
	src/app_list.cpp
	src/app_record.cpp
	src/app_script.cpp
	src/app_play.cpp
	src/app_tree.cpp
	src/app_settings.cpp
	src/form.cpp
	src/lang.cpp
	src/keys.cpp
	src/theme.cpp
	src/draw_board.cpp
	src/draw_panel.cpp
)

if(WIN32)
	enable_language(RC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	add_compile_options(/utf-8 /W4 /permissive- /Zc:__cplusplus /EHsc)
	add_compile_definitions(UNICODE _UNICODE NOMINMAX)
	set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /DNDEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF")
	set(NG_PLATFORM src/win/platform_win.cpp)
	set(NG_SYSTEM
		src/win/app_win.cpp
		src/win/paths_win.cpp
		src/win/app_record_win.cpp
		src/win/form_win.cpp
		src/win/engine_win.cpp
		src/win/gfx_win.cpp
	)
elseif(APPLE)
	enable_language(OBJCXX)
	set(CMAKE_OBJCXX_STANDARD 23)
	set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
	add_compile_options(-Wall -Wextra -Wno-unused-parameter)
	add_compile_options($<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
	set(NG_PLATFORM src/mac/platform_mac.mm)
	set(NG_CORE_LIBS "-framework Cocoa")
	set(NG_SYSTEM
		src/mac/app_mac.mm
		src/mac/paths_mac.cpp
		src/mac/app_record_mac.mm
		src/mac/form_mac.mm
		src/mac/engine_mac.cpp
		src/mac/gfx_mac.mm
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# GTK 3 with Cairo and Pango (LGPL), linked as the shared libraries of the system
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
	find_package(Threads REQUIRED)
	add_compile_options(-Wall -Wextra -Wno-unused-parameter)
	set(NG_PLATFORM src/posix/files_posix.cpp src/linux/platform_linux.cpp)
	set(NG_CORE_LIBS PkgConfig::GTK3 Threads::Threads)
	set(NG_SYSTEM
		src/linux/app_linux.cpp
		src/posix/paths_posix.cpp
		src/linux/app_record_linux.cpp
		src/linux/form_linux.cpp
		src/posix/engine_posix.cpp
		src/linux/gfx_linux.cpp
	)
else()
	# (Android: android/app/src/main/cpp/CMakeLists.txt, built by Gradle)
	message(FATAL_ERROR "nikque GUI is built for Windows, macOS or Linux")
endif()

set(NG_CORE
	${NG_PLATFORM}
	src/ini.cpp
	src/rules.cpp
	src/game.cpp
	src/gamelist.cpp
	src/linefind.cpp
)

# the GUI without its entry point: the program and the tests of the app (tests/app_test.cpp) are built from it
add_library(ng_app OBJECT ${NG_CORE} ${NG_SHARED} ${NG_SYSTEM})
target_include_directories(ng_app PUBLIC src)
target_compile_definitions(ng_app PRIVATE NG_VERSION="${PROJECT_VERSION}")
if(NG_MEASURE)
	target_compile_definitions(ng_app PRIVATE NG_MEASURE=1)
endif()

if(WIN32)
	target_link_libraries(ng_app PUBLIC d2d1 d3d11 dxgi dxguid dwrite windowscodecs dwmapi shcore comctl32 ole32 shell32 uuid comdlg32)

	# the icons are drawn by a program at build time (no image files in the sources)
	add_executable(ng_mkicon tools/mkicon.cpp src/win/icon_draw.cpp)
	target_link_libraries(ng_mkicon PRIVATE d2d1 windowscodecs ole32)
	set(NG_ICON ${CMAKE_BINARY_DIR}/gen/app.ico ${CMAKE_BINARY_DIR}/gen/paw.ico)
	add_custom_command(
		OUTPUT ${NG_ICON}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gen
		COMMAND ng_mkicon ${CMAKE_BINARY_DIR}/gen
		DEPENDS ng_mkicon
		COMMENT "drawing the icon")
	add_custom_target(ng_icon DEPENDS ${NG_ICON})

	add_executable(nikque-gui WIN32
		src/win/main_win.cpp
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
else()
	# the Japanese words in the program (lang/ja.txt as the bytes of a C++ array: src/lang.cpp)
	set(NG_LANG_INC ${CMAKE_BINARY_DIR}/gen/lang_ja.inc)
	add_custom_command(
		OUTPUT ${NG_LANG_INC}
		COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_SOURCE_DIR}/lang/ja.txt -DOUT=${NG_LANG_INC} -P ${CMAKE_SOURCE_DIR}/tools/bytes_inc.cmake
		DEPENDS ${CMAKE_SOURCE_DIR}/lang/ja.txt ${CMAKE_SOURCE_DIR}/tools/bytes_inc.cmake
		COMMENT "the Japanese words")
	target_sources(ng_app PRIVATE ${NG_LANG_INC})
	target_include_directories(ng_app PRIVATE ${CMAKE_BINARY_DIR}/gen)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(ng_app PUBLIC PkgConfig::GTK3 Threads::Threads)
	add_executable(nikque-gui src/linux/main_linux.cpp)
	target_link_libraries(nikque-gui PRIVATE ng_app)
	# (the C++ library of the compiler is in the program: it runs where the system has an older one)
	target_link_options(nikque-gui PRIVATE -static-libstdc++ -static-libgcc)
	# the language files and the readme next to the program
	add_custom_command(TARGET nikque-gui POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lang $<TARGET_FILE_DIR:nikque-gui>/lang
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/readme.txt $<TARGET_FILE_DIR:nikque-gui>/readme.txt)
elseif(APPLE)
	target_link_libraries(ng_app PUBLIC "-framework Cocoa" "-framework CoreText" "-framework CoreGraphics"
		"-framework ImageIO" "-framework UniformTypeIdentifiers")

	# the icon is drawn by a program at build time (as on Windows), then made into AppIcon.icns
	add_executable(ng_mkicon_mac tools/mkicon_mac.mm)
	target_link_libraries(ng_mkicon_mac PRIVATE "-framework Foundation" "-framework CoreGraphics" "-framework ImageIO")
	set(NG_ICNS ${CMAKE_BINARY_DIR}/gen/AppIcon.icns)
	add_custom_command(
		OUTPUT ${NG_ICNS}
		COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/gen/AppIcon.iconset
		COMMAND ng_mkicon_mac ${CMAKE_BINARY_DIR}/gen/AppIcon.iconset
		COMMAND iconutil -c icns -o ${NG_ICNS} ${CMAKE_BINARY_DIR}/gen/AppIcon.iconset
		DEPENDS ng_mkicon_mac
		COMMENT "drawing the icon")
	set_source_files_properties(${NG_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

	add_executable(nikque-gui MACOSX_BUNDLE src/mac/main_mac.mm ${NG_ICNS})
	target_link_libraries(nikque-gui PRIVATE ng_app)
	set_target_properties(nikque-gui PROPERTIES
		OUTPUT_NAME "nikque GUI"
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/mac/Info.plist.in
		MACOSX_BUNDLE_BUNDLE_NAME "nikque GUI"
		MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.nikque.gui"
		MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

enable_testing()
# (the games of the tests are in _test, which is not in the published sources: those tests only where it is)
set(NG_GAMES ${CMAKE_SOURCE_DIR}/_test/books/book1.games ${CMAKE_SOURCE_DIR}/_test/books/book2.games
	${CMAKE_SOURCE_DIR}/_test/books/book3.games)
add_executable(rules_test tests/rules_test.cpp ${NG_CORE})
target_link_libraries(rules_test PRIVATE ${NG_CORE_LIBS})
target_include_directories(rules_test PRIVATE src)
if(EXISTS ${CMAKE_SOURCE_DIR}/_test/books/book1.games)
	add_test(NAME rules COMMAND rules_test ${NG_GAMES})
endif()
add_executable(parse_test tests/parse_test.cpp src/analysis.cpp ${NG_CORE})
target_link_libraries(parse_test PRIVATE ${NG_CORE_LIBS})
target_include_directories(parse_test PRIVATE src)
add_test(NAME parse COMMAND parse_test)
if(WIN32)
	add_executable(path_test tests/path_test.cpp src/win/paths_win.cpp res/app.manifest)  # the manifest of the GUI: its code page is UTF-8
	target_include_directories(path_test PRIVATE src)
	add_test(NAME path COMMAND path_test ${CMAKE_SOURCE_DIR}/_test)
endif()
add_executable(lang_test tests/lang_test.cpp)
add_test(NAME lang COMMAND lang_test ${CMAKE_SOURCE_DIR}/lang)
add_executable(list_test tests/list_test.cpp ${NG_CORE})
target_link_libraries(list_test PRIVATE ${NG_CORE_LIBS})
target_include_directories(list_test PRIVATE src)
if(EXISTS ${CMAKE_SOURCE_DIR}/_test/books/book1.games)
	add_test(NAME list COMMAND list_test ${NG_GAMES})
endif()
if(WIN32)
	add_executable(app_test tests/app_test.cpp)
	target_link_libraries(app_test PRIVATE ng_app)
	add_test(NAME app COMMAND app_test ${CMAKE_SOURCE_DIR})
endif()
