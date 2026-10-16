#include "disassembler.hpp"
#include "memory.hpp"
#include "sfr.hpp"
#include <sstream>
#include <iomanip>

static std::string hex8(uint8_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(val);
    return ss.str();
}

static std::string hex16(uint16_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << static_cast<int>(val);
    return ss.str();
}

std::string Disassembler::format_direct_name(uint8_t direct) {
    switch (direct) {
        case sfr::P0:   return "P0";
        case sfr::SP:   return "SP";
        case sfr::DPL:  return "DPL";
        case sfr::DPH:  return "DPH";
        case sfr::PCON: return "PCON";
        case sfr::TCON: return "TCON";
        case sfr::TMOD: return "TMOD";
        case sfr::TL0:  return "TL0";
        case sfr::TL1:  return "TL1";
        case sfr::TH0:  return "TH0";
        case sfr::TH1:  return "TH1";
        case sfr::P1:   return "P1";
        case sfr::SCON: return "SCON";
        case sfr::SBUF: return "SBUF";
        case sfr::P2:   return "P2";
        case sfr::IE:   return "IE";
        case sfr::P3:   return "P3";
        case sfr::IP:   return "IP";
        case sfr::PSW:  return "PSW";
        case sfr::ACC:  return "ACC";
        case sfr::B:    return "B";
        default:        return hex8(direct);
    }
}

std::string Disassembler::format_bit_name(uint8_t bit_addr) {
    if (bit_addr >= 0x80) {
        uint8_t sfr_base = bit_addr & 0xF8;
        uint8_t bit_idx = bit_addr & 0x07;
        std::string base_name = format_direct_name(sfr_base);
        return base_name + "." + std::to_string(bit_idx);
    } else {
        uint8_t ram_byte = 0x20 + (bit_addr >> 3);
        uint8_t bit_idx = bit_addr & 0x07;
        return hex8(ram_byte) + "." + std::to_string(bit_idx);
    }
}

