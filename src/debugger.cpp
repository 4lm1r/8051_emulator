#include "debugger.hpp"
#include "disassembler.hpp"
#include "sfr.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

Debugger::Debugger(CPU& c, Memory& m) : cpu(c), memory(m) {}

void Debugger::add_breakpoint(uint16_t addr) {
    breakpoints.insert(addr);
    std::cout << "Breakpoint set at 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
              << addr << std::nouppercase << std::dec << std::endl;
}

void Debugger::remove_breakpoint(uint16_t addr) {
    if (breakpoints.erase(addr)) {
        std::cout << "Breakpoint removed at 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                  << addr << std::nouppercase << std::dec << std::endl;
    } else {
        std::cout << "No breakpoint at 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                  << addr << std::nouppercase << std::dec << std::endl;
    }
}

void Debugger::list_breakpoints() const {
    if (breakpoints.empty()) {
        std::cout << "No breakpoints active." << std::endl;
        return;
    }
    std::cout << "Active breakpoints:\n";
    for (uint16_t bp : breakpoints) {
        std::cout << "  - 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << bp << "\n";
    }
    std::cout << std::nouppercase << std::dec;
}

bool Debugger::has_breakpoint(uint16_t addr) const {
    return breakpoints.find(addr) != breakpoints.end();
}

static uint32_t parse_num(const std::string& str) {
    if (str.rfind("0x", 0) == 0 || str.rfind("0X", 0) == 0) {
        return std::stoul(str.substr(2), nullptr, 16);
    }
    // Check if contains hex digits A-F
    bool has_hex_char = false;
    for (char ch : str) {
        if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
            has_hex_char = true;
            break;
        }
    }
    if (has_hex_char) {
        return std::stoul(str, nullptr, 16);
    }
    return std::stoul(str, nullptr, 10);
}

void Debugger::disassemble_range(uint16_t start_addr, size_t count) const {
    uint16_t current = start_addr;
    std::cout << "--- Disassembly @ 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << current << " ---\n";
    for (size_t i = 0; i < count; ++i) {
        DisassemblyResult res = Disassembler::disassemble(memory, current);
        bool is_pc = (current == cpu.get_pc());
        bool is_bp = has_breakpoint(current);

        std::cout << (is_pc ? "=> " : "   ")
                  << (is_bp ? "[*] " : "    ")
                  << res.text << "\n";
        current += res.length;
    }
    std::cout << std::nouppercase << std::dec;
}

void Debugger::print_help() const {
    std::cout << "\n================ 8051 EMULATOR DEBUGGER COMMANDS ================\n"
              << "  step, s [n]             - Step 1 or n instructions\n"
              << "  run, r, c               - Run execution until breakpoint or halt\n"
              << "  break, b <addr>         - Set breakpoint at address (hex or dec)\n"
              << "  del, d <addr>           - Remove breakpoint\n"
              << "  breaks                  - List all active breakpoints\n"
              << "  reg                     - Display all registers, flags, and next inst\n"
              << "  disasm, u [addr] [n]    - Disassemble n instructions (default: from PC)\n"
              << "  dump [ram|sfr|code|x]   - Dump memory spaces\n"
              << "  load <hex|bin> <file>   - Load Intel HEX or binary file into CODE memory\n"
              << "  reset                   - Reset CPU and internal memory to power-on state\n"
              << "  help, ?                 - Display this help message\n"
              << "  quit, q                 - Exit emulator\n"
              << "=================================================================\n\n";
}

