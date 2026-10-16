#include "cpu.hpp"
#include "sfr.hpp"
#include "disassembler.hpp"
#include <iostream>
#include <iomanip>

CPU::CPU(Memory& mem) : memory(mem), pc(0x0000), cycles(0), halted(false) {
    reset();
}

void CPU::reset() {
    pc = 0x0000;
    cycles = 0;
    halted = false;
    memory.reset();
}

uint8_t CPU::get_acc() const { return memory.read_direct(sfr::ACC); }
void CPU::set_acc(uint8_t val) { memory.write_direct(sfr::ACC, val); }

uint8_t CPU::get_b() const { return memory.read_direct(sfr::B); }
void CPU::set_b(uint8_t val) { memory.write_direct(sfr::B, val); }

uint8_t CPU::get_sp() const { return memory.read_direct(sfr::SP); }
void CPU::set_sp(uint8_t val) { memory.write_direct(sfr::SP, val); }

uint8_t CPU::get_psw() const { return memory.read_direct(sfr::PSW); }
void CPU::set_psw(uint8_t val) { memory.write_direct(sfr::PSW, val); }

uint16_t CPU::get_dptr() const {
    return (memory.read_direct(sfr::DPH) << 8) | memory.read_direct(sfr::DPL);
}

void CPU::set_dptr(uint16_t val) {
    memory.write_direct(sfr::DPH, (val >> 8) & 0xFF);
    memory.write_direct(sfr::DPL, val & 0xFF);
}

bool CPU::get_cy() const { return (get_psw() & sfr::PSW_CY) != 0; }
void CPU::set_cy(bool v) {
    uint8_t psw = get_psw();
    set_psw(v ? (psw | sfr::PSW_CY) : (psw & ~sfr::PSW_CY));
}

bool CPU::get_ac() const { return (get_psw() & sfr::PSW_AC) != 0; }
void CPU::set_ac(bool v) {
    uint8_t psw = get_psw();
    set_psw(v ? (psw | sfr::PSW_AC) : (psw & ~sfr::PSW_AC));
}

bool CPU::get_ov() const { return (get_psw() & sfr::PSW_OV) != 0; }
void CPU::set_ov(bool v) {
    uint8_t psw = get_psw();
    set_psw(v ? (psw | sfr::PSW_OV) : (psw & ~sfr::PSW_OV));
}

bool CPU::get_p() const { return (get_psw() & sfr::PSW_P) != 0; }

uint8_t CPU::get_active_bank() const {
    return (get_psw() >> 3) & 0x03;
}

uint8_t CPU::get_register_address(uint8_t reg_num) const {
    return (get_active_bank() * 8) + (reg_num & 0x07);
}

uint8_t CPU::get_rn(uint8_t reg_num) const {
    return memory.read_direct(get_register_address(reg_num));
}

void CPU::set_rn(uint8_t reg_num, uint8_t val) {
    memory.write_direct(get_register_address(reg_num), val);
}

uint8_t CPU::get_ri(uint8_t ri_num) const {
    uint8_t indirect_addr = get_rn(ri_num & 0x01);
    return memory.read_indirect(indirect_addr);
}

void CPU::set_ri(uint8_t ri_num, uint8_t val) {
    uint8_t indirect_addr = get_rn(ri_num & 0x01);
    memory.write_indirect(indirect_addr, val);
}

void CPU::push(uint8_t val) {
    uint8_t sp = (get_sp() + 1) & 0xFF;
    set_sp(sp);
    memory.write_indirect(sp, val);
}

uint8_t CPU::pop() {
    uint8_t sp = get_sp();
    uint8_t val = memory.read_indirect(sp);
    set_sp((sp - 1) & 0xFF);
    return val;
}

uint8_t CPU::alu_add(uint8_t a, uint8_t b, bool carry_in) {
    uint8_t cin = carry_in ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(a) + b + cin;
    uint8_t r8 = static_cast<uint8_t>(result);

    set_cy(result > 0xFF);
    set_ac(((a & 0x0F) + (b & 0x0F) + cin) > 0x0F);
    set_ov(((~(a ^ b)) & (a ^ r8) & 0x80) != 0);

    return r8;
}

