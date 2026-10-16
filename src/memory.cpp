#include "memory.hpp"
#include "sfr.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

Memory::Memory() {
    code_mem.resize(65536, 0x00);
    data_mem.resize(256, 0x00);
    sfr_mem.resize(128, 0x00);
    xdata_mem.resize(65536, 0x00);
    reset();
}

void Memory::reset() {
    data_mem.assign(256, 0x00);
    sfr_mem.assign(128, 0x00);
    xdata_mem.assign(65536, 0x00);

    // Standard 8051 Reset Values
    write_direct(sfr::SP, 0x07);   // Stack Pointer defaults to 0x07
    write_direct(sfr::P0, 0xFF);   // Ports default to 0xFF (High)
    write_direct(sfr::P1, 0xFF);
    write_direct(sfr::P2, 0xFF);
    write_direct(sfr::P3, 0xFF);
    write_direct(sfr::ACC, 0x00);
    write_direct(sfr::B, 0x00);
    write_direct(sfr::DPL, 0x00);
    write_direct(sfr::DPH, 0x00);
    write_direct(sfr::PSW, 0x00);
    write_direct(sfr::IP, 0x00);
    write_direct(sfr::IE, 0x00);
    write_direct(sfr::TMOD, 0x00);
    write_direct(sfr::TCON, 0x00);
    write_direct(sfr::TH0, 0x00);
    write_direct(sfr::TL0, 0x00);
    write_direct(sfr::TH1, 0x00);
    write_direct(sfr::TL1, 0x00);
    write_direct(sfr::SCON, 0x00);
    write_direct(sfr::PCON, 0x00);
}

uint8_t Memory::read_code(uint16_t address) const {
    return code_mem[address];
}

void Memory::write_code(uint16_t address, uint8_t value) {
    code_mem[address] = value;
}

bool Memory::compute_parity(uint8_t val) {
    val ^= val >> 4;
    val ^= val >> 2;
    val ^= val >> 1;
    return (val & 1) != 0;
}

uint8_t Memory::read_direct(uint8_t address) const {
    if (address < 0x80) {
        return data_mem[address];
    } else {
        uint8_t val = sfr_mem[address - 0x80];
        if (address == sfr::PSW) {
            bool p = compute_parity(sfr_mem[sfr::ACC - 0x80]);
            val = (val & ~sfr::PSW_P) | (p ? sfr::PSW_P : 0);
        }
        return val;
    }
}

void Memory::write_direct(uint8_t address, uint8_t value) {
    if (address < 0x80) {
        data_mem[address] = value;
    } else {
        if (address == sfr::ACC) {
            sfr_mem[sfr::ACC - 0x80] = value;
            bool p = compute_parity(value);
            if (p) sfr_mem[sfr::PSW - 0x80] |= sfr::PSW_P;
            else sfr_mem[sfr::PSW - 0x80] &= ~sfr::PSW_P;
        } else if (address == sfr::PSW) {
            bool p = compute_parity(sfr_mem[sfr::ACC - 0x80]);
            sfr_mem[sfr::PSW - 0x80] = (value & ~sfr::PSW_P) | (p ? sfr::PSW_P : 0);
        } else {
            sfr_mem[address - 0x80] = value;
        }
    }
}

uint8_t Memory::read_indirect(uint8_t address) const {
    return data_mem[address];
}

void Memory::write_indirect(uint8_t address, uint8_t value) {
    data_mem[address] = value;
}

uint8_t Memory::read_xdata(uint16_t address) const {
    return xdata_mem[address];
}

void Memory::write_xdata(uint16_t address, uint8_t value) {
    xdata_mem[address] = value;
}

bool Memory::resolve_bit_address(uint8_t bit_address, uint8_t& target_byte_addr, uint8_t& bit_idx, bool& is_sfr) const {
    if (bit_address < 0x80) {
        // RAM bit address space: 0x00 - 0x7F corresponds to RAM bytes 0x20 - 0x2F
        is_sfr = false;
        target_byte_addr = 0x20 + (bit_address >> 3);
        bit_idx = bit_address & 0x07;
        return true;
    } else {
        // SFR bit address space: 0x80 - 0xFF
        is_sfr = true;
        target_byte_addr = bit_address & 0xF8;
        bit_idx = bit_address & 0x07;
        return true;
    }
}

