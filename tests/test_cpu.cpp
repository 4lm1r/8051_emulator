#include "memory.hpp"
#include "cpu.hpp"
#include "sfr.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

bool test_reset() {
    Memory mem;
    CPU cpu(mem);
    TEST_ASSERT(cpu.get_pc() == 0x0000, "PC should reset to 0x0000");
    TEST_ASSERT(cpu.get_sp() == 0x07, "SP should reset to 0x07");
    TEST_ASSERT(cpu.get_acc() == 0x00, "ACC should reset to 0x00");
    TEST_ASSERT(cpu.get_b() == 0x00, "B should reset to 0x00");
    TEST_ASSERT(cpu.get_psw() == 0x00, "PSW should reset to 0x00");
    TEST_ASSERT(cpu.get_dptr() == 0x0000, "DPTR should reset to 0x0000");
    TEST_ASSERT(mem.read_direct(sfr::P0) == 0xFF, "P0 should reset to 0xFF");
    TEST_ASSERT(mem.read_direct(sfr::P1) == 0xFF, "P1 should reset to 0xFF");
    return true;
}

bool test_data_transfer() {
    Memory mem;
    CPU cpu(mem);

    // MOV R0, #0x42
    mem.write_code(0, 0x78); mem.write_code(1, 0x42);
    // MOV A, R0
    mem.write_code(2, 0xE8);
    // MOV 0x30, A (MOV direct, A)
    mem.write_code(3, 0xF5); mem.write_code(4, 0x30);
    // MOV DPTR, #0x1234
    mem.write_code(5, 0x90); mem.write_code(6, 0x12); mem.write_code(7, 0x34);
    // MOVX @DPTR, A
    mem.write_code(8, 0xF0);

    cpu.step(); // MOV R0, #0x42
    TEST_ASSERT(cpu.get_rn(0) == 0x42, "R0 should be 0x42");

    cpu.step(); // MOV A, R0
    TEST_ASSERT(cpu.get_acc() == 0x42, "ACC should be 0x42");

    cpu.step(); // MOV 0x30, A
    TEST_ASSERT(mem.read_direct(0x30) == 0x42, "RAM[0x30] should be 0x42");

    cpu.step(); // MOV DPTR, #0x1234
    TEST_ASSERT(cpu.get_dptr() == 0x1234, "DPTR should be 0x1234");

    cpu.step(); // MOVX @DPTR, A
    TEST_ASSERT(mem.read_xdata(0x1234) == 0x42, "XDATA[0x1234] should be 0x42");

    return true;
}

bool test_alu_add() {
    Memory mem;
    CPU cpu(mem);

    // MOV A, #0x7F
    mem.write_code(0, 0x74); mem.write_code(1, 0x7F);
    // ADD A, #0x01 -> result 0x80 (OV = 1, AC = 1, CY = 0, P = 1)
    mem.write_code(2, 0x24); mem.write_code(3, 0x01);
    // ADDC A, #0x80 -> with CY=0, 0x80 + 0x80 = 0x00 (CY = 1, OV = 1, P = 0)
    mem.write_code(4, 0x34); mem.write_code(5, 0x80);

    cpu.step();
    TEST_ASSERT(cpu.get_acc() == 0x7F, "ACC should be 0x7F");

    cpu.step();
    TEST_ASSERT(cpu.get_acc() == 0x80, "ACC should be 0x80");
    TEST_ASSERT(cpu.get_ov() == true, "OV should be set for 0x7F + 1");
    TEST_ASSERT(cpu.get_ac() == true, "AC should be set for 0x7F + 1");
    TEST_ASSERT(cpu.get_cy() == false, "CY should be clear");
    TEST_ASSERT(cpu.get_p() == true, "Parity of 0x80 should be 1 (odd)");

    cpu.step();
    TEST_ASSERT(cpu.get_acc() == 0x00, "ACC should be 0x00");
    TEST_ASSERT(cpu.get_cy() == true, "CY should be set for 0x80 + 0x80");
    TEST_ASSERT(cpu.get_ov() == true, "OV should be set for 0x80 + 0x80");
    TEST_ASSERT(cpu.get_p() == false, "Parity of 0x00 should be 0 (even)");

    return true;
}

bool test_alu_subb() {
    Memory mem;
    CPU cpu(mem);

    // MOV A, #0x10
    mem.write_code(0, 0x74); mem.write_code(1, 0x10);
    // SUBB A, #0x20 -> CY=0, 0x10 - 0x20 = 0xF0 (CY = 1, AC = 0, OV = 0)
    mem.write_code(2, 0x94); mem.write_code(3, 0x20);

    cpu.step();
    cpu.step();
    TEST_ASSERT(cpu.get_acc() == 0xF0, "ACC should be 0xF0");
    TEST_ASSERT(cpu.get_cy() == true, "CY should be set (borrow)");

    return true;
}