uint8_t CPU::alu_subb(uint8_t a, uint8_t b) {
    uint8_t cin = get_cy() ? 1 : 0;
    int16_t result = static_cast<int16_t>(a) - b - cin;
    uint8_t r8 = static_cast<uint8_t>(result);

    set_cy(result < 0);
    set_ac(((a & 0x0F) - (b & 0x0F) - cin) < 0);
    set_ov(((a ^ b) & (a ^ r8) & 0x80) != 0);

    return r8;
}

uint8_t CPU::step() {
    if (halted) return 0;

    uint8_t opcode = memory.read_code(pc++);

    // 1. ACALL addr11: [a10 a9 a8 1 0 0 0 1] -> 0x11, 31, 51, 71, 91, B1, D1, F1
    if ((opcode & 0x1F) == 0x11) {
        uint8_t b1 = memory.read_code(pc++);
        push(pc & 0xFF);
        push((pc >> 8) & 0xFF);
        pc = (pc & 0xF800) | ((opcode & 0xE0) << 3) | b1;
        cycles += 2;
        return 2;
    }

    // 2. AJMP addr11: [a10 a9 a8 0 0 0 0 1] -> 0x01, 21, 41, 61, 81, A1, C1, E1
    if ((opcode & 0x1F) == 0x01) {
        uint8_t b1 = memory.read_code(pc++);
        pc = (pc & 0xF800) | ((opcode & 0xE0) << 3) | b1;
        cycles += 2;
        return 2;
    }

    // 3. Register Rn operations: Opcodes ending in 0x08 - 0x0F
    if ((opcode & 0x08) && ((opcode & 0x07) <= 7)) {
        uint8_t reg = opcode & 0x07;
        switch (opcode & 0xF8) {
            case 0x08: // INC Rn
                set_rn(reg, get_rn(reg) + 1);
                cycles += 1; return 1;
            case 0x18: // DEC Rn
                set_rn(reg, get_rn(reg) - 1);
                cycles += 1; return 1;
            case 0x28: // ADD A, Rn
                set_acc(alu_add(get_acc(), get_rn(reg), false));
                cycles += 1; return 1;
            case 0x38: // ADDC A, Rn
                set_acc(alu_add(get_acc(), get_rn(reg), get_cy()));
                cycles += 1; return 1;
            case 0x48: // ORL A, Rn
                set_acc(get_acc() | get_rn(reg));
                cycles += 1; return 1;
            case 0x58: // ANL A, Rn
                set_acc(get_acc() & get_rn(reg));
                cycles += 1; return 1;
            case 0x68: // XRL A, Rn
                set_acc(get_acc() ^ get_rn(reg));
                cycles += 1; return 1;
            case 0x78: { // MOV Rn, #imm
                uint8_t imm = memory.read_code(pc++);
                set_rn(reg, imm);
                cycles += 1; return 1;
            }
            case 0x88: { // MOV direct, Rn
                uint8_t dir = memory.read_code(pc++);
                memory.write_direct(dir, get_rn(reg));
                cycles += 2; return 2;
            }
            case 0x98: // SUBB A, Rn
                set_acc(alu_subb(get_acc(), get_rn(reg)));
                cycles += 1; return 1;
            case 0xA8: { // MOV Rn, direct
                uint8_t dir = memory.read_code(pc++);
                set_rn(reg, memory.read_direct(dir));
                cycles += 2; return 2;
            }
            case 0xB8: { // CJNE Rn, #imm, rel
                uint8_t imm = memory.read_code(pc++);
                int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
                uint8_t val = get_rn(reg);
                set_cy(val < imm);
                if (val != imm) pc += rel;
                cycles += 2; return 2;
            }
            case 0xC8: { // XCH A, Rn
                uint8_t a = get_acc();
                set_acc(get_rn(reg));
                set_rn(reg, a);
                cycles += 1; return 1;
            }
            case 0xD8: { // DJNZ Rn, rel
                int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
                uint8_t val = get_rn(reg) - 1;
                set_rn(reg, val);
                if (val != 0) pc += rel;
                cycles += 2; return 2;
            }
            case 0xE8: // MOV A, Rn
                set_acc(get_rn(reg));
                cycles += 1; return 1;
            case 0xF8: // MOV Rn, A
                set_rn(reg, get_acc());
                cycles += 1; return 1;
        }
    }

    // 4. Indirect @Ri operations: Opcodes ending in 6 or 7
    if ((opcode & 0x06) == 0x06 && ((opcode & 0x01) <= 1)) {
        uint8_t ri = opcode & 0x01;
        switch (opcode & 0xFE) {
            case 0x06: // INC @Ri
                set_ri(ri, get_ri(ri) + 1);
                cycles += 1; return 1;
            case 0x16: // DEC @Ri
                set_ri(ri, get_ri(ri) - 1);
                cycles += 1; return 1;
            case 0x26: // ADD A, @Ri
                set_acc(alu_add(get_acc(), get_ri(ri), false));
                cycles += 1; return 1;
            case 0x36: // ADDC A, @Ri
                set_acc(alu_add(get_acc(), get_ri(ri), get_cy()));
                cycles += 1; return 1;
            case 0x46: // ORL A, @Ri
                set_acc(get_acc() | get_ri(ri));
                cycles += 1; return 1;
            case 0x56: // ANL A, @Ri
                set_acc(get_acc() & get_ri(ri));
                cycles += 1; return 1;
            case 0x66: // XRL A, @Ri
                set_acc(get_acc() ^ get_ri(ri));
                cycles += 1; return 1;
            case 0x76: { // MOV @Ri, #imm
                uint8_t imm = memory.read_code(pc++);
                set_ri(ri, imm);
                cycles += 1; return 1;
            }
            case 0x86: { // MOV direct, @Ri
                uint8_t dir = memory.read_code(pc++);
                memory.write_direct(dir, get_ri(ri));
                cycles += 2; return 2;
            }
            case 0x96: // SUBB A, @Ri
                set_acc(alu_subb(get_acc(), get_ri(ri)));
                cycles += 1; return 1;
            case 0xA6: { // MOV @Ri, direct
                uint8_t dir = memory.read_code(pc++);
                set_ri(ri, memory.read_direct(dir));
                cycles += 2; return 2;
            }
            case 0xB6: { // CJNE @Ri, #imm, rel
                uint8_t imm = memory.read_code(pc++);
                int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
                uint8_t val = get_ri(ri);
                set_cy(val < imm);
                if (val != imm) pc += rel;
                cycles += 2; return 2;
            }
            case 0xC6: { // XCH A, @Ri
                uint8_t a = get_acc();
                set_acc(get_ri(ri));
                set_ri(ri, a);
                cycles += 1; return 1;
            }
            case 0xD6: { // XCHD A, @Ri
                uint8_t a = get_acc();
                uint8_t ri_val = get_ri(ri);
                set_acc((a & 0xF0) | (ri_val & 0x0F));
                set_ri(ri, (ri_val & 0xF0) | (a & 0x0F));
                cycles += 1; return 1;
            }
            case 0xE6: // MOV A, @Ri
                set_acc(get_ri(ri));
                cycles += 1; return 1;
            case 0xF6: // MOV @Ri, A
                set_ri(ri, get_acc());
                cycles += 1; return 1;
        }
    }

    // 5. Discrete Opcodes
    switch (opcode) {
        case 0x00: // NOP
            cycles += 1; return 1;

        case 0x02: { // LJMP addr16
            uint16_t addr = (memory.read_code(pc) << 8) | memory.read_code(pc + 1);
            pc = addr;
            cycles += 2; return 2;
        }

        case 0x03: { // RR A
            uint8_t a = get_acc();
            set_acc((a >> 1) | (a << 7));
            cycles += 1; return 1;
        }

        case 0x04: // INC A
            set_acc(get_acc() + 1);
            cycles += 1; return 1;

        case 0x05: { // INC direct
            uint8_t dir = memory.read_code(pc++);
            memory.write_direct(dir, memory.read_direct(dir) + 1);
            cycles += 1; return 1;
        }

        case 0x10: { // JBC bit, rel
            uint8_t bit_addr = memory.read_code(pc++);
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            if (memory.read_bit(bit_addr)) {
                memory.clear_bit(bit_addr);
                pc += rel;
            }
            cycles += 2; return 2;
        }

        case 0x12: { // LCALL addr16
            uint16_t addr = (memory.read_code(pc) << 8) | memory.read_code(pc + 1);
            pc += 2;
            push(pc & 0xFF);
            push((pc >> 8) & 0xFF);
            pc = addr;
            cycles += 2; return 2;
        }

        case 0x13: { // RRC A
            uint8_t a = get_acc();
            bool old_c = get_cy();
            set_cy((a & 0x01) != 0);
            set_acc((a >> 1) | (old_c ? 0x80 : 0));
            cycles += 1; return 1;
        }

        case 0x14: // DEC A
            set_acc(get_acc() - 1);
            cycles += 1; return 1;

        case 0x15: { // DEC direct
            uint8_t dir = memory.read_code(pc++);
            memory.write_direct(dir, memory.read_direct(dir) - 1);
            cycles += 1; return 1;
        }

        case 0x20: { // JB bit, rel
            uint8_t bit_addr = memory.read_code(pc++);
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            if (memory.read_bit(bit_addr)) pc += rel;
            cycles += 2; return 2;
        }

        case 0x22: { // RET
            uint8_t hi = pop();
            uint8_t lo = pop();
            pc = (hi << 8) | lo;
            cycles += 2; return 2;
        }

        case 0x23: { // RL A
            uint8_t a = get_acc();
            set_acc((a << 1) | (a >> 7));
            cycles += 1; return 1;
        }

        case 0x24: { // ADD A, #imm
            uint8_t imm = memory.read_code(pc++);
            set_acc(alu_add(get_acc(), imm, false));
            cycles += 1; return 1;
        }

        case 0x25: { // ADD A, direct
            uint8_t dir = memory.read_code(pc++);
            set_acc(alu_add(get_acc(), memory.read_direct(dir), false));
            cycles += 1; return 1;
        }

        case 0x30: { // JNB bit, rel
            uint8_t bit_addr = memory.read_code(pc++);
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            if (!memory.read_bit(bit_addr)) pc += rel;
            cycles += 2; return 2;
        }

        case 0x32: { // RETI
            uint8_t hi = pop();
            uint8_t lo = pop();
            pc = (hi << 8) | lo;
            cycles += 2; return 2;
        }

        case 0x33: { // RLC A
            uint8_t a = get_acc();
            bool old_c = get_cy();
            set_cy((a & 0x80) != 0);
            set_acc((a << 1) | (old_c ? 1 : 0));
            cycles += 1; return 1;
        }

        case 0x34: { // ADDC A, #imm
            uint8_t imm = memory.read_code(pc++);
            set_acc(alu_add(get_acc(), imm, get_cy()));
            cycles += 1; return 1;
        }

        case 0x35: { // ADDC A, direct
            uint8_t dir = memory.read_code(pc++);
            set_acc(alu_add(get_acc(), memory.read_direct(dir), get_cy()));
            cycles += 1; return 1;
        }

        case 0x40: { // JC rel
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            if (get_cy()) pc += rel;
            cycles += 2; return 2;
        }

        case 0x42: { // ORL direct, A
            uint8_t dir = memory.read_code(pc++);
            memory.write_direct(dir, memory.read_direct(dir) | get_acc());
            cycles += 1; return 1;
        }

        case 0x43: { // ORL direct, #imm
            uint8_t dir = memory.read_code(pc++);
            uint8_t imm = memory.read_code(pc++);
            memory.write_direct(dir, memory.read_direct(dir) | imm);
            cycles += 2; return 2;
        }

        case 0x44: { // ORL A, #imm
            uint8_t imm = memory.read_code(pc++);
            set_acc(get_acc() | imm);
            cycles += 1; return 1;
        }

        case 0x45: { // ORL A, direct
            uint8_t dir = memory.read_code(pc++);
            set_acc(get_acc() | memory.read_direct(dir));
            cycles += 1; return 1;
        }

        case 0x50: { // JNC rel
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            if (!get_cy()) pc += rel;
            cycles += 2; return 2;
        }

        case 0x52: { // ANL direct, A
            uint8_t dir = memory.read_code(pc++);
            memory.write_direct(dir, memory.read_direct(dir) & get_acc());
            cycles += 1; return 1;
        }

        case 0x53: { // ANL direct, #imm
            uint8_t dir = memory.read_code(pc++);
            uint8_t imm = memory.read_code(pc++);
            memory.write_direct(dir, memory.read_direct(dir) & imm);
            cycles += 2; return 2;
        }

        case 0x54: { // ANL A, #imm
            uint8_t imm = memory.read_code(pc++);
            set_acc(get_acc() & imm);
            cycles += 1; return 1;
        }

        case 0x55: { // ANL A, direct
            uint8_t dir = memory.read_code(pc++);
            set_acc(get_acc() & memory.read_direct(dir));
            cycles += 1; return 1;
        }

        case 0x60: { // JZ rel
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            if (get_acc() == 0) pc += rel;
            cycles += 2; return 2;
        }

        case 0x62: { // XRL direct, A
            uint8_t dir = memory.read_code(pc++);
            memory.write_direct(dir, memory.read_direct(dir) ^ get_acc());
            cycles += 1; return 1;
        }

        case 0x63: { // XRL direct, #imm
            uint8_t dir = memory.read_code(pc++);
            uint8_t imm = memory.read_code(pc++);
            memory.write_direct(dir, memory.read_direct(dir) ^ imm);
            cycles += 2; return 2;
        }

        case 0x64: { // XRL A, #imm
            uint8_t imm = memory.read_code(pc++);
            set_acc(get_acc() ^ imm);
            cycles += 1; return 1;
        }

        case 0x65: { // XRL A, direct
            uint8_t dir = memory.read_code(pc++);
            set_acc(get_acc() ^ memory.read_direct(dir));
            cycles += 1; return 1;
        }

        case 0x70: { // JNZ rel
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            if (get_acc() != 0) pc += rel;
            cycles += 2; return 2;
        }

        case 0x72: { // ORL C, bit
            uint8_t bit_addr = memory.read_code(pc++);
            set_cy(get_cy() || memory.read_bit(bit_addr));
            cycles += 2; return 2;
        }

        case 0x73: { // JMP @A+DPTR
            pc = (get_acc() + get_dptr()) & 0xFFFF;
            cycles += 2; return 2;
        }

        case 0x74: { // MOV A, #imm
            uint8_t imm = memory.read_code(pc++);
            set_acc(imm);
            cycles += 1; return 1;
        }

        case 0x75: { // MOV direct, #imm
            uint8_t dir = memory.read_code(pc++);
            uint8_t imm = memory.read_code(pc++);
            memory.write_direct(dir, imm);
            cycles += 2; return 2;
        }

        case 0x80: { // SJMP rel
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            pc += rel;
            cycles += 2; return 2;
        }

        case 0x82: { // ANL C, bit
            uint8_t bit_addr = memory.read_code(pc++);
            set_cy(get_cy() && memory.read_bit(bit_addr));
            cycles += 2; return 2;
        }

        case 0x83: { // MOVC A, @A+PC
            uint16_t addr = (pc + get_acc()) & 0xFFFF;
            set_acc(memory.read_code(addr));
            cycles += 2; return 2;
        }

        case 0x84: { // DIV AB
            uint8_t a = get_acc();
            uint8_t b = get_b();
            set_cy(false);
            if (b == 0) {
                set_ov(true);
            } else {
                set_ov(false);
                set_acc(a / b);
                set_b(a % b);
            }
            cycles += 4; return 4;
        }

        case 0x85: { // MOV direct, direct
            uint8_t src_dir = memory.read_code(pc++);
            uint8_t dst_dir = memory.read_code(pc++);
            memory.write_direct(dst_dir, memory.read_direct(src_dir));
            cycles += 2; return 2;
        }

        case 0x90: { // MOV DPTR, #imm16
            uint8_t hi = memory.read_code(pc++);
            uint8_t lo = memory.read_code(pc++);
            set_dptr((hi << 8) | lo);
            cycles += 2; return 2;
        }

        case 0x92: { // MOV bit, C
            uint8_t bit_addr = memory.read_code(pc++);
            memory.write_bit(bit_addr, get_cy());
            cycles += 2; return 2;
        }

        case 0x93: { // MOVC A, @A+DPTR
            uint16_t addr = (get_dptr() + get_acc()) & 0xFFFF;
            set_acc(memory.read_code(addr));
            cycles += 2; return 2;
        }

        case 0x94: { // SUBB A, #imm
            uint8_t imm = memory.read_code(pc++);
            set_acc(alu_subb(get_acc(), imm));
            cycles += 1; return 1;
        }

        case 0x95: { // SUBB A, direct
            uint8_t dir = memory.read_code(pc++);
            set_acc(alu_subb(get_acc(), memory.read_direct(dir)));
            cycles += 1; return 1;
        }

        case 0xA0: { // ORL C, /bit
            uint8_t bit_addr = memory.read_code(pc++);
            set_cy(get_cy() || !memory.read_bit(bit_addr));
            cycles += 2; return 2;
        }

        case 0xA2: { // MOV C, bit
            uint8_t bit_addr = memory.read_code(pc++);
            set_cy(memory.read_bit(bit_addr));
            cycles += 1; return 1;
        }

        case 0xA3: // INC DPTR
            set_dptr(get_dptr() + 1);
            cycles += 2; return 2;

        case 0xA4: { // MUL AB
            uint16_t prod = static_cast<uint16_t>(get_acc()) * static_cast<uint16_t>(get_b());
            set_acc(prod & 0xFF);
            set_b((prod >> 8) & 0xFF);
            set_cy(false);
            set_ov(prod > 0xFF);
            cycles += 4; return 4;
        }

        case 0xA5: // Reserved opcode
            cycles += 1; return 1;

        case 0xB0: { // ANL C, /bit
            uint8_t bit_addr = memory.read_code(pc++);
            set_cy(get_cy() && !memory.read_bit(bit_addr));
            cycles += 2; return 2;
        }

        case 0xB2: { // CPL bit
            uint8_t bit_addr = memory.read_code(pc++);
            memory.complement_bit(bit_addr);
            cycles += 1; return 1;
        }

        case 0xB3: // CPL C
            set_cy(!get_cy());
            cycles += 1; return 1;

        case 0xB4: { // CJNE A, #imm, rel
            uint8_t imm = memory.read_code(pc++);
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            uint8_t a = get_acc();
            set_cy(a < imm);
            if (a != imm) pc += rel;
            cycles += 2; return 2;
        }

        case 0xB5: { // CJNE A, direct, rel
            uint8_t dir = memory.read_code(pc++);
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            uint8_t a = get_acc();
            uint8_t val = memory.read_direct(dir);
            set_cy(a < val);
            if (a != val) pc += rel;
            cycles += 2; return 2;
        }

        case 0xC0: { // PUSH direct
            uint8_t dir = memory.read_code(pc++);
            push(memory.read_direct(dir));
            cycles += 2; return 2;
        }

        case 0xC2: { // CLR bit
            uint8_t bit_addr = memory.read_code(pc++);
            memory.clear_bit(bit_addr);
            cycles += 1; return 1;
        }

        case 0xC3: // CLR C
            set_cy(false);
            cycles += 1; return 1;

        case 0xC4: { // SWAP A
            uint8_t a = get_acc();
            set_acc((a << 4) | (a >> 4));
            cycles += 1; return 1;
        }

        case 0xC5: { // XCH A, direct
            uint8_t dir = memory.read_code(pc++);
            uint8_t a = get_acc();
            set_acc(memory.read_direct(dir));
            memory.write_direct(dir, a);
            cycles += 1; return 1;
        }

        case 0xD0: { // POP direct
            uint8_t dir = memory.read_code(pc++);
            memory.write_direct(dir, pop());
            cycles += 2; return 2;
        }

        case 0xD2: { // SETB bit
            uint8_t bit_addr = memory.read_code(pc++);
            memory.set_bit(bit_addr);
            cycles += 1; return 1;
        }

        case 0xD3: // SETB C
            set_cy(true);
            cycles += 1; return 1;

        case 0xD4: { // DA A
            uint8_t a = get_acc();
            uint8_t incr = 0;
            bool carry = get_cy();
            if (get_ac() || ((a & 0x0F) > 9)) {
                incr |= 0x06;
            }
            if (carry || ((a + incr) > 0x9F) || (a > 0x99)) {
                incr |= 0x60;
                carry = true;
            }
            set_acc(a + incr);
            set_cy(carry);
            cycles += 1; return 1;
        }

        case 0xD5: { // DJNZ direct, rel
            uint8_t dir = memory.read_code(pc++);
            int8_t rel = static_cast<int8_t>(memory.read_code(pc++));
            uint8_t val = memory.read_direct(dir) - 1;
            memory.write_direct(dir, val);
            if (val != 0) pc += rel;
            cycles += 2; return 2;
        }

        case 0xE0: // MOVX A, @DPTR
            set_acc(memory.read_xdata(get_dptr()));
            cycles += 2; return 2;

        case 0xE2: // MOVX A, @R0
            set_acc(memory.read_xdata(get_rn(0)));
            cycles += 2; return 2;

        case 0xE3: // MOVX A, @R1
            set_acc(memory.read_xdata(get_rn(1)));
            cycles += 2; return 2;

        case 0xE4: // CLR A
            set_acc(0);
            cycles += 1; return 1;

        case 0xE5: { // MOV A, direct
            uint8_t dir = memory.read_code(pc++);
            set_acc(memory.read_direct(dir));
            cycles += 1; return 1;
        }

        case 0xF0: // MOVX @DPTR, A
            memory.write_xdata(get_dptr(), get_acc());
            cycles += 2; return 2;

        case 0xF2: // MOVX @R0, A
            memory.write_xdata(get_rn(0), get_acc());
            cycles += 2; return 2;

        case 0xF3: // MOVX @R1, A
            memory.write_xdata(get_rn(1), get_acc());
            cycles += 2; return 2;

        case 0xF4: // CPL A
            set_acc(~get_acc());
            cycles += 1; return 1;

        case 0xF5: { // MOV direct, A
            uint8_t dir = memory.read_code(pc++);
            memory.write_direct(dir, get_acc());
            cycles += 1; return 1;
        }

        default:
            std::cerr << "Unrecognized opcode: 0x" << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(opcode) << " at PC=0x" << (pc - 1) << std::dec << std::endl;
            halted = true;
            return 0;
    }
}

