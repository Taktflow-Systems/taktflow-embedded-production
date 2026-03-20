# foxBMS 2 Integration — Findings & Setup (2026-03-20)

## foxBMS Unit Test Infrastructure (Windows PC)

### Tools Installed
- **Ruby 3.4.4**: `C:\foxbms\Ruby\Ruby34-x64\`
- **Ceedling 1.0.1**: `C:\foxbms\Ceedling\1.0.1\bin\`
- **GCC 15.2 (MSYS2)**: `C:\tools\msys64\ucrt64\bin\` — foxBMS expects GCC 8.1 but 15 works with warning fixes
- **gcovr**: `C:\Users\andao\AppData\Local\Programs\Python\Python312\Scripts\gcovr.exe`
- **Python 3.12.10**: `C:\Users\andao\AppData\Local\Programs\Python\Python312\python.exe`

### GCC 15 Compatibility Fixes (conf/unit/app_project_win32.yml)
Added to `:flags: :test: :compile: :*:`:
```yaml
- -Wno-enum-conversion        # enum type mismatch in mock returns
- -Wno-incompatible-pointer-types  # pointer type differences
- -Wno-attributes             # section attribute conflicts in FreeRTOS MPU
- -Wno-unterminated-string-initialization  # string init without NUL
- -Wno-absolute-value         # abs() with time_t argument
- -fcommon                    # multiple definition of tentative globals
```

### paths_win32.txt Changes
Changed mingw path from `C:\foxbms\mingw-w64\x86_64-8.1.0-release-posix-seh-rt_v6-rev0\bin` to `C:\tools\msys64\ucrt64\bin`

### Build Commands
```bash
# Must add gcovr to PATH first
export PATH="/c/Users/andao/AppData/Local/Programs/Python/Python312/Scripts:$PATH"

# Configure (once)
python fox.py waf configure

# Run all tests (clean first if stale cache suspected)
python fox.py waf clean_app_host_unit_test
python fox.py ceedling test:all

