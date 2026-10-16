#pragma once
#include <cstdint>

namespace sfr {
    // Special Function Register (SFR) direct memory addresses (0x80 - 0xFF)
    constexpr uint8_t P0   = 0x80; // Port 0 (bit-addressable)
    constexpr uint8_t SP   = 0x81; // Stack Pointer
    constexpr uint8_t DPL  = 0x82; // Data Pointer Low byte
    constexpr uint8_t DPH  = 0x83; // Data Pointer High byte
    constexpr uint8_t PCON = 0x87; // Power Control
    constexpr uint8_t TCON = 0x88; // Timer Control (bit-addressable)
    constexpr uint8_t TMOD = 0x89; // Timer Mode
    constexpr uint8_t TL0  = 0x8A; // Timer 0 Low byte
    constexpr uint8_t TL1  = 0x8B; // Timer 1 Low byte
    constexpr uint8_t TH0  = 0x8C; // Timer 0 High byte
    constexpr uint8_t TH1  = 0x8D; // Timer 1 High byte
    constexpr uint8_t P1   = 0x90; // Port 1 (bit-addressable)
    constexpr uint8_t SCON = 0x98; // Serial Control (bit-addressable)
    constexpr uint8_t SBUF = 0x99; // Serial Data Buffer
    constexpr uint8_t P2   = 0xA0; // Port 2 (bit-addressable)
    constexpr uint8_t IE   = 0xA8; // Interrupt Enable (bit-addressable)
    constexpr uint8_t P3   = 0xB0; // Port 3 (bit-addressable)
    constexpr uint8_t IP   = 0xB8; // Interrupt Priority (bit-addressable)
    constexpr uint8_t PSW  = 0xD0; // Program Status Word (bit-addressable)
    constexpr uint8_t ACC  = 0xE0; // Accumulator (bit-addressable)
    constexpr uint8_t B    = 0xF0; // B Register (bit-addressable)

    // PSW Bit Masks
    constexpr uint8_t PSW_P   = 0x01; // Bit 0: Parity Flag (1 if ACC has odd # of 1 bits)
    constexpr uint8_t PSW_F1  = 0x02; // Bit 1: User Flag 1 / Reserved
    constexpr uint8_t PSW_OV  = 0x04; // Bit 2: Overflow Flag
    constexpr uint8_t PSW_RS0 = 0x08; // Bit 3: Register Bank Select 0
    constexpr uint8_t PSW_RS1 = 0x10; // Bit 4: Register Bank Select 1
    constexpr uint8_t PSW_F0  = 0x20; // Bit 5: User Flag 0
    constexpr uint8_t PSW_AC  = 0x40; // Bit 6: Auxiliary Carry Flag
    constexpr uint8_t PSW_CY  = 0x80; // Bit 7: Carry Flag
}
