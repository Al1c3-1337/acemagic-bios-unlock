$ErrorActionPreference = 'Stop'
$rxBuildDir = $PSScriptRoot
$rxCompiler = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe'
$rxLinker = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\lld-link.exe'
& $rxCompiler --target=x86_64-pc-win32-coff -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -fno-builtin -O1 -Wall -Wextra -Werror -I (Join-Path $rxBuildDir 'edk2_include') -I (Join-Path $rxBuildDir 'edk2_include\X64') -c (Join-Path $rxBuildDir 'rx16_menu.c') -o (Join-Path $rxBuildDir 'rx16_menu.obj')
if ($LASTEXITCODE -ne 0) { throw 'EFI compile failed' }
& $rxLinker /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x64 "/out:$rxBuildDir\RX16Menu.efi" (Join-Path $rxBuildDir 'rx16_menu.obj')
if ($LASTEXITCODE -ne 0) { throw 'EFI link failed' }
& $rxCompiler --target=x86_64-pc-win32-coff -x c -DRX16_HOST_TEST -ffreestanding -fno-stack-protector -fno-builtin -O1 -Wall -Wextra -Werror -c (Join-Path $rxBuildDir 'rx16_core.h') -o (Join-Path $rxBuildDir 'rx16_core.obj')
if ($LASTEXITCODE -ne 0) { throw 'Test library compile failed' }
& $rxLinker /dll /noentry /nodefaultlib /machine:x64 "/out:$rxBuildDir\rx16_core.dll" (Join-Path $rxBuildDir 'rx16_core.obj')
if ($LASTEXITCODE -ne 0) { throw 'Test library link failed' }