void Debugger::handle_command(const std::string& line, bool& should_exit) {
    std::istringstream iss(line);
    std::string cmd;
    if (!(iss >> cmd)) return;

    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "q" || cmd == "quit" || cmd == "exit") {
        should_exit = true;
    } else if (cmd == "h" || cmd == "help" || cmd == "?") {
        print_help();
    } else if (cmd == "reg") {
        cpu.print_status();
    } else if (cmd == "s" || cmd == "step") {
        std::string n_str;
        size_t steps = 1;
        if (iss >> n_str) {
            try { steps = parse_num(n_str); } catch (...) { steps = 1; }
        }
        for (size_t i = 0; i < steps; ++i) {
            if (cpu.is_halted()) {
                std::cout << "CPU is halted.\n";
                break;
            }
            cpu.step();
            if (has_breakpoint(cpu.get_pc())) {
                std::cout << "Hit breakpoint at 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                          << cpu.get_pc() << std::nouppercase << std::dec << "\n";
                break;
            }
        }
        cpu.print_status();
    } else if (cmd == "r" || cmd == "run" || cmd == "c" || cmd == "continue") {
        std::cout << "Running... (Press Ctrl+C to interrupt if running long)\n";
        uint64_t step_count = 0;
        constexpr uint64_t max_run_steps = 10000000;
        while (!cpu.is_halted() && step_count < max_run_steps) {
            cpu.step();
            step_count++;
            if (has_breakpoint(cpu.get_pc())) {
                std::cout << "Hit breakpoint at 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                          << cpu.get_pc() << std::nouppercase << std::dec << "\n";
                break;
            }
        }
        if (step_count >= max_run_steps) {
            std::cout << "Stopped after reaching execution safety limit of " << max_run_steps << " steps.\n";
        }
        cpu.print_status();
    } else if (cmd == "b" || cmd == "break") {
        std::string addr_str;
        if (iss >> addr_str) {
            try {
                uint16_t addr = static_cast<uint16_t>(parse_num(addr_str));
                add_breakpoint(addr);
            } catch (...) {
                std::cout << "Invalid address format.\n";
            }
        } else {
            std::cout << "Usage: break <address>\n";
        }
    } else if (cmd == "d" || cmd == "del") {
        std::string addr_str;
        if (iss >> addr_str) {
            try {
                uint16_t addr = static_cast<uint16_t>(parse_num(addr_str));
                remove_breakpoint(addr);
            } catch (...) {
                std::cout << "Invalid address format.\n";
            }
        } else {
            std::cout << "Usage: del <address>\n";
        }
    } else if (cmd == "breaks") {
        list_breakpoints();
    } else if (cmd == "u" || cmd == "disasm") {
        std::string addr_str;
        uint16_t start_addr = cpu.get_pc();
        size_t count = 10;
        if (iss >> addr_str) {
            try { start_addr = static_cast<uint16_t>(parse_num(addr_str)); } catch (...) {}
            std::string cnt_str;
            if (iss >> cnt_str) {
                try { count = parse_num(cnt_str); } catch (...) {}
            }
        }
        disassemble_range(start_addr, count);
    } else if (cmd == "dump") {
        std::string target;
        if (iss >> target) {
            std::transform(target.begin(), target.end(), target.begin(), ::tolower);
            if (target == "ram") {
                memory.dump_internal_ram();
            } else if (target == "sfr") {
                memory.dump_sfr();
            } else if (target == "code") {
                std::string s_str, l_str;
                uint16_t start = 0;
                uint16_t len = 64;
                if (iss >> s_str) { try { start = static_cast<uint16_t>(parse_num(s_str)); } catch (...) {} }
                if (iss >> l_str) { try { len = static_cast<uint16_t>(parse_num(l_str)); } catch (...) {} }
                memory.dump_code(start, len);
            } else if (target == "xdata" || target == "x") {
                std::string s_str, l_str;
                uint16_t start = 0;
                uint16_t len = 64;
                if (iss >> s_str) { try { start = static_cast<uint16_t>(parse_num(s_str)); } catch (...) {} }
                if (iss >> l_str) { try { len = static_cast<uint16_t>(parse_num(l_str)); } catch (...) {} }
                memory.dump_xdata(start, len);
            } else {
                memory.dump();
            }
        } else {
            memory.dump();
        }
    } else if (cmd == "load") {
        std::string type, filepath;
        if (iss >> type >> filepath) {
            std::transform(type.begin(), type.end(), type.begin(), ::tolower);
            if (type == "hex") {
                if (memory.load_hex(filepath)) {
                    std::cout << "Successfully loaded HEX file: " << filepath << "\n";
                }
            } else if (type == "bin") {
                std::string base_str;
                uint16_t base_addr = 0x0000;
                if (iss >> base_str) {
                    try { base_addr = static_cast<uint16_t>(parse_num(base_str)); } catch (...) {}
                }
                if (memory.load_bin(filepath, base_addr)) {
                    std::cout << "Successfully loaded binary file: " << filepath << " at 0x"
                              << std::hex << std::uppercase << base_addr << std::nouppercase << std::dec << "\n";
                }
            } else {
                std::cout << "Unknown load type: " << type << " (use 'hex' or 'bin')\n";
            }
        } else {
            std::cout << "Usage: load <hex|bin> <filepath> [base_address]\n";
        }
    } else if (cmd == "reset") {
        cpu.reset();
        std::cout << "System reset executed.\n";
        cpu.print_status();
    } else {
        std::cout << "Unknown command: '" << cmd << "'. Type 'help' for available commands.\n";
    }
}

void Debugger::run_repl() {
    std::cout << "========================================================\n"
              << "       8051 Microprocessor Emulator & Debugger CLI      \n"
              << "========================================================\n"
              << "Type 'help' for commands, 'step' to step, 'quit' to exit.\n\n";

    cpu.print_status();

    bool should_exit = false;
    std::string line;
    while (!should_exit) {
        std::cout << "(8051-dbg) " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) continue;
        handle_command(line, should_exit);
    }

    std::cout << "Exiting debugger.\n";
}

void Debugger::run_batch(uint64_t max_steps, bool trace) {
    uint64_t steps = 0;
    while (!cpu.is_halted() && (max_steps == 0 || steps < max_steps)) {
        if (has_breakpoint(cpu.get_pc())) {
            std::cout << "Breakpoint hit at 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                      << cpu.get_pc() << std::nouppercase << std::dec << "\n";
            break;
        }

        if (trace) {
            std::cout << "[" << std::dec << std::setw(6) << steps << "] "
                      << cpu.disassemble_current() << "\n";
        }

        cpu.step();
        steps++;
    }

    std::cout << "\nExecution finished after " << std::dec << steps << " steps ("
              << cpu.get_cycles() << " machine cycles).\n";
    cpu.print_status();
}
