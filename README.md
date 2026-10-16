# Intel 8051 Microprocessor Emulator in Modern C++

A robust, high-performance Intel 8051 microprocessor emulator written in C++17. Features complete instruction set coverage (255 opcodes), memory architecture modeling (CODE, internal RAM, SFR, XDATA, bit-addressable RAM), a disassembler, an interactive CLI debugger, an Intel HEX / binary loader, and a unit test suite.

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

- **Program Loaders**:
  - **Intel HEX (`.hex`)**: Full parser with checksum validation and record type handling (00, 01, 02, 04).
  - **Raw Binary (`.bin`)**: Configurable base address loading.

- **Integrated Disassembler & Interactive Debugger**:
  - Disassembles any opcode with mnemonics, operands, raw hex bytes, and cycle counts.
  - Interactive REPL debugger with stepping, breakpoints, disassembly view, register inspection, and memory dumps.
  - Non-interactive batch execution with instruction tracing (`--trace`).

- **Automated Test Suite**:
  - CTest / unit tests covering ALU arithmetic, flags, bit logic, jumps, stack, calls, and loaders.

---

## 🚀 Building the Project

### Prerequisites
- C++17 compliant compiler (`g++` >= 9, `clang++` >= 10, or MSVC)
- CMake >= 3.15

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

### Interactive Debugger Mode
When launched in an interactive terminal without arguments, the emulator loads an internal demonstration program and launches the REPL debugger:

```bash
./build/8051_emulator
```

#### Debugger Commands
| Command | Description |
|---|---|
| `step`, `s [N]` | Step 1 or N instructions |
| `run`, `r`, `c` | Continue execution until breakpoint or halt |
| `break`, `b <addr>` | Set a breakpoint at address (hex or decimal) |
| `del`, `d <addr>` | Remove a breakpoint |
| `breaks` | List all active breakpoints |
| `reg` | Display registers (PC, ACC, B, DPTR, SP, PSW, R0-R7) and next instruction |
| `disasm`, `u [addr] [N]` | Disassemble N instructions from address (default: PC) |
| `dump [ram\|sfr\|code\|x]` | Display formatted memory dump |
| `load <hex\|bin> <file>` | Load an Intel HEX or binary firmware file |
| `reset` | Reset CPU and memory to power-on state |
| `help`, `?` | Display command help |
| `quit`, `q` | Exit debugger |

### Executing Firmware Directly
To run an Intel HEX or binary file with execution tracing:

```bash
./build/8051_emulator firmware.hex --run --trace --steps 1000
```

Command-line flags:
- `--run`, `-r`: Run firmware automatically instead of launching the REPL shell.
- `--trace`, `-t`: Print each disassembled instruction as it executes.
- `--steps`, `-s <N>`: Maximum instructions to execute (default: 10000).
- `--repl`, `-i`: Force interactive REPL shell after loading firmware.
- `--help`, `-h`: Show usage information.

---

## 📁 Project Architecture

```
.
├── CMakeLists.txt             # Build configuration, static library, and tests
├── include/
│   ├── cpu.hpp                # CPU core definition, register accessors, ALU & execution
│   ├── debugger.hpp           # Interactive REPL shell and breakpoint manager
│   ├── disassembler.hpp       # Full 8051 instruction disassembler
│   ├── memory.hpp             # Memory subsystem (CODE, RAM, SFR, XDATA, bit-addressing)
│   └── sfr.hpp                # SFR memory addresses and PSW bit masks
├── src/
│   ├── cpu.cpp                # 255-opcode decoder, flags logic, and cycle accounting
│   ├── debugger.cpp           # REPL implementation and interactive CLI commands
│   ├── disassembler.cpp       # Opcode disassembler implementation
│   ├── main.cpp               # CLI argument parser and entry point
│   └── memory.cpp             # Memory operations, HEX parser, and memory dump formatters
└── tests/
    └── test_cpu.cpp           # Unit tests for ALU, stack, jumps, bit logic, and loaders
```

---

## 🧪 Running Unit Tests

```bash
# Run via CTest
ctest --test-dir build --verbose

# Or run directly
./build/test_cpu
```
