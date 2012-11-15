Rem ember to run this from the 64 BIT Visual Studio Command Prompt.  
set OLD_CD=%CD%

set CONFIGURATION= /p:Configuration=Release /p:Platform=x64 
set BIN_DIR=bin64
set LIB_DIR=lib64
set BUILD_DIR=x64\Release
set ZIP_OUT_NAME=gravity-MSVC11-64bit.zip

mkdir %BIN_DIR%
mkdir %LIB_DIR%
mkdir include
mkdir include\protobuf

Rem Build ThirdParty
cd ThirdParty

mkdir %BIN_DIR%
mkdir %LIB_DIR%

Rem Build Third party libs (these have the most variation and may require separate configuration).  
cd protobuf-2.4.1\vsprojects
msbuild libprotobuf.vcxproj %CONFIGURATION% || goto build_fail
msbuild protoc.vcxproj %CONFIGURATION% || goto build_fail
copy %BUILD_DIR%\libprotobuf.lib ..\..\%LIB_DIR%
copy %BUILD_DIR%\protoc.exe ..\..\%BIN_DIR%
cd ..\..
xcopy /s /q /y ThirdParty\protobuf-2.4.1\src\*.h include

cd zeromq-3.2.1\builds\msvc11
msbuild msvc11.sln %CONFIGURATION% || goto build_fail
copy ..\..\lib\x64\libzmq.lib ..\..\..\%LIB_DIR%\libzmq.lib
copy ..\..\bin\x64\libzmq.dll ..\..\..\%BIN_DIR%\libzmq.dll

cd ..\..\..\iniparser\build
msbuild iniparser.sln %CONFIGURATION% || goto build_fail
Rem lib File from this project is built directly into the lib directory

where /q cmake
if not errorlevel 1 (

cd ThirdParty\cppdb-trunk
mkdir buildMSVS1-64bit
cd buildMSVS11-64bit
cmake -G"Visual Studio 11" ..
msbuild cppdb.vcxproj %CONFIGURATION% || goto build_fail
copy %BUILD_DIR%\cppdb.lib ..\..\%LIB_DIR%
copy %BUILD_DIR%\cppdb.dll ..\..\%BIN_DIR%
cd ..\..\..

)

cd ..\..\..
copy ThirdParty\pthreads\lib\x64\pthreadVC2.lib ThirdParty\%LIB_DIR%
copy ThirdParty\pthreads\bin\x64\pthreadVC2.dll ThirdParty\%BIN_DIR%
copy ThirdParty\pthreads\include\*.h include\

call BuildGravityVSCommon.bat
if errorlevel 1 goto build_fail
@echo Build Succeeded

goto end

:build_fail
@echo Build Failed

:end
@cd %OLD_CD%