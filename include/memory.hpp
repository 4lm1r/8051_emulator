#pragma once 
#include <vector>
#include <string>
#include <cstdint>

class Memory {
public:
    Memory();
    
    // Reset memory to initial power-on state
    void reset();

    // CODE Memory (ROM - up to 64KB)
    uint8_t read_code(uint16_t address) const;
    void write_code(uint16_t address, uint8_t value);

    // Direct RAM & SFR addressing (0x00-0x7F: RAM; 0x80-0xFF: SFR)
    uint8_t read_direct(uint8_t address) const;
    void write_direct(uint8_t address, uint8_t value);

    // Backward-compatible aliases for direct RAM
    uint8_t read_data(uint8_t address) const { return read_direct(address); }
    void write_data(uint8_t address, uint8_t value) { write_direct(address, value); }

    // Indirect RAM addressing (0x00-0xFF, accessed via @R0 or @R1)
    uint8_t read_indirect(uint8_t address) const;
    void write_indirect(uint8_t address, uint8_t value);

    // External Data Memory (XDATA - up to 64KB)
    uint8_t read_xdata(uint16_t address) const;
    void write_xdata(uint16_t address, uint8_t value);

    // Bit-addressable memory (0x00-0x7F: RAM 0x20-0x2F; 0x80-0xFF: Bit-addressable SFRs)
    bool read_bit(uint8_t bit_address) const;
    void write_bit(uint8_t bit_address, bool value);
    void set_bit(uint8_t bit_address);
    void clear_bit(uint8_t bit_address);
    void complement_bit(uint8_t bit_address);

    // Binary / HEX program loading
    bool load_hex(const std::string& filepath);
    bool load_bin(const std::string& filepath, uint16_t base_address = 0x0000);

    // Dumps for diagnostics and debugging
    void dump() const; // Default 256 bytes direct RAM + SFR dump
    void dump_internal_ram() const;
    void dump_sfr() const;
    void dump_xdata(uint16_t start = 0, uint16_t length = 128) const;
    void dump_code(uint16_t start = 0, uint16_t length = 128) const;

    // Helper for parity calculation
    static bool compute_parity(uint8_t val);

private:
    std::vector<uint8_t> code_mem;   // 64KB (ROM)
    std::vector<uint8_t> data_mem;   // 256 bytes (Internal RAM)
    std::vector<uint8_t> sfr_mem;    // 128 bytes (SFRs, direct 0x80-0xFF)
    std::vector<uint8_t> xdata_mem;  // 64KB (External RAM)

    bool resolve_bit_address(uint8_t bit_address, uint8_t& target_byte_addr, uint8_t& bit_idx, bool& is_sfr) const;
};