std::string CPU::disassemble_current() const {
    DisassemblyResult res = Disassembler::disassemble(memory, pc);
    return res.text;
}

void CPU::print_status() const {
    std::cout << "PC: 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << pc
              << " | ACC: 0x" << std::setw(2) << static_cast<int>(get_acc())
              << " | B: 0x" << std::setw(2) << static_cast<int>(get_b())
              << " | DPTR: 0x" << std::setw(4) << get_dptr()
              << " | SP: 0x" << std::setw(2) << static_cast<int>(get_sp())
              << " | PSW: 0x" << std::setw(2) << static_cast<int>(get_psw())
              << " [CY:" << get_cy()
              << " AC:" << get_ac()
              << " OV:" << get_ov()
              << " P:" << get_p()
              << " Bank:" << static_cast<int>(get_active_bank())
              << "] | Cycles: " << std::dec << cycles << "\n";

    std::cout << "R0-R7: ";
    for (int i = 0; i < 8; ++i) {
        std::cout << "R" << i << ":0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                  << static_cast<int>(get_rn(i)) << (i == 7 ? "" : " ");
    }
    std::cout << "\nNext:  " << disassemble_current() << "\n";
    std::cout << std::nouppercase << std::dec;
}

void CPU::dump_memory() const {
    memory.dump();
}
