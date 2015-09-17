:: Set required variables for win x86 build
set vcredist=x86
set postfix=%vcredist%
path=%QtDir_x86%;%QtIFW%;%path%

if %compiler% EQU msvc (
	:: set the build platform variables from visual studio compiler
	call %VSPath%\VC\vcvarsall.bat %vcredist% || exit /b
) else (
	path=%MinGWPath%;%path%
)
