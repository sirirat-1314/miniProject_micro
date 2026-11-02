"""Compile the real application with hardware stubs and execute ARM code in Unicorn."""
import pathlib
import subprocess
import sys

here = pathlib.Path(__file__).resolve().parent
project = here.parent
sys.path.insert(0, str(here / '.deps'))
try:
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
    from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0
except ImportError:
    raise SystemExit('Install test dependency: python -m pip install --target tests/.deps unicorn==2.1.4')

gcc = next(pathlib.Path('C:/ST').rglob('arm-none-eabi-gcc.exe'))
out = here / 'build'
out.mkdir(exist_ok=True)
elf = out / 'test_energy.elf'
binary = out / 'test_energy.bin'
flags = ['-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', '-Os', '-g',
         '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections',
         '-nostartfiles', '--specs=nosys.specs', '--specs=nano.specs',
         '-Wl,--gc-sections', '-Wl,-e,test_main', '-Wl,-Ttext=0x08000000',
         '-Wl,-Tdata=0x20000000', '-Wl,--section-start=.bss=0x20001000']
for folder in ['Inc', 'Application/Inc', 'Drivers/Inc']:
    flags += ['-I', str(project / folder)]
subprocess.run([str(gcc), *flags, str(here / 'test_energy.c'), '-o', str(elf)], check=True)
subprocess.run([str(gcc.with_name('arm-none-eabi-objcopy.exe')), '-O', 'binary',
                '-j', '.text', '-j', '.rodata', str(elf), str(binary)], check=True)
nm = subprocess.check_output([str(gcc.with_name('arm-none-eabi-nm.exe')), str(elf)], text=True)
symbols = {}
for line in nm.splitlines():
    parts = line.split()
    if len(parts) == 3:
        symbols[parts[2]] = int(parts[0], 16)
cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
cpu.mem_map(0x08000000, 0x10000)
cpu.mem_map(0x20000000, 0x20000)
cpu.mem_write(0x08000000, binary.read_bytes())
cpu.reg_write(UC_ARM_REG_SP, 0x2001fff0)
stop = 0x0800f000
cpu.reg_write(UC_ARM_REG_LR, stop | 1)
completed = [False]
def on_code(uc, address, size, _):
    if address == stop:
        completed[0] = True
        uc.emu_stop()
cpu.hook_add(UC_HOOK_CODE, on_code)
cpu.emu_start(symbols['test_main'] | 1, stop + 2, count=2000000)
if not completed[0]:
    raise SystemExit('FAIL: test entry did not return within instruction limit')
result = cpu.reg_read(UC_ARM_REG_R0)
if result:
    raise SystemExit(f'FAIL: assertion at test_energy.c:{result}')
checks = int.from_bytes(cpu.mem_read(symbols['test_checks'], 4), 'little')
print(f'PASS: {checks} checks on compiled ARM application: Eco entry/reopen/reentry, real tick loop, buttons/UART, energy costs, empty lock, recharge.')