bool Memory::read_bit(uint8_t bit_address) const {
    uint8_t byte_addr = 0;
    uint8_t bit_idx = 0;
    bool is_sfr = false;
    resolve_bit_address(bit_address, byte_addr, bit_idx, is_sfr);

    uint8_t byte_val = is_sfr ? read_direct(byte_addr) : read_indirect(byte_addr);
    return (byte_val & (1 << bit_idx)) != 0;
}

void Memory::write_bit(uint8_t bit_address, bool value) {
    uint8_t byte_addr = 0;
    uint8_t bit_idx = 0;
    bool is_sfr = false;
    resolve_bit_address(bit_address, byte_addr, bit_idx, is_sfr);

    uint8_t byte_val = is_sfr ? read_direct(byte_addr) : read_indirect(byte_addr);
    if (value) {
        byte_val |= (1 << bit_idx);
    } else {
        byte_val &= ~(1 << bit_idx);
    }

    if (is_sfr) {
        write_direct(byte_addr, byte_val);
    } else {
        write_indirect(byte_addr, byte_val);
    }
}

void Memory::set_bit(uint8_t bit_address) {
    write_bit(bit_address, true);
}

void Memory::clear_bit(uint8_t bit_address) {
    write_bit(bit_address, false);
}

void Memory::complement_bit(uint8_t bit_address) {
    write_bit(bit_address, !read_bit(bit_address));
}

bool Memory::load_hex(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open HEX file: " << filepath << std::endl;
        return false;
    }

    std::string line;
    uint32_t base_ext_addr = 0;
    size_t line_num = 0;

    while (std::getline(file, line)) {
        line_num++;
        // Remove trailing CR/LF or whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        if (line[0] != ':') {
            std::cerr << "Warning: HEX parse skipped invalid line " << line_num << std::endl;
            continue;
        }

        if (line.length() < 11) {
            std::cerr << "Error: HEX line " << line_num << " is too short." << std::endl;
            return false;
        }

        auto hex2byte = [](const std::string& str, size_t pos) -> uint8_t {
            return static_cast<uint8_t>(std::stoul(str.substr(pos, 2), nullptr, 16));
        };

        try {
            uint8_t byte_count = hex2byte(line, 1);
            uint16_t record_address = (hex2byte(line, 3) << 8) | hex2byte(line, 5);
            uint8_t record_type = hex2byte(line, 7);

            uint8_t checksum = byte_count + (record_address >> 8) + (record_address & 0xFF) + record_type;

            std::vector<uint8_t> data;
            data.reserve(byte_count);
            for (size_t i = 0; i < byte_count; ++i) {
                uint8_t b = hex2byte(line, 9 + i * 2);
                data.push_back(b);
                checksum += b;
            }

            uint8_t file_checksum = hex2byte(line, 9 + byte_count * 2);
            checksum += file_checksum;
            if (checksum != 0) {
                std::cerr << "Error: Checksum mismatch on line " << line_num << std::endl;
                return false;
            }

            if (record_type == 0x00) { // Data record
                for (size_t i = 0; i < byte_count; ++i) {
                    uint32_t final_addr = base_ext_addr + record_address + i;
                    if (final_addr < code_mem.size()) {
                        code_mem[final_addr] = data[i];
                    }
                }
            } else if (record_type == 0x01) { // End Of File record
                break;
            } else if (record_type == 0x02) { // Extended Segment Address
                if (byte_count == 2) {
                    base_ext_addr = ((data[0] << 8) | data[1]) << 4;
                }
            } else if (record_type == 0x04) { // Extended Linear Address
                if (byte_count == 2) {
                    base_ext_addr = ((data[0] << 8) | data[1]) << 16;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: Failed to parse HEX line " << line_num << ": " << e.what() << std::endl;
            return false;
        }
    }

    return true;
}

