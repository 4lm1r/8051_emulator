#pragma once
#include <string>
#include <cstdint>

class Memory;

struct DisassemblyResult {
    uint16_t address = 0;
    uint8_t opcode = 0;
    std::string mnemonic;
    std::string operands;
    uint8_t length = 1;
    uint8_t cycles = 1;
    std::string bytes_hex;
    std::string text;
};

class Disassembler {
public:
    static DisassemblyResult disassemble(const Memory& mem, uint16_t address);
    static std::string format_bit_name(uint8_t bit_address);
    static std::string format_direct_name(uint8_t direct_address);
};