bool test_mul_div() {
    Memory mem;
    CPU cpu(mem);

    // MOV A, #10; MOV B, #20; MUL AB -> ACC=200, B=0, OV=0, CY=0
    mem.write_code(0, 0x74); mem.write_code(1, 10);
    mem.write_code(2, 0x75); mem.write_code(3, sfr::B); mem.write_code(4, 20);
    mem.write_code(5, 0xA4); // MUL AB

    // MOV A, #50; MOV B, #10; MUL AB -> 500 = 0x01F4 -> ACC=0xF4, B=0x01, OV=1
    mem.write_code(6, 0x74); mem.write_code(7, 50);
    mem.write_code(8, 0x75); mem.write_code(9, sfr::B); mem.write_code(10, 10);
    mem.write_code(11, 0xA4); // MUL AB

    // MOV A, #53; MOV B, #10; DIV AB -> ACC=5, B=3, OV=0, CY=0
    mem.write_code(12, 0x74); mem.write_code(13, 53);
    mem.write_code(14, 0x75); mem.write_code(15, sfr::B); mem.write_code(16, 10);
    mem.write_code(17, 0x84); // DIV AB

    // MOV B, #0; DIV AB -> OV=1
    mem.write_code(18, 0x75); mem.write_code(19, sfr::B); mem.write_code(20, 0);
    mem.write_code(21, 0x84); // DIV AB

    cpu.step(); cpu.step(); cpu.step();
    TEST_ASSERT(cpu.get_acc() == 200, "10 * 20: ACC should be 200");
    TEST_ASSERT(cpu.get_b() == 0, "10 * 20: B should be 0");
    TEST_ASSERT(cpu.get_ov() == false, "10 * 20: OV should be 0");

    cpu.step(); cpu.step(); cpu.step();
    TEST_ASSERT(cpu.get_acc() == 0xF4, "50 * 10: ACC should be 0xF4");
    TEST_ASSERT(cpu.get_b() == 0x01, "50 * 10: B should be 0x01");
    TEST_ASSERT(cpu.get_ov() == true, "50 * 10: OV should be 1");

    cpu.step(); cpu.step(); cpu.step();
    TEST_ASSERT(cpu.get_acc() == 5, "53 / 10: ACC should be 5");
    TEST_ASSERT(cpu.get_b() == 3, "53 / 10: B should be 3");
    TEST_ASSERT(cpu.get_ov() == false, "53 / 10: OV should be 0");

    cpu.step(); cpu.step();
    TEST_ASSERT(cpu.get_ov() == true, "Division by 0: OV should be 1");

    return true;
}

bool test_da_a() {
    Memory mem;
    CPU cpu(mem);

    // BCD addition: 0x28 + 0x37 = 0x5F -> DA A -> 0x65
    // MOV A, #0x28
    mem.write_code(0, 0x74); mem.write_code(1, 0x28);
    // ADD A, #0x37
    mem.write_code(2, 0x24); mem.write_code(3, 0x37);
    // DA A
    mem.write_code(4, 0xD4);

    cpu.step();
    cpu.step();
    TEST_ASSERT(cpu.get_acc() == 0x5F, "Intermediate binary sum should be 0x5F");
    cpu.step();
    TEST_ASSERT(cpu.get_acc() == 0x65, "DA A should adjust 0x5F to BCD 0x65");

    return true;
}

bool test_bit_operations() {
    Memory mem;
    CPU cpu(mem);

    // SETB 0x05 (RAM 0x20, bit 5)
    mem.write_code(0, 0xD2); mem.write_code(1, 0x05);
    // MOV C, 0x05
    mem.write_code(2, 0xA2); mem.write_code(3, 0x05);
    // CLR 0x05
    mem.write_code(4, 0xC2); mem.write_code(5, 0x05);
    // MOV 0x06, C
    mem.write_code(6, 0x92); mem.write_code(7, 0x06);

    cpu.step();
    TEST_ASSERT(mem.read_bit(0x05) == true, "Bit 0x05 should be set");
    TEST_ASSERT(mem.read_direct(0x20) == (1 << 5), "RAM 0x20 should have bit 5 set");

    cpu.step();
    TEST_ASSERT(cpu.get_cy() == true, "Carry flag should receive bit 0x05");

    cpu.step();
    TEST_ASSERT(mem.read_bit(0x05) == false, "Bit 0x05 should be cleared");

    cpu.step();
    TEST_ASSERT(mem.read_bit(0x06) == true, "Bit 0x06 should have received Carry (true)");

    return true;
}