bool Memory::load_bin(const std::string& filepath, uint16_t base_address) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open binary file: " << filepath << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (base_address + file_size > code_mem.size()) {
        std::cerr << "Error: Binary file size exceeds available CODE memory from base address." << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(&code_mem[base_address]), file_size);
    return true;
}

void Memory::dump() const {
    dump_internal_ram();
    dump_sfr();
}

void Memory::dump_internal_ram() const {
    std::cout << "\n====================== INTERNAL RAM DUMP (128 Bytes: 0x00-0x7F) ======================" << std::endl;
    std::cout << "      ";
    for (int i = 0; i < 16; ++i) {
        std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << i << " ";
    }
    std::cout << "\n-----------------------------------------------------------------------------";

    for (int address = 0; address < 128; ++address) {
        if (address % 16 == 0) {
            std::cout << "\n0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << address << " | ";
        }
        std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(data_mem[address]) << " ";
        if (address == 0x1F) std::cout << " <-- Register Banks R0-R7 (0x00-0x1F)";
        if (address == 0x2F) std::cout << " <-- Bit-addressable RAM (0x20-0x2F)";
    }
    std::cout << "\n-----------------------------------------------------------------------------\n";
    std::cout << std::nouppercase << std::dec;
}

void Memory::dump_sfr() const {
    std::cout << "\n====================== SFR REGISTERS (Direct 0x80-0xFF) ======================" << std::endl;
    std::cout << "  SP:   0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(read_direct(sfr::SP))
              << " | DPTR: 0x" << std::setw(2) << static_cast<int>(read_direct(sfr::DPH))
              << std::setw(2) << static_cast<int>(read_direct(sfr::DPL))
              << " | ACC:  0x" << std::setw(2) << static_cast<int>(read_direct(sfr::ACC))
              << " | B:    0x" << std::setw(2) << static_cast<int>(read_direct(sfr::B)) << "\n"
              << "  PSW:  0x" << std::setw(2) << static_cast<int>(read_direct(sfr::PSW))
              << " | P0:   0x" << std::setw(2) << static_cast<int>(read_direct(sfr::P0))
              << " | P1:   0x" << std::setw(2) << static_cast<int>(read_direct(sfr::P1))
              << " | P2:   0x" << std::setw(2) << static_cast<int>(read_direct(sfr::P2))
              << " | P3:   0x" << std::setw(2) << static_cast<int>(read_direct(sfr::P3)) << "\n"
              << "  IE:   0x" << std::setw(2) << static_cast<int>(read_direct(sfr::IE))
              << " | IP:   0x" << std::setw(2) << static_cast<int>(read_direct(sfr::IP))
              << " | TCON: 0x" << std::setw(2) << static_cast<int>(read_direct(sfr::TCON))
              << " | TMOD: 0x" << std::setw(2) << static_cast<int>(read_direct(sfr::TMOD)) << "\n";
    std::cout << "==============================================================================\n";
    std::cout << std::nouppercase << std::dec;
}

void Memory::dump_xdata(uint16_t start, uint16_t length) const {
    std::cout << "\n====================== EXTERNAL RAM DUMP (XDATA) ======================" << std::endl;
    for (uint32_t i = 0; i < length && (start + i) < xdata_mem.size(); ++i) {
        uint16_t addr = start + i;
        if (i % 16 == 0) {
            std::cout << "\n0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << addr << " | ";
        }
        std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(xdata_mem[addr]) << " ";
    }
    std::cout << "\n========================================================================\n";
    std::cout << std::nouppercase << std::dec;
}

void Memory::dump_code(uint16_t start, uint16_t length) const {
    std::cout << "\n====================== PROGRAM MEMORY DUMP (CODE) ======================" << std::endl;
    for (uint32_t i = 0; i < length && (start + i) < code_mem.size(); ++i) {
        uint16_t addr = start + i;
        if (i % 16 == 0) {
            std::cout << "\n0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << addr << " | ";
        }
        std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(code_mem[addr]) << " ";
    }
    std::cout << "\n========================================================================\n";
    std::cout << std::nouppercase << std::dec;
}
