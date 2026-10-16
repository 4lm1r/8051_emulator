# Intel 8051 Microprocessor Emulator in Modern C++

A robust, high-performance Intel 8051 microprocessor emulator written in C++17. Features complete instruction set coverage (255 opcodes), memory architecture modeling (CODE, internal RAM, SFR, XDATA, bit-addressable RAM), a disassembler, an interactive **ncurses Terminal User Interface (TUI)**, a classic CLI debugger, an Intel HEX / binary loader, and an automated unit test suite.

---

## 🌟 Key Features

- **Full Instruction Set Architecture (ISA)**:
  - All 255 valid 8051 opcodes implemented.
  - Accurate ALU arithmetic with full flag evaluation:
    - **CY** (Carry Flag)
    - **AC** (Auxiliary Carry Flag for BCD arithmetic)
    - **OV** (Two's complement Overflow Flag)
    - **P** (Hardware Parity bit dynamically updated with Accumulator)
  - Full BCD decimal arithmetic support (`DA A`).
  - Hardware multiplication and division (`MUL AB`, `DIV AB`).
  - Complete Boolean / Bit-manipulation instruction set (`SETB`, `CLR`, `CPL`, `MOV C, bit`, `MOV bit, C`, `ANL C, bit`, `ORL C, bit`, etc.).
  - Program control and subroutine calls (`LJMP`, `AJMP`, `SJMP`, `JMP @A+DPTR`, `LCALL`, `ACALL`, `RET`, `RETI`, `CJNE`, `DJNZ`, `JZ`, `JNZ`, `JC`, `JNC`, `JB`, `JNB`, `JBC`).
  - Cycle-accurate machine cycle counting per instruction (1, 2, or 4 cycles).

- **Complete Memory Subsystem**:
  - **CODE**: 64 KB program memory space.
  - **Internal RAM**: 256 bytes (Bank 0-3 registers `0x00-0x1F`, bit-addressable RAM `0x20-0x2F`, scratchpad RAM `0x30-0x7F`, upper indirect RAM `0x80-0xFF`).
  - **SFR**: 128 bytes Special Function Register space (direct addressing `0x80-0xFF`).
  - **Bit-addressable Memory**: Seamlessly addresses RAM bits (`0x00-0x7F`) and SFR bits (`0x80-0xFF`).
  - **XDATA**: 64 KB external data memory space (`MOVX @DPTR`, `MOVX @Ri`).

- **Interactive ncurses Terminal User Interface (TUI)**:
  - **Top Title Bar**: Displays loaded ROM/file, execution state (RUNNING, PAUSED, HALTED), and machine cycles.
  - **Left Reverse-Highlighted Menu & Register HUD**:
    - Selectable action menu with inverse/reverse highlight (`Step`, `Run/Pause`, `Reset`, `Disassembly`, `Internal RAM`, `SFRs`, `XDATA`, `Breakpoints`, `Log`, `Quit`).
    - Live hardware register monitor (`PC`, `SP`, `DPTR`, `ACC`, `B`, `PSW`, flags, and `R0-R7` bank).
  - **Main Results Window**: Dynamic multi-view display:
    - **Disassembly View**: Code listing with current `PC` indicator (`=>`), breakpoint markers (`[*]`), and cycle counts.
    - **Internal RAM View**: 16-byte hex + ASCII grid with register bank and bit-RAM annotations.
    - **SFR View**: Special Function Registers with hex, decimal, binary, and bit descriptions.
    - **External XDATA View**: 64 KB memory inspector with page scrolling.
    - **Breakpoints View**: Manage active breakpoints.
    - **Log View**: Historical execution trace and event log.
  - **Bottom Status & Message Bar**: Real-time status, upcoming instruction, and alert messages.
  - **Bottom Command Bar**: Interactive command prompt (`:`) accepting debugger commands.

- **Program Loaders**:
  - **Intel HEX (`.hex`)**: Full parser with checksum validation and record type handling (00, 01, 02, 04).
  - **Raw Binary (`.bin`)**: Configurable base address loading.

- **Automated Test Suite**:
  - CTest / unit tests covering ALU arithmetic, flags, bit logic, jumps, stack, calls, and loaders.

---

## 🖥️ TUI Layout Overview

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ 8051 MICROPROCESSOR EMULATOR             [ROM: Built-in Demo]  State: PAUSED  Cycles: 15             │
├──────────────────────────┬───────────────────────────────────────────────────────────────────────────┤
│ [ MENU & ACTIONS ]       │ [ DISASSEMBLY / CODE VIEW ] (PgUp/PgDn to scroll, 'c' to center)          │
│ > Step (1 inst)  F7/Spc  │    000C:  C0 E0      PUSH   ACC                        [2 cycles]         │
│   Run / Pause    F5/r    │    000E:  7F 03      MOV    R7, #0x03                  [1 cycles]         │
│   Reset CPU      F9      │    0010:  04         INC    A                          [1 cycles]         │
│   Disassembly    F1      │ => 0011:  DF FD      DJNZ   R7, 0x0010                 [2 cycles]         │
│   Internal RAM   F2      │    0013:  D0 30      POP    0x30                       [2 cycles]         │
│   SFR Registers  F3      │    0015:  80 FE      SJMP   0x0015                     [2 cycles]         │
│   External XDATA F4      │                                                                           │
│ [ CPU REGISTERS ]        │                                                                           │
│ PC: 0x0011   SP: 0x08    │                                                                           │
│ DPTR: 0x0000             │                                                                           │
│ ACC: 0x01     B: 0x02    │                                                                           │
│ PSW: 0x45   [Bank 0]     │                                                                           │
│ CY:0 AC:1 OV:1 P:1       │                                                                           │
│ [ R0 - R7 ]              │                                                                           │
│ R0:0x55  R1:0x2B         │                                                                           │
│ R2:0x00  R3:0x00         │                                                                           │
│ R4:0x00  R5:0x00         │                                                                           │
│ R6:0x00  R7:0x03         │                                                                           │
├──────────────────────────┴───────────────────────────────────────────────────────────────────────────┤
│ [STATUS] PC: 0x0011 | Next: DJNZ R7, 0x0010 | Info: Stepped 1 instruction (2 cycles).                │
├──────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ [COMMAND] Press ':' to enter command, Space/F7:Step, F5:Run, F1-F4:Views, q:Quit                     │
└──────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 🚀 Building the Project

### Prerequisites
- C++17 compliant compiler (`g++` >= 9, `clang++` >= 10, or MSVC)
- CMake >= 3.15
- `ncurses` development library (`libncurses-dev` on Debian/Ubuntu, `ncurses` on Arch/Fedora)

### Build Instructions

```bash
# Configure build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build emulator and test suite
cmake --build build

# Run automated tests
ctest --test-dir build --output-on-failure
```

---

## 💻 Running the Emulator

### Interactive TUI Mode (Default)
When run in an interactive terminal, the emulator automatically opens the full ncurses interface:

```bash
./build/8051_emulator
# Or with a firmware file:
./build/8051_emulator firmware.hex
```

#### TUI Hotkeys & Navigation
| Key | Action |
|---|---|
| `Space` or `s` or `F7` | Step 1 instruction |
| `r` or `F5` | Toggle continuous run / pause |
| `F9` | Reset CPU and memory to power-on state |
| `Up` / `Down` | Navigate reverse menu items |
| `Enter` | Activate selected menu item |
| `F1` or `1` | Switch main view to **Disassembly** |
| `F2` or `2` | Switch main view to **Internal RAM** |
| `F3` or `3` | Switch main view to **SFR Registers** |
| `F4` or `4` | Switch main view to **External XDATA** |
| `F8` | Switch main view to **Breakpoints** |
| `L` | Switch main view to **Execution Log** |
| `c` | Center disassembly view on current `PC` |
| `PgUp` / `PgDn` | Scroll disassembly or memory dump |
| `:` or `/` | Enter bottom command line mode |
| `q` or `ESC` | Quit emulator |

#### Commands (typed into bottom `:` prompt)
- `:step [n]` or `:s [n]` - Step $n$ instructions
- `:run` or `:r` - Run continuously until breakpoint or pause
- `:b <address>` - Set breakpoint (e.g. `:b 0x0010` or `:b 16`)
- `:del <address>` - Remove breakpoint
- `:reset` - Reset CPU
- `:view <disasm|ram|sfr|xdata|bp|log>` - Switch active view
- `:load <hex|bin> <filepath>` - Load firmware file
- `:quit` or `:q` - Exit

---

### Command-Line Modes & Batch Execution

```bash
# Run classic line-based CLI debugger shell
./build/8051_emulator --cli

# Run firmware in batch mode with instruction tracing
./build/8051_emulator firmware.hex --run --trace --steps 1000

# Show usage and available flags
./build/8051_emulator --help
```

---

## 📁 Project Architecture

```
.
├── CMakeLists.txt             # Build configuration, Curses integration, and CTest
├── include/
│   ├── cpu.hpp                # CPU core, ALU, 255-opcode dispatch, registers
│   ├── debugger.hpp           # Debugger core and breakpoint manager
│   ├── disassembler.hpp       # Full 8051 instruction disassembler
│   ├── memory.hpp             # Memory subsystem (CODE, RAM, SFR, XDATA, bit-addressing)
│   ├── sfr.hpp                # SFR memory addresses and PSW bit masks
│   └── tui.hpp                # ncurses Terminal User Interface header
├── src/
│   ├── cpu.cpp                # 8051 opcode execution and cycle accounting
│   ├── debugger.cpp           # REPL debugger implementation
│   ├── disassembler.cpp       # Opcode disassembler implementation
│   ├── main.cpp               # CLI entry point and mode dispatcher
│   ├── memory.cpp             # Memory spaces, Intel HEX parser, and memory formatters
│   └── tui.cpp                # ncurses TUI windows, reverse menu, rendering, and event loop
└── tests/
    └── test_cpu.cpp           # Unit tests for ALU, stack, jumps, bit logic, and loaders
```

---

## 🧪 Running Unit Tests

```bash
ctest --test-dir build --verbose
# Or run test binary directly:
./build/test_cpu
```
