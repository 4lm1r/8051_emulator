#pragma once
#include "cpu.hpp"
#include "memory.hpp"
#include <set>
#include <string>

class Debugger {
public:
    Debugger(CPU& cpu, Memory& mem);

    // Starts the interactive REPL debugger
    void run_repl();

    // Batch execution helper
    void run_batch(uint64_t max_steps, bool trace);

    // Breakpoint management
    void add_breakpoint(uint16_t addr);
    void remove_breakpoint(uint16_t addr);
    void list_breakpoints() const;
    bool has_breakpoint(uint16_t addr) const;
    const std::set<uint16_t>& get_breakpoints() const { return breakpoints; }
    void clear_breakpoints() { breakpoints.clear(); }

private:
    CPU& cpu;
    Memory& memory;
    std::set<uint16_t> breakpoints;

    void handle_command(const std::string& line, bool& should_exit);
    void print_help() const;
    void disassemble_range(uint16_t start_addr, size_t count) const;
};
