@echo off
pushd %~dp0
echo Compiling shader and packing into header: %~2
setlocal

fxc.exe  /nologo /E main /T cs_5_0 /Fo "%1" %2
bin2header.exe "%1"

echo Generating shader reflection data for %1
ShaderReflector "%1" "%1_refl.h"

echo /* Concatenating shader reflector output:*/ >> "%1.h"
type "%1_refl.h" >> "%1.h"
del "%1_refl.h"

del "%1"
endlocal
popd
