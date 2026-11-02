param([ValidateSet(0,1,2,3)][int]$TempModel = 3)
$ErrorActionPreference = 'Stop'
$projectDir = $PSScriptRoot
$gcc = (Get-ChildItem 'C:/ST' -Filter arm-none-eabi-gcc.exe -Recurse | Select-Object -First 1).FullName
if (!$gcc) { throw 'ARM GCC not found in C:/ST' }
$libraryDir = Join-Path $projectDir 'CMSIS'
$outDir = Join-Path $projectDir 'Firmware'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$flags = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-DSTM32F411xE',"-DTEMP_SENSOR_MODEL=$TempModel",'-Os','-g3','-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror','-std=gnu11',"-I$projectDir/Inc","-I$projectDir/Drivers/Inc","-I$projectDir/Application/Inc","-I$libraryDir/Core/Include","-I$libraryDir/Device/Include")
$objects = @()
$sources = Get-ChildItem "$projectDir/Src", "$projectDir/Drivers/Src", "$projectDir/Application/Src" -Filter *.c
foreach ($source in $sources) {
    $obj = Join-Path $outDir ($source.BaseName + '.o')
    & $gcc @flags -c $source.FullName -o $obj
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    $objects += $obj
}
$startupObj = Join-Path $outDir 'startup.o'
& $gcc @flags -c "$projectDir/Startup/startup_stm32f411retx.s" -o $startupObj
if ($LASTEXITCODE -ne 0) { throw 'Startup failed' }
$objects += $startupObj
$elf = Join-Path $outDir 'Smart_Home_final.elf'
& $gcc @flags @objects "-T$projectDir/STM32F411RETX_FLASH.ld" --specs=nosys.specs --specs=nano.specs '-Wl,--gc-sections' "-Wl,-Map=$outDir/Smart_Home_final.map" -o $elf -lm
if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
& (Join-Path (Split-Path $gcc) 'arm-none-eabi-size.exe') $elf
& (Join-Path (Split-Path $gcc) 'arm-none-eabi-objcopy.exe') -O ihex $elf "$outDir/Smart_Home_final.hex"
if ($LASTEXITCODE -ne 0) { throw 'HEX conversion failed' }



