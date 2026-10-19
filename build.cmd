@echo off
rem Builds nikque GUI (Release, x64) into build\Release. "build.cmd measure" builds the measuring version into build-measure.
setlocal
set MSBUILDDISABLENODEREUSE=1
set DIR=%~dp0
set B=%DIR%build
set M=OFF
if /i "%1"=="measure" (
	set B=%DIR%build-measure
	set M=ON
)
cmake -S "%DIR%." -B "%B%" -G "Visual Studio 17 2022" -A x64 -DNG_MEASURE=%M% || exit /b 1
cmake --build "%B%" --config Release --parallel -- /nodeReuse:false || exit /b 1
ctest --test-dir "%B%" -C Release --output-on-failure || exit /b 1