bool test_branching_and_loops() {
    Memory mem;
    CPU cpu(mem);

    // Counter loop: R2 = 5; loop: DJNZ R2, loop
    // 0000: MOV R2, #5
    mem.write_code(0, 0x7A); mem.write_code(1, 0x05);
    // 0002: DJNZ R2, -2 (relative offset -2 points back to 0002)
    mem.write_code(2, 0xDA); mem.write_code(3, static_cast<uint8_t>(-2));
    // 0004: NOP
    mem.write_code(4, 0x00);

    cpu.step(); // R2 = 5, PC = 2
    TEST_ASSERT(cpu.get_rn(2) == 5, "R2 should be 5");

    // 5 iterations of DJNZ
    for (int expected = 4; expected >= 0; --expected) {
        cpu.step();
        TEST_ASSERT(cpu.get_rn(2) == expected, "R2 decrement mismatch");
        if (expected > 0) {
            TEST_ASSERT(cpu.get_pc() == 0x0002, "Should branch back to 0x0002");
        } else {
            TEST_ASSERT(cpu.get_pc() == 0x0004, "Should fall through to 0x0004");
        }
    }

    return true;
}

bool test_call_and_ret() {
    Memory mem;
    CPU cpu(mem);

    // 0000: LCALL 0x0100
    mem.write_code(0, 0x12); mem.write_code(1, 0x01); mem.write_code(2, 0x00);
    // 0003: NOP
    mem.write_code(3, 0x00);

    // Subroutine at 0x0100:
    // 0100: MOV A, #0x99
    mem.write_code(0x0100, 0x74); mem.write_code(0x0101, 0x99);
    // 0102: RET
    mem.write_code(0x0102, 0x22);

    cpu.step(); // LCALL 0x0100
    TEST_ASSERT(cpu.get_pc() == 0x0100, "PC should jump to subroutine 0x0100");
    TEST_ASSERT(cpu.get_sp() == 0x09, "SP should increase by 2 (0x07 -> 0x09)");

    cpu.step(); // MOV A, #0x99
    TEST_ASSERT(cpu.get_acc() == 0x99, "ACC should be 0x99");

    cpu.step(); // RET
    TEST_ASSERT(cpu.get_pc() == 0x0003, "PC should return to 0x0003");
    TEST_ASSERT(cpu.get_sp() == 0x07, "SP should restore to 0x07");

    return true;
}

bool test_intel_hex_loader() {
    Memory mem;
    std::string test_filename = "test_rom.hex";
    {
        std::ofstream out(test_filename);
        // :030000007855E848
        // Byte count: 3, Addr: 0000, Type: 00, Data: 78 55 E8, Checksum: 48
        // 03 + 00 + 00 + 00 + 78 + 55 + E8 = 1B8 -> two's complement = 0x48
        out << ":030000007855E848\n";
        out << ":00000001FF\n"; // EOF
    }

    bool ok = mem.load_hex(test_filename);
    std::remove(test_filename.c_str());

    TEST_ASSERT(ok, "HEX loader should succeed");
    TEST_ASSERT(mem.read_code(0) == 0x78, "Code[0] should be 0x78");
    TEST_ASSERT(mem.read_code(1) == 0x55, "Code[1] should be 0x55");
    TEST_ASSERT(mem.read_code(2) == 0xE8, "Code[2] should be 0xE8");

    return true;
}

int main() {
    std::cout << "Running 8051 Emulator Test Suite...\n";
    int passed = 0;
    int total = 0;

    auto run_test = [&](const char* name, bool (*fn)()) {
        total++;
        std::cout << "  [TEST] " << name << " ... ";
        if (fn()) {
            std::cout << "PASS\n";
            passed++;
        }
    };

    run_test("Reset State", test_reset);
    run_test("Data Transfer (MOV, MOVX, DPTR)", test_data_transfer);
    run_test("ALU Addition & Flags (CY, AC, OV, P)", test_alu_add);
    run_test("ALU Subtraction & Borrow", test_alu_subb);
    run_test("Multiplication & Division (MUL, DIV)", test_mul_div);
    run_test("Decimal Adjust Accumulator (DA A)", test_da_a);
    run_test("Bit-addressable Memory & Boolean Logic", test_bit_operations);
    run_test("Branching & DJNZ Loops", test_branching_and_loops);
    run_test("Subroutine Calls (LCALL) & Returns (RET)", test_call_and_ret);
    run_test("Intel HEX Parser & Loader", test_intel_hex_loader);

    std::cout << "\nResults: " << passed << "/" << total << " tests passed.\n";
    return (passed == total) ? 0 : 1;
}
