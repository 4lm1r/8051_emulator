#pragma once
#include "memory.hpp"
#include <cstdint>
#include <string>

class CPU {
public:
    explicit CPU(Memory& mem);

    // Resets CPU state (PC=0, SP=0x07, PSW=0, ACC=0, B=0, DPTR=0, cycles=0)
    void reset();

    // Executes a single instruction and returns the number of machine cycles consumed
    uint8_t step();

    // Program execution control
    uint16_t get_pc() const { return pc; }
    void set_pc(uint16_t new_pc) { pc = new_pc; }
    uint64_t get_cycles() const { return cycles; }
    bool is_halted() const { return halted; }
    void set_halted(bool h) { halted = h; }

    // Direct access to primary SFR registers
    uint8_t get_acc() const;
    void set_acc(uint8_t val);
    uint8_t get_b() const;
    void set_b(uint8_t val);
    uint8_t get_sp() const;
    void set_sp(uint8_t val);
    uint8_t get_psw() const;
    void set_psw(uint8_t val);
    uint16_t get_dptr() const;
    void set_dptr(uint16_t val);

    // PSW Flags
    bool get_cy() const;
    void set_cy(bool v);
    bool get_ac() const;
    void set_ac(bool v);
    bool get_ov() const;
    void set_ov(bool v);
    bool get_p() const;
    uint8_t get_active_bank() const;

    // Working Registers (R0-R7) in current bank
    uint8_t get_register_address(uint8_t reg_num) const;
    uint8_t get_rn(uint8_t reg_num) const;
    void set_rn(uint8_t reg_num, uint8_t val);

    // Indirect Addressing (@R0, @R1)
    uint8_t get_ri(uint8_t ri_num) const;
    void set_ri(uint8_t ri_num, uint8_t val);

    // Stack operations
    void push(uint8_t val);
    uint8_t pop();

    // ALU operations
    uint8_t alu_add(uint8_t a, uint8_t b, bool carry_in);
    uint8_t alu_subb(uint8_t a, uint8_t b);

    // Diagnostics & Inspection
    void print_status() const;
    void dump_memory() const;
    std::string disassemble_current() const;

private:
    Memory& memory;
    uint16_t pc;
    uint64_t cycles;
    bool halted;
};