DisassemblyResult Disassembler::disassemble(const Memory& mem, uint16_t address) {
    DisassemblyResult res;
    res.address = address;
    uint8_t op = mem.read_code(address);
    res.opcode = op;

    uint8_t b1 = mem.read_code(address + 1);
    uint8_t b2 = mem.read_code(address + 2);

    auto target_rel = [&](int8_t offset, uint8_t inst_len) -> std::string {
        uint16_t tgt = address + inst_len + offset;
        return hex16(tgt);
    };

    // 1. ACALL addr11: [a10 a9 a8 1 0 0 0 1] -> 0x11, 31, 51, 71, 91, B1, D1, F1
    if ((op & 0x1F) == 0x11) {
        res.mnemonic = "ACALL";
        uint16_t next_pc = address + 2;
        uint16_t target = (next_pc & 0xF800) | ((op & 0xE0) << 3) | b1;
        res.operands = hex16(target);
        res.length = 2;
        res.cycles = 2;
    }
    // 2. AJMP addr11: [a10 a9 a8 0 0 0 0 1] -> 0x01, 21, 41, 61, 81, A1, C1, E1
    else if ((op & 0x1F) == 0x01) {
        res.mnemonic = "AJMP";
        uint16_t next_pc = address + 2;
        uint16_t target = (next_pc & 0xF800) | ((op & 0xE0) << 3) | b1;
        res.operands = hex16(target);
        res.length = 2;
        res.cycles = 2;
    }
    // 3. Rn registers: opcodes ending in 0x08 - 0x0F
    else if ((op & 0x08) && ((op & 0x07) <= 7) && ((op & 0xF0) != 0x00 || op >= 0x08)) {
        uint8_t reg = op & 0x07;
        std::string rn = "R" + std::to_string(reg);

        switch (op & 0xF8) {
            case 0x08: res.mnemonic = "INC";  res.operands = rn; res.length = 1; res.cycles = 1; break;
            case 0x18: res.mnemonic = "DEC";  res.operands = rn; res.length = 1; res.cycles = 1; break;
            case 0x28: res.mnemonic = "ADD";  res.operands = "A, " + rn; res.length = 1; res.cycles = 1; break;
            case 0x38: res.mnemonic = "ADDC"; res.operands = "A, " + rn; res.length = 1; res.cycles = 1; break;
            case 0x48: res.mnemonic = "ORL";  res.operands = "A, " + rn; res.length = 1; res.cycles = 1; break;
            case 0x58: res.mnemonic = "ANL";  res.operands = "A, " + rn; res.length = 1; res.cycles = 1; break;
            case 0x68: res.mnemonic = "XRL";  res.operands = "A, " + rn; res.length = 1; res.cycles = 1; break;
            case 0x78: res.mnemonic = "MOV";  res.operands = rn + ", #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x88: res.mnemonic = "MOV";  res.operands = format_direct_name(b1) + ", " + rn; res.length = 2; res.cycles = 2; break;
            case 0x98: res.mnemonic = "SUBB"; res.operands = "A, " + rn; res.length = 1; res.cycles = 1; break;
            case 0xA8: res.mnemonic = "MOV";  res.operands = rn + ", " + format_direct_name(b1); res.length = 2; res.cycles = 2; break;
            case 0xB8: res.mnemonic = "CJNE"; res.operands = rn + ", #" + hex8(b1) + ", " + target_rel(static_cast<int8_t>(b2), 3); res.length = 3; res.cycles = 2; break;
            case 0xC8: res.mnemonic = "XCH";  res.operands = "A, " + rn; res.length = 1; res.cycles = 1; break;
            case 0xD8: res.mnemonic = "DJNZ"; res.operands = rn + ", " + target_rel(static_cast<int8_t>(b1), 2); res.length = 2; res.cycles = 2; break;
            case 0xE8: res.mnemonic = "MOV";  res.operands = "A, " + rn; res.length = 1; res.cycles = 1; break;
            case 0xF8: res.mnemonic = "MOV";  res.operands = rn + ", A"; res.length = 1; res.cycles = 1; break;
        }
    }
    // 4. @Ri registers: opcodes ending in 6 or 7
    else if ((op & 0x06) == 0x06 && ((op & 0x01) == 0 || (op & 0x01) == 1)) {
        uint8_t ri_num = op & 0x01;
        std::string ri = "@R" + std::to_string(ri_num);

        switch (op & 0xFE) {
            case 0x06: res.mnemonic = "INC";  res.operands = ri; res.length = 1; res.cycles = 1; break;
            case 0x16: res.mnemonic = "DEC";  res.operands = ri; res.length = 1; res.cycles = 1; break;
            case 0x26: res.mnemonic = "ADD";  res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0x36: res.mnemonic = "ADDC"; res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0x46: res.mnemonic = "ORL";  res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0x56: res.mnemonic = "ANL";  res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0x66: res.mnemonic = "XRL";  res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0x76: res.mnemonic = "MOV";  res.operands = ri + ", #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x86: res.mnemonic = "MOV";  res.operands = format_direct_name(b1) + ", " + ri; res.length = 2; res.cycles = 2; break;
            case 0x96: res.mnemonic = "SUBB"; res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0xA6: res.mnemonic = "MOV";  res.operands = ri + ", " + format_direct_name(b1); res.length = 2; res.cycles = 2; break;
            case 0xB6: res.mnemonic = "CJNE"; res.operands = ri + ", #" + hex8(b1) + ", " + target_rel(static_cast<int8_t>(b2), 3); res.length = 3; res.cycles = 2; break;
            case 0xC6: res.mnemonic = "XCH";  res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0xD6: res.mnemonic = "XCHD"; res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0xE6: res.mnemonic = "MOV";  res.operands = "A, " + ri; res.length = 1; res.cycles = 1; break;
            case 0xF6: res.mnemonic = "MOV";  res.operands = ri + ", A"; res.length = 1; res.cycles = 1; break;
        }
    }
    // 5. Remaining discrete opcodes
    else {
        switch (op) {
            case 0x00: res.mnemonic = "NOP"; res.operands = ""; res.length = 1; res.cycles = 1; break;
            case 0x02: res.mnemonic = "LJMP"; res.operands = hex16((b1 << 8) | b2); res.length = 3; res.cycles = 2; break;
            case 0x03: res.mnemonic = "RR"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0x04: res.mnemonic = "INC"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0x05: res.mnemonic = "INC"; res.operands = format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0x10: res.mnemonic = "JBC"; res.operands = format_bit_name(b1) + ", " + target_rel(static_cast<int8_t>(b2), 3); res.length = 3; res.cycles = 2; break;
            case 0x12: res.mnemonic = "LCALL"; res.operands = hex16((b1 << 8) | b2); res.length = 3; res.cycles = 2; break;
            case 0x13: res.mnemonic = "RRC"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0x14: res.mnemonic = "DEC"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0x15: res.mnemonic = "DEC"; res.operands = format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0x20: res.mnemonic = "JB"; res.operands = format_bit_name(b1) + ", " + target_rel(static_cast<int8_t>(b2), 3); res.length = 3; res.cycles = 2; break;
            case 0x22: res.mnemonic = "RET"; res.operands = ""; res.length = 1; res.cycles = 2; break;
            case 0x23: res.mnemonic = "RL"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0x24: res.mnemonic = "ADD"; res.operands = "A, #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x25: res.mnemonic = "ADD"; res.operands = "A, " + format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0x30: res.mnemonic = "JNB"; res.operands = format_bit_name(b1) + ", " + target_rel(static_cast<int8_t>(b2), 3); res.length = 3; res.cycles = 2; break;
            case 0x32: res.mnemonic = "RETI"; res.operands = ""; res.length = 1; res.cycles = 2; break;
            case 0x33: res.mnemonic = "RLC"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0x34: res.mnemonic = "ADDC"; res.operands = "A, #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x35: res.mnemonic = "ADDC"; res.operands = "A, " + format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0x40: res.mnemonic = "JC"; res.operands = target_rel(static_cast<int8_t>(b1), 2); res.length = 2; res.cycles = 2; break;
            case 0x42: res.mnemonic = "ORL"; res.operands = format_direct_name(b1) + ", A"; res.length = 2; res.cycles = 1; break;
            case 0x43: res.mnemonic = "ORL"; res.operands = format_direct_name(b1) + ", #" + hex8(b2); res.length = 3; res.cycles = 2; break;
            case 0x44: res.mnemonic = "ORL"; res.operands = "A, #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x45: res.mnemonic = "ORL"; res.operands = "A, " + format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0x50: res.mnemonic = "JNC"; res.operands = target_rel(static_cast<int8_t>(b1), 2); res.length = 2; res.cycles = 2; break;
            case 0x52: res.mnemonic = "ANL"; res.operands = format_direct_name(b1) + ", A"; res.length = 2; res.cycles = 1; break;
            case 0x53: res.mnemonic = "ANL"; res.operands = format_direct_name(b1) + ", #" + hex8(b2); res.length = 3; res.cycles = 2; break;
            case 0x54: res.mnemonic = "ANL"; res.operands = "A, #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x55: res.mnemonic = "ANL"; res.operands = "A, " + format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0x60: res.mnemonic = "JZ"; res.operands = target_rel(static_cast<int8_t>(b1), 2); res.length = 2; res.cycles = 2; break;
            case 0x62: res.mnemonic = "XRL"; res.operands = format_direct_name(b1) + ", A"; res.length = 2; res.cycles = 1; break;
            case 0x63: res.mnemonic = "XRL"; res.operands = format_direct_name(b1) + ", #" + hex8(b2); res.length = 3; res.cycles = 2; break;
            case 0x64: res.mnemonic = "XRL"; res.operands = "A, #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x65: res.mnemonic = "XRL"; res.operands = "A, " + format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0x70: res.mnemonic = "JNZ"; res.operands = target_rel(static_cast<int8_t>(b1), 2); res.length = 2; res.cycles = 2; break;
            case 0x72: res.mnemonic = "ORL"; res.operands = "C, " + format_bit_name(b1); res.length = 2; res.cycles = 2; break;
            case 0x73: res.mnemonic = "JMP"; res.operands = "@A+DPTR"; res.length = 1; res.cycles = 2; break;
            case 0x74: res.mnemonic = "MOV"; res.operands = "A, #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x75: res.mnemonic = "MOV"; res.operands = format_direct_name(b1) + ", #" + hex8(b2); res.length = 3; res.cycles = 2; break;

            case 0x80: res.mnemonic = "SJMP"; res.operands = target_rel(static_cast<int8_t>(b1), 2); res.length = 2; res.cycles = 2; break;
            case 0x82: res.mnemonic = "ANL"; res.operands = "C, " + format_bit_name(b1); res.length = 2; res.cycles = 2; break;
            case 0x83: res.mnemonic = "MOVC"; res.operands = "A, @A+PC"; res.length = 1; res.cycles = 2; break;
            case 0x84: res.mnemonic = "DIV"; res.operands = "AB"; res.length = 1; res.cycles = 4; break;
            case 0x85: res.mnemonic = "MOV"; res.operands = format_direct_name(b2) + ", " + format_direct_name(b1); res.length = 3; res.cycles = 2; break;

            case 0x90: res.mnemonic = "MOV"; res.operands = "DPTR, #" + hex16((b1 << 8) | b2); res.length = 3; res.cycles = 2; break;
            case 0x92: res.mnemonic = "MOV"; res.operands = format_bit_name(b1) + ", C"; res.length = 2; res.cycles = 2; break;
            case 0x93: res.mnemonic = "MOVC"; res.operands = "A, @A+DPTR"; res.length = 1; res.cycles = 2; break;
            case 0x94: res.mnemonic = "SUBB"; res.operands = "A, #" + hex8(b1); res.length = 2; res.cycles = 1; break;
            case 0x95: res.mnemonic = "SUBB"; res.operands = "A, " + format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0xA0: res.mnemonic = "ORL"; res.operands = "C, /" + format_bit_name(b1); res.length = 2; res.cycles = 2; break;
            case 0xA2: res.mnemonic = "MOV"; res.operands = "C, " + format_bit_name(b1); res.length = 2; res.cycles = 1; break;
            case 0xA3: res.mnemonic = "INC"; res.operands = "DPTR"; res.length = 1; res.cycles = 2; break;
            case 0xA4: res.mnemonic = "MUL"; res.operands = "AB"; res.length = 1; res.cycles = 4; break;
            case 0xA5: res.mnemonic = "DB"; res.operands = "0xA5 (Undefined)"; res.length = 1; res.cycles = 1; break;

            case 0xB0: res.mnemonic = "ANL"; res.operands = "C, /" + format_bit_name(b1); res.length = 2; res.cycles = 2; break;
            case 0xB2: res.mnemonic = "CPL"; res.operands = format_bit_name(b1); res.length = 2; res.cycles = 1; break;
            case 0xB3: res.mnemonic = "CPL"; res.operands = "C"; res.length = 1; res.cycles = 1; break;
            case 0xB4: res.mnemonic = "CJNE"; res.operands = "A, #" + hex8(b1) + ", " + target_rel(static_cast<int8_t>(b2), 3); res.length = 3; res.cycles = 2; break;
            case 0xB5: res.mnemonic = "CJNE"; res.operands = "A, " + format_direct_name(b1) + ", " + target_rel(static_cast<int8_t>(b2), 3); res.length = 3; res.cycles = 2; break;

            case 0xC0: res.mnemonic = "PUSH"; res.operands = format_direct_name(b1); res.length = 2; res.cycles = 2; break;
            case 0xC2: res.mnemonic = "CLR"; res.operands = format_bit_name(b1); res.length = 2; res.cycles = 1; break;
            case 0xC3: res.mnemonic = "CLR"; res.operands = "C"; res.length = 1; res.cycles = 1; break;
            case 0xC4: res.mnemonic = "SWAP"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0xC5: res.mnemonic = "XCH"; res.operands = "A, " + format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0xD0: res.mnemonic = "POP"; res.operands = format_direct_name(b1); res.length = 2; res.cycles = 2; break;
            case 0xD2: res.mnemonic = "SETB"; res.operands = format_bit_name(b1); res.length = 2; res.cycles = 1; break;
            case 0xD3: res.mnemonic = "SETB"; res.operands = "C"; res.length = 1; res.cycles = 1; break;
            case 0xD4: res.mnemonic = "DA"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0xD5: res.mnemonic = "DJNZ"; res.operands = format_direct_name(b1) + ", " + target_rel(static_cast<int8_t>(b2), 3); res.length = 3; res.cycles = 2; break;

            case 0xE0: res.mnemonic = "MOVX"; res.operands = "A, @DPTR"; res.length = 1; res.cycles = 2; break;
            case 0xE2: res.mnemonic = "MOVX"; res.operands = "A, @R0"; res.length = 1; res.cycles = 2; break;
            case 0xE3: res.mnemonic = "MOVX"; res.operands = "A, @R1"; res.length = 1; res.cycles = 2; break;
            case 0xE4: res.mnemonic = "CLR"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0xE5: res.mnemonic = "MOV"; res.operands = "A, " + format_direct_name(b1); res.length = 2; res.cycles = 1; break;

            case 0xF0: res.mnemonic = "MOVX"; res.operands = "@DPTR, A"; res.length = 1; res.cycles = 2; break;
            case 0xF2: res.mnemonic = "MOVX"; res.operands = "@R0, A"; res.length = 1; res.cycles = 2; break;
            case 0xF3: res.mnemonic = "MOVX"; res.operands = "@R1, A"; res.length = 1; res.cycles = 2; break;
            case 0xF4: res.mnemonic = "CPL"; res.operands = "A"; res.length = 1; res.cycles = 1; break;
            case 0xF5: res.mnemonic = "MOV"; res.operands = format_direct_name(b1) + ", A"; res.length = 2; res.cycles = 1; break;

            default:
                res.mnemonic = "???";
                res.operands = hex8(op);
                res.length = 1;
                res.cycles = 1;
                break;
        }
    }

    // Build bytes_hex string
    std::ostringstream bytes_ss;
    for (uint8_t i = 0; i < res.length; ++i) {
        uint8_t b = mem.read_code(address + i);
        bytes_ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
    res.bytes_hex = bytes_ss.str();

    // Format complete display text
    std::ostringstream text_ss;
    text_ss << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << address << ":  "
            << std::setfill(' ') << std::left << std::setw(10) << res.bytes_hex
            << std::setw(6) << res.mnemonic << " " << res.operands;
    res.text = text_ss.str();

    return res;
}
