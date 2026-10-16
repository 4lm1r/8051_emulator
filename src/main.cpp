#include "memory.hpp"
#include "cpu.hpp"
#include "debugger.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

void load_demo_program(Memory& memory) {
    // Demo program demonstrating 8051 features:
    // 1. Data transfer to registers
    // 2. Arithmetic addition with flags
    // 3. Multiplication
    // 4. Bit manipulation
    // 5. Stack push and pop
    // 6. Loop using DJNZ

    uint16_t addr = 0x0000;
    auto emit = [&](uint8_t byte) {
        memory.write_code(addr++, byte);
    };

    // 0000: MOV R0, #0x55
    emit(0x78); emit(0x55);
    // 0002: MOV R1, #0x2B
    emit(0x79); emit(0x2B);
    // 0004: MOV A, R0
    emit(0xE8);
    // 0005: ADD A, R1       ; ACC = 0x55 + 0x2B = 0x80 (OV and AC flags set)
    emit(0x29);
    // 0006: MOV B, #0x04
    emit(0x75); emit(0xF0); emit(0x04);
    // 0009: MUL AB          ; ACC = (0x80 * 4) & 0xFF = 0x00, B = 0x02, OV = 1
    emit(0xA4);
    // 000A: SETB 0x20.0     ; Set bit 0x00 (RAM 0x20, bit 0)
    emit(0xD2); emit(0x00);
    // 000C: PUSH ACC        ; Push ACC to stack
    emit(0xC0); emit(0xE0);
    // 000E: MOV R7, #0x03   ; Loop counter
    emit(0x7F); emit(0x03);
    // 0010: INC A           ; Loop body
    emit(0x04);
    // 0011: DJNZ R7, -3     ; Jump back to INC A (0x0010) while R7 != 0
    emit(0xDF); emit(static_cast<uint8_t>(-3));
    // 0013: POP 0x30        ; Pop stack into RAM direct 0x30
    emit(0xD0); emit(0x30);
    // 0015: SJMP -2         ; Infinite loop
    emit(0x80); emit(static_cast<uint8_t>(-2));
}

int main(int argc, char* argv[]) {
    Memory memory;
    CPU cpu(memory);
    Debugger dbg(cpu, memory);

    std::string filename = "";
    bool run_mode = false;
    bool trace_mode = false;
    bool force_repl = false;
    uint64_t max_steps = 10000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--run" || arg == "-r") {
            run_mode = true;
        } else if (arg == "--trace" || arg == "-t") {
            trace_mode = true;
            run_mode = true;
        } else if (arg == "--repl" || arg == "-i") {
            force_repl = true;
        } else if (arg == "--steps" || arg == "-s") {
            if (i + 1 < argc) {
                max_steps = std::stoull(argv[++i]);
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options] [firmware.hex | firmware.bin]\n\n"
                      << "Options:\n"
                      << "  --run, -r          Run automatically instead of opening REPL\n"
                      << "  --trace, -t        Trace each instruction during execution\n"
                      << "  --steps, -s <N>    Maximum steps to execute (default: 10000)\n"
                      << "  --repl, -i         Force interactive REPL after loading\n"
                      << "  --help, -h         Display this help message\n";
            return 0;
        } else if (arg[0] != '-') {
            filename = arg;
        }
    }

    if (!filename.empty()) {
        bool loaded = false;
        if (filename.length() >= 4 && filename.substr(filename.length() - 4) == ".hex") {
            loaded = memory.load_hex(filename);
        } else {
            loaded = memory.load_bin(filename, 0x0000);
        }

        if (!loaded) {
            std::cerr << "Failed to load program file: " << filename << std::endl;
            return 1;
        }
        std::cout << "Loaded: " << filename << std::endl;
    } else {
        std::cout << "No program file specified. Loading built-in 8051 demonstration program...\n";
        load_demo_program(memory);
    }

    bool is_interactive = isatty(fileno(stdin));

    if (run_mode && !force_repl) {
        dbg.run_batch(max_steps, trace_mode);
        return 0;
    }

    if (is_interactive || force_repl) {
        dbg.run_repl();
    } else {
        // Non-interactive fallback: execute demo steps and dump results
        std::cout << "\n--- Initial CPU State ---\n";
        cpu.print_status();

        std::cout << "\n--- Stepping 10 instructions ---\n";
        for (int i = 0; i < 10; ++i) {
            std::cout << "Step " << (i + 1) << ": " << cpu.disassemble_current() << "\n";
            cpu.step();
        }

        std::cout << "\n--- Final CPU State ---\n";
        cpu.print_status();

        std::cout << "\n--- Internal RAM & SFRs ---\n";
        memory.dump();
    }

    return 0;
}