# Run single test
python fox.py ceedling test:test_bms
```

### Critical Lesson: Always Clean Before Full Test Run
Stale Ceedling build cache causes mock generation to fail silently for files with hyphens in names (e.g., `can_cbs_tx_crash-dump.h`). After `clean_app_host_unit_test`, all 183+ tests pass. Without clean, 11 tests fail with "No such file" for mock headers.

### Test Results: 100% PASS (after clean + GCC 15 fixes)
All non-AFE-vendor tests pass. ADI ADES1830 tests also pass after clean.

## foxBMS CAN Message Map (for Plant Model)

### RX: Plant → foxBMS (must send)
| CAN ID | Message | Period | Endianness | DLC |
|--------|---------|--------|------------|-----|
| 0x210 | BMS State Request | aperiodic | Big | 8 |
| 0x270 | AFE Cell Voltages | periodic | Big | 8 |
| 0x280 | AFE Cell Temperatures | periodic | Big | 8 |
| 0x521 | IVT String0 Current | periodic | Big | 6 |
| 0x522 | IVT String0 Voltage1 | periodic | Big | 6 |

### TX: foxBMS → Plant (will receive)
| CAN ID | Message | Content |
|--------|---------|---------|
| 0x220 | BMS State | Operating state |
| 0x221 | BMS State Details | Detailed state info |
| 0x231 | Pack Min/Max Values | Min/max cell voltages |
| 0x232 | Pack Limits | Current/voltage limits |
| 0x233 | Pack Values P0 | Pack voltage, current |
| 0x234 | Pack Values P1 | Additional pack data |
| 0x235 | Pack State Estimation | SOC/SOE |
| 0x240 | String State | Contactor/string state |
| 0x241 | String Min/Max | Per-string min/max |
| 0x243 | String Values P0 | String voltage |
| 0x244 | String Values P1 | String current |
| 0x245 | String State Estimation | Per-string SOC |
| 0x250 | Cell Voltages | Individual cell V |
| 0x260 | Cell Temperatures | Individual cell T |

### Cell Voltage Message Format (0x270)
- Mux byte (bits 7:0): selects group of 4 cells
- 4 cell voltages: 13 bits each at bit positions 11, 30, 33, 52
- 4 invalid flags: 1 bit each at bit positions 12, 13, 14, 15
- Endianness: Big

### BMS Configuration (conf/bms/bms.json)
- AFE: `debug/can` (reads cell data from CAN, not SPI)
- SOC: counting method
- SOE: counting method
- SOF: trapezoid method
- Balancing: voltage-based
- IMD: none (insulation monitoring disabled)

## foxBMS Hardware Port (TMS570 LaunchPad) — Status

### What Works
- foxBMS builds: `fox.py waf build_app_embedded` → `foxbms.elf` (BE32)
- HALCoGen partially adapted: 16 MHz crystal, CAN1 500 kbps, SCI1 115200, GIOB6/7

### What Blocks Boot
- **PLL NR divider wrong**: HALCoGen NR=10 with 16 MHz crystal → 240 MHz (should be 300 MHz). Fix: NR=8.
- **Driver stubs missing**: SBC, SPS, PEX, FRAM hang on missing SPI/I2C hardware
- **LED driver**: uses HET1[1], LaunchPad has GIOB[6:7]

### TMS570 LaunchPad Hardware Findings
- **DCAN1 TX/RX**: J10 pin 44 (RX), J10 pin 45 (TX) — NOT J9 pins 20/21 as markdown summaries claimed
- **CAN transceiver**: SN65HVD230 module, needs 3.3V VCC
- **BE32 required**: `-Wl,--be32` for all TMS570 linking. BE8 ELF = doesn't run, no error
- **sc_startup.S required**: HALCoGen's HL_sys_startup.c doesn't work with tiarmclang on LaunchPad
- **Entry point**: `_c_int00` (not `main()`) — sc_startup.S branches to it
- **HALCoGen all RX mailboxes**: Must manually set Dir=TX in IF1ARB after canInit()
- **DCAN1 loopback PASS, external TX PASS** (after VCC fix)
- **Production SC blinks but no CAN TX** — separate bug in dcan1_transmit(), not yet debugged

## foxBMS on Ubuntu Laptop
- Cloned at `/home/an-dao/foxbms-2` (v1.10.0)
- Python venv: `/home/an-dao/foxbms-2/.venv/`
- Ruby 3.2.3 + Ceedling 1.0.1 installed
- waf configure fails (no TI CGT) — needs patching for host-only build

## foxBMS POSIX vECU — WORKING (2026-03-20)

### Achievement
foxBMS 2 v1.10.0 running as a native Linux x86-64 process on Ubuntu laptop.
Sends CAN messages (0x220 BMS State, 0x600/0x601 debug AFE) on vcan0.

### How It Works
- FreeRTOS POSIX port (ThirdParty/GCC/Posix) provides real task scheduling via pthreads
- 7 FreeRTOS threads created: TaskEngine, Task1ms, Task10ms, Task100ms, TaskAlgo, TaskAFE, TaskI2C
- foxBMS's own main.c drives initialization (not custom entry point)
- HAL stubs replace hardware register access (IO, CRC, SPI, I2C, ADC, etc.)
- SocketCAN via canInit() stub opens vcan0 automatically

### Files (in foxbms-2/posix/)
- `Makefile` — builds 170+ foxBMS source files with GCC
- `hal_stubs_posix.c` — 50+ HAL function stubs
- `posix_overrides.h` — force-included, stubs `__asm` and `__curpc`
- `foxbms_posix_main.c` — SocketCAN helper functions
- `config_cpu_clock_hz.h` — defines CPU clock for FreeRTOS config

### Excluded Source Files (variants we don't use)
- AFE: adi, nxp, maxim, ltc, ti, debug/default (keep debug/can only)
- IMD: bender, no-imd (keep imd.c with none config)
- SOC/SOE/SOH: debug and none variants (keep counting)
- TS: all except epcos b57251v5103j060
- Others: ethernet, bal/history, bal/none, polynomial, fstartup, io.c, crc.c

### Build & Run
```bash
cd /home/an-dao/foxbms-2/posix
make -j4                              # Build
sudo ip link add vcan0 type vcan      # Create virtual CAN
sudo ip link set vcan0 up
./foxbms-vecu                         # Run (Ctrl+C to stop)
candump vcan0                         # See CAN output in another terminal
```

### Environment Variable
`FOXBMS_CAN_IF=can0` to use real CAN instead of vcan0
