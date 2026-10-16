#include "tui.hpp"
#include "disassembler.hpp"
#include "sfr.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

enum ColorPairs {
    PAIR_DEFAULT = 0,
    PAIR_TITLE = 1,
    PAIR_MENU_SEL = 2,
    PAIR_PC_LINE = 3,
    PAIR_BP = 4,
    PAIR_HEADER = 5,
    PAIR_STATUS = 6,
    PAIR_CMD = 7,
    PAIR_ACCENT = 8,
    PAIR_NONZERO = 9
};

static volatile sig_atomic_t g_terminal_resized = 0;
static void sigwinch_handler(int) {
    g_terminal_resized = 1;
}

static std::string to_hex16(uint16_t val) {
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << static_cast<int>(val);
    return ss.str();
}

static std::string to_bin8(uint8_t val) {
    std::string s;
    for (int i = 7; i >= 0; --i) {
        s += ((val >> i) & 1) ? '1' : '0';
    }
    return s;
}

TUI::TUI(CPU& c, Memory& m, Debugger& dbg, const std::string& loaded_file)
    : cpu(c), memory(m), debugger(dbg), filename(loaded_file.empty() ? "Built-in Demo" : loaded_file),
      current_view(MainView::Disassembly), menu_selected_idx(0), scroll_offset(0),
      is_command_mode(false), is_running_continuous(false),
      win_title(nullptr), win_menu(nullptr), win_main(nullptr), win_status(nullptr), win_cmd(nullptr) {

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    signal(SIGWINCH, sigwinch_handler);

    init_colors();
    create_windows();

    center_disassembly_on_pc();
    log_message("8051 Emulator started. Press Space/F7 to step, F5 to run, ':' for command.");
}

TUI::~TUI() {
    signal(SIGWINCH, SIG_DFL);
    destroy_windows();
    endwin();
}

void TUI::init_colors() {
    if (has_colors()) {
        start_color();
        use_default_colors();

        init_pair(PAIR_TITLE, COLOR_WHITE, COLOR_BLUE);
        init_pair(PAIR_MENU_SEL, COLOR_BLACK, COLOR_CYAN);
        init_pair(PAIR_PC_LINE, COLOR_BLACK, COLOR_YELLOW);
        init_pair(PAIR_BP, COLOR_RED, -1);
        init_pair(PAIR_HEADER, COLOR_CYAN, -1);
        init_pair(PAIR_STATUS, COLOR_WHITE, COLOR_BLUE);
        init_pair(PAIR_CMD, COLOR_GREEN, -1);
        init_pair(PAIR_ACCENT, COLOR_YELLOW, -1);
        init_pair(PAIR_NONZERO, COLOR_GREEN, -1);
    }
}

void TUI::create_windows() {
    int h = 0, w = 0;
    getmaxyx(stdscr, h, w);

    if (h < 12 || w < 50) {
        // Fallback for very small terminal size
        win_main = newwin(h, w, 0, 0);
        return;
    }

    // Proportional & responsive menu width:
    int menu_w = (w >= 120) ? 30 : ((w >= 90) ? 26 : 24);
    int main_w = w - menu_w;
    int mid_h = h - 3;
    if (mid_h < 4) mid_h = 4;

    win_title = newwin(1, w, 0, 0);
    win_menu = newwin(mid_h, menu_w, 1, 0);
    win_main = newwin(mid_h, main_w, 1, menu_w);
    win_status = newwin(1, w, h - 2, 0);
    win_cmd = newwin(1, w, h - 1, 0);

    keypad(win_main, TRUE);
    keypad(win_menu, TRUE);
    keypad(win_cmd, TRUE);
}

void TUI::destroy_windows() {
    if (win_title) { delwin(win_title); win_title = nullptr; }
    if (win_menu) { delwin(win_menu); win_menu = nullptr; }
    if (win_main) { delwin(win_main); win_main = nullptr; }
    if (win_status) { delwin(win_status); win_status = nullptr; }
    if (win_cmd) { delwin(win_cmd); win_cmd = nullptr; }
}

void TUI::resize_all() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        resizeterm(ws.ws_row, ws.ws_col);
    } else {
        resizeterm(0, 0);
    }

    endwin();
    refresh();
    clear();

    destroy_windows();
    create_windows();

    clear();
    refresh();
    render_all();
}

void TUI::log_message(const std::string& msg) {
    status_message = msg;
    message_log.push_back(msg);
    if (message_log.size() > 200) {
        message_log.pop_front();
    }
}

void TUI::center_disassembly_on_pc() {
    scroll_offset = (cpu.get_pc() >= 5) ? (cpu.get_pc() - 5) : 0;
}

void TUI::render_title() {
    if (!win_title) return;
    int w = getmaxx(win_title);
    werase(win_title);
    wbkgd(win_title, COLOR_PAIR(PAIR_TITLE));

    std::string state_str = cpu.is_halted() ? "HALTED" : (is_running_continuous ? "RUNNING" : "PAUSED");
    std::string title = " 8051 MICROPROCESSOR EMULATOR ";
    std::string info = "[ROM: " + filename + "]  State: " + state_str + "  Cycles: " + std::to_string(cpu.get_cycles()) + " ";

    mvwprintw(win_title, 0, 0, "%s", title.c_str());
    if (static_cast<int>(info.length()) < w) {
        mvwprintw(win_title, 0, w - info.length(), "%s", info.c_str());
    }
    wrefresh(win_title);
}

void TUI::render_menu() {
    if (!win_menu) return;
    int h, w;
    getmaxyx(win_menu, h, w);
    werase(win_menu);
    box(win_menu, 0, 0);

    // Title
    wattron(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_menu, 0, 2, "[ MENU & ACTIONS ]");
    wattroff(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    struct MenuOption {
        std::string label;
        std::string key;
    };

    const std::vector<MenuOption> options = {
        {"Step (1 inst)",  "F7/Spc"},
        {"Run / Pause",    "F5/r"},
        {"Reset CPU",      "F9"},
        {"Disassembly",    "F1"},
        {"Internal RAM",   "F2"},
        {"SFR Registers",  "F3"},
        {"External XDATA", "F4"},
        {"Breakpoints",    "F8"},
        {"Execution Log",  "L"},
        {"Quit",           "q"}
    };

    int y = 1;
    for (size_t i = 0; i < options.size() && y < h - 10; ++i, ++y) {
        bool selected = (static_cast<int>(i) == menu_selected_idx);

        if (selected) {
            wattron(win_menu, COLOR_PAIR(PAIR_MENU_SEL) | A_REVERSE | A_BOLD);
        }

        std::string line = (selected ? "> " : "  ") + options[i].label;
        std::string key_str = options[i].key;

        mvwprintw(win_menu, y, 1, "%-*s", w - 2, "");
        mvwprintw(win_menu, y, 1, "%s", line.c_str());
        if (w >= 24) {
            mvwprintw(win_menu, y, w - 1 - key_str.length(), "%s", key_str.c_str());
        }

        if (selected) {
            wattroff(win_menu, COLOR_PAIR(PAIR_MENU_SEL) | A_REVERSE | A_BOLD);
        }
    }

    // Divider
    if (y < h - 7) {
        mvwhline(win_menu, y, 1, ACS_HLINE, w - 2);
        wattron(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
        mvwprintw(win_menu, y, 2, "[ CPU REGISTERS ]");
        wattroff(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
        y++;

        // Register HUD
        mvwprintw(win_menu, y++, 2, "PC: 0x%04X   SP: 0x%02X", cpu.get_pc(), cpu.get_sp());
        mvwprintw(win_menu, y++, 2, "DPTR: 0x%04X", cpu.get_dptr());
        mvwprintw(win_menu, y++, 2, "ACC: 0x%02X     B: 0x%02X", cpu.get_acc(), cpu.get_b());
        mvwprintw(win_menu, y++, 2, "PSW: 0x%02X   [Bank %d]", cpu.get_psw(), cpu.get_active_bank());
        mvwprintw(win_menu, y++, 2, "CY:%d AC:%d OV:%d P:%d", cpu.get_cy(), cpu.get_ac(), cpu.get_ov(), cpu.get_p());

        if (y < h - 4) {
            mvwhline(win_menu, y, 1, ACS_HLINE, w - 2);
            wattron(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
            mvwprintw(win_menu, y, 2, "[ R0 - R7 ]");
            wattroff(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
            y++;

            mvwprintw(win_menu, y++, 2, "R0:0x%02X  R1:0x%02X", cpu.get_rn(0), cpu.get_rn(1));
            mvwprintw(win_menu, y++, 2, "R2:0x%02X  R3:0x%02X", cpu.get_rn(2), cpu.get_rn(3));
            if (y < h - 1) mvwprintw(win_menu, y++, 2, "R4:0x%02X  R5:0x%02X", cpu.get_rn(4), cpu.get_rn(5));
            if (y < h - 1) mvwprintw(win_menu, y++, 2, "R6:0x%02X  R7:0x%02X", cpu.get_rn(6), cpu.get_rn(7));
        }
    }

    wrefresh(win_menu);
}

void TUI::render_main() {
    if (!win_main) return;
    werase(win_main);
    box(win_main, 0, 0);

    switch (current_view) {
        case MainView::Disassembly: render_disassembly(); break;
        case MainView::InternalRAM: render_internal_ram(); break;
        case MainView::SFR:         render_sfr(); break;
        case MainView::ExternalRAM: render_external_ram(); break;
        case MainView::Breakpoints: render_breakpoints(); break;
        case MainView::Log:         render_log(); break;
    }

    wrefresh(win_main);
}

void TUI::render_disassembly() {
    int h, w;
    getmaxyx(win_main, h, w);

    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ DISASSEMBLY / CODE VIEW ] (PgUp/PgDn to scroll, 'c' to center)");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    uint16_t curr_addr = scroll_offset;
    for (int y = 1; y < h - 1; ++y) {
        DisassemblyResult res = Disassembler::disassemble(memory, curr_addr);
        bool is_pc = (curr_addr == cpu.get_pc());
        bool is_bp = debugger.has_breakpoint(curr_addr);

        if (is_pc) {
            wattron(win_main, COLOR_PAIR(PAIR_PC_LINE) | A_BOLD);
        }

        mvwprintw(win_main, y, 1, "%-*s", w - 2, "");

        if (is_bp) {
            wattron(win_main, COLOR_PAIR(PAIR_BP) | A_BOLD);
            mvwprintw(win_main, y, 1, "[*]");
            wattroff(win_main, COLOR_PAIR(PAIR_BP) | A_BOLD);
        } else {
            mvwprintw(win_main, y, 1, "   ");
        }

        mvwprintw(win_main, y, 4, "%s %04X:  %-10s %-6s %-16s",
                  (is_pc ? "=>" : "  "),
                  res.address,
                  res.bytes_hex.c_str(),
                  res.mnemonic.c_str(),
                  res.operands.c_str());

        if (w >= 65) {
            mvwprintw(win_main, y, w - 12, "[%d cycles]", res.cycles);
        }

        if (is_pc) {
            wattroff(win_main, COLOR_PAIR(PAIR_PC_LINE) | A_BOLD);
        }

        curr_addr += res.length;
    }
}

void TUI::render_internal_ram() {
    int h, w;
    getmaxyx(win_main, h, w);
    (void)h;

    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ INTERNAL RAM (0x00-0x7F Direct, 0x80-0xFF Indirect) ]");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    wattron(win_main, COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 1, 2, "Addr  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII");
    wattroff(win_main, COLOR_PAIR(PAIR_HEADER));

    for (int row = 0; row < 16 && (2 + row) < h - 1; ++row) {
        int y = 2 + row;
        uint8_t base_addr = row * 16;

        mvwprintw(win_main, y, 2, "0x%02X: ", base_addr);

        std::string ascii_str;
        for (int col = 0; col < 16; ++col) {
            uint8_t addr = base_addr + col;
            uint8_t val = (addr < 0x80) ? memory.read_direct(addr) : memory.read_indirect(addr);

            int x = 8 + (col * 3) + (col >= 8 ? 1 : 0);

            if (val != 0) {
                wattron(win_main, COLOR_PAIR(PAIR_NONZERO) | A_BOLD);
            }
            mvwprintw(win_main, y, x, "%02X", val);
            if (val != 0) {
                wattroff(win_main, COLOR_PAIR(PAIR_NONZERO) | A_BOLD);
            }

            ascii_str += (val >= 32 && val <= 126) ? static_cast<char>(val) : '.';
        }

        mvwprintw(win_main, y, 59, "|%s|", ascii_str.c_str());

        if (base_addr == 0x00 && w >= 80) mvwprintw(win_main, y, 78, "<- Bank 0-1");
        if (base_addr == 0x10 && w >= 80) mvwprintw(win_main, y, 78, "<- Bank 2-3");
        if (base_addr == 0x20 && w >= 80) mvwprintw(win_main, y, 78, "<- Bit-RAM (0x20-0x2F)");
        if (base_addr == 0x80 && w >= 80) mvwprintw(win_main, y, 78, "<- Upper 128B (Indirect)");
    }
}

void TUI::render_sfr() {
    int h, w;
    getmaxyx(win_main, h, w);

    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ SPECIAL FUNCTION REGISTERS (Direct 0x80-0xFF) ]");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    struct SfrInfo {
        uint8_t addr;
        std::string name;
        std::string desc;
    };

    const std::vector<SfrInfo> sfrs = {
        {sfr::P0,   "P0",   "Port 0 (Bit)"},
        {sfr::SP,   "SP",   "Stack Pointer"},
        {sfr::DPL,  "DPL",  "DPTR Low byte"},
        {sfr::DPH,  "DPH",  "DPTR High byte"},
        {sfr::PCON, "PCON", "Power Control"},
        {sfr::TCON, "TCON", "Timer Control (Bit)"},
        {sfr::TMOD, "TMOD", "Timer Mode"},
        {sfr::TL0,  "TL0",  "Timer 0 Low"},
        {sfr::TL1,  "TL1",  "Timer 1 Low"},
        {sfr::TH0,  "TH0",  "Timer 0 High"},
        {sfr::TH1,  "TH1",  "Timer 1 High"},
        {sfr::P1,   "P1",   "Port 1 (Bit)"},
        {sfr::SCON, "SCON", "Serial Control (Bit)"},
        {sfr::SBUF, "SBUF", "Serial Buffer"},
        {sfr::P2,   "P2",   "Port 2 (Bit)"},
        {sfr::IE,   "IE",   "Interrupt Enable (Bit)"},
        {sfr::P3,   "P3",   "Port 3 (Bit)"},
        {sfr::IP,   "IP",   "Interrupt Priority (Bit)"},
        {sfr::PSW,  "PSW",  "Program Status Word (Bit)"},
        {sfr::ACC,  "ACC",  "Accumulator (Bit)"},
        {sfr::B,    "B",    "B Register (Bit)"}
    };

    bool two_columns = (w >= 75 && h < 24);

    if (two_columns) {
        wattron(win_main, COLOR_PAIR(PAIR_HEADER));
        mvwprintw(win_main, 1, 2, "Addr Name Hex  Binary     Addr Name Hex  Binary");
        wattroff(win_main, COLOR_PAIR(PAIR_HEADER));

        size_t half = (sfrs.size() + 1) / 2;
        for (size_t i = 0; i < half; ++i) {
            int y = 2 + i;
            if (y >= h - 1) break;

            const auto& s1 = sfrs[i];
            uint8_t v1 = memory.read_direct(s1.addr);
            mvwprintw(win_main, y, 2, "0x%02X %-4s 0x%02X %s",
                      s1.addr, s1.name.c_str(), v1, to_bin8(v1).c_str());

            if (i + half < sfrs.size()) {
                const auto& s2 = sfrs[i + half];
                uint8_t v2 = memory.read_direct(s2.addr);
                mvwprintw(win_main, y, 28, "0x%02X %-4s 0x%02X %s",
                          s2.addr, s2.name.c_str(), v2, to_bin8(v2).c_str());
            }
        }
    } else {
        wattron(win_main, COLOR_PAIR(PAIR_HEADER));
        mvwprintw(win_main, 1, 2, "Addr Name  Hex  Decimal  Binary    Description");
        wattroff(win_main, COLOR_PAIR(PAIR_HEADER));

        for (size_t i = 0; i < sfrs.size(); ++i) {
            int y = 2 + i;
            if (y >= h - 1) break;
            const auto& item = sfrs[i];
            uint8_t val = memory.read_direct(item.addr);
            mvwprintw(win_main, y, 2, "0x%02X %-5s 0x%02X %-8d %s  %-s",
                      item.addr,
                      item.name.c_str(),
                      val,
                      val,
                      to_bin8(val).c_str(),
                      (w >= 60 ? item.desc.c_str() : ""));
        }
    }
}

void TUI::render_external_ram() {
    int h, w;
    getmaxyx(win_main, h, w);

    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ EXTERNAL RAM (XDATA 64KB) ] (PgUp/PgDn to scroll)");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    wattron(win_main, COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 1, 2, "Addr    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII");
    wattroff(win_main, COLOR_PAIR(PAIR_HEADER));

    for (int row = 0; row < h - 3; ++row) {
        int y = 2 + row;
        uint32_t base = scroll_offset + (row * 16);
        if (base >= 65536) break;

        mvwprintw(win_main, y, 2, "0x%04X: ", base);

        std::string ascii_str;
        for (int col = 0; col < 16; ++col) {
            uint32_t addr = base + col;
            uint8_t val = (addr < 65536) ? memory.read_xdata(addr) : 0;
            int x = 10 + (col * 3) + (col >= 8 ? 1 : 0);

            if (val != 0) wattron(win_main, COLOR_PAIR(PAIR_NONZERO) | A_BOLD);
            mvwprintw(win_main, y, x, "%02X", val);
            if (val != 0) wattroff(win_main, COLOR_PAIR(PAIR_NONZERO) | A_BOLD);

            ascii_str += (val >= 32 && val <= 126) ? static_cast<char>(val) : '.';
        }

        mvwprintw(win_main, y, 61, "|%s|", ascii_str.c_str());
    }
}

void TUI::render_breakpoints() {
    int h, w;
    getmaxyx(win_main, h, w);
    (void)h;
    (void)w;

    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ ACTIVE BREAKPOINTS ] (Use ':b <addr>' or ':del <addr>')");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    const auto& bps = debugger.get_breakpoints();
    if (bps.empty()) {
        mvwprintw(win_main, 3, 4, "No breakpoints currently set.");
        mvwprintw(win_main, 5, 4, "Tip: In command bar at bottom, type ':b 0x0010' to add a breakpoint.");
        return;
    }

    int y = 2;
    for (uint16_t bp : bps) {
        if (y >= h - 1) break;
        DisassemblyResult res = Disassembler::disassemble(memory, bp);
        wattron(win_main, COLOR_PAIR(PAIR_BP) | A_BOLD);
        mvwprintw(win_main, y, 3, "[*]");
        wattroff(win_main, COLOR_PAIR(PAIR_BP) | A_BOLD);

        mvwprintw(win_main, y, 8, "0x%04X:  %-10s %-6s %-16s",
                  bp, res.bytes_hex.c_str(), res.mnemonic.c_str(), res.operands.c_str());
        y++;
    }
}

void TUI::render_log() {
    int h, w;
    getmaxyx(win_main, h, w);

    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ EXECUTION LOG & MESSAGES ]");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    int max_lines = h - 2;
    int start_idx = static_cast<int>(message_log.size()) - max_lines;
    if (start_idx < 0) start_idx = 0;

    int y = 1;
    for (size_t i = start_idx; i < message_log.size() && y < h - 1; ++i, ++y) {
        mvwprintw(win_main, y, 2, "%-*s", w - 4, message_log[i].c_str());
    }
}

void TUI::render_status() {
    if (!win_status) return;
    int w = getmaxx(win_status);
    werase(win_status);
    wbkgd(win_status, COLOR_PAIR(PAIR_STATUS));

    DisassemblyResult cur = Disassembler::disassemble(memory, cpu.get_pc());
    std::string line = " [STATUS] PC: 0x" + to_hex16(cpu.get_pc()) +
                       " | Next: " + cur.mnemonic + " " + cur.operands +
                       " | Info: " + status_message;

    mvwprintw(win_status, 0, 0, "%-*s", w, line.c_str());
    wrefresh(win_status);
}

void TUI::render_cmd() {
    if (!win_cmd) return;
    int w = getmaxx(win_cmd);
    werase(win_cmd);

    if (is_command_mode) {
        curs_set(1);
        wattron(win_cmd, COLOR_PAIR(PAIR_CMD) | A_BOLD);
        mvwprintw(win_cmd, 0, 0, ":%s", command_buffer.c_str());
        wattroff(win_cmd, COLOR_PAIR(PAIR_CMD) | A_BOLD);
        wmove(win_cmd, 0, 1 + command_buffer.length());
    } else {
        curs_set(0);
        mvwprintw(win_cmd, 0, 0, "%-*s", w,
                  " [COMMAND] Press ':' to enter command, Space/F7:Step, F5:Run, F1-F4:Views, q:Quit");
    }
    wrefresh(win_cmd);
}

void TUI::render_all() {
    if (!win_title) {
        if (win_main) {
            int h, w;
            getmaxyx(win_main, h, w);
            werase(win_main);
            box(win_main, 0, 0);
            std::string msg = "Terminal too small: " + std::to_string(w) + "x" + std::to_string(h);
            std::string tip = "Please resize window to at least 50x12.";
            mvwprintw(win_main, std::max(1, h / 2 - 1), std::max(1, (w - static_cast<int>(msg.length())) / 2), "%s", msg.c_str());
            mvwprintw(win_main, std::max(1, h / 2), std::max(1, (w - static_cast<int>(tip.length())) / 2), "%s", tip.c_str());
            wrefresh(win_main);
        }
        return;
    }

    render_title();
    render_menu();
    render_main();
    render_status();
    render_cmd();
}

void TUI::execute_menu_action(int idx) {
    switch (idx) {
        case 0: // Step
            if (!cpu.is_halted()) {
                uint8_t c = cpu.step();
                center_disassembly_on_pc();
                log_message("Stepped 1 instruction (" + std::to_string(c) + " cycles).");
            } else {
                log_message("CPU is halted.");
            }
            break;
        case 1: // Run / Pause
            is_running_continuous = !is_running_continuous;
            log_message(is_running_continuous ? "Continuous execution running..." : "Execution paused.");
            break;
        case 2: // Reset
            cpu.reset();
            center_disassembly_on_pc();
            log_message("CPU reset to power-on state.");
            break;
        case 3: current_view = MainView::Disassembly; center_disassembly_on_pc(); break;
        case 4: current_view = MainView::InternalRAM; break;
        case 5: current_view = MainView::SFR; break;
        case 6: current_view = MainView::ExternalRAM; scroll_offset = 0; break;
        case 7: current_view = MainView::Breakpoints; break;
        case 8: current_view = MainView::Log; break;
        case 9: // Quit
            break;
    }
}

void TUI::execute_command(const std::string& cmd_str) {
    std::istringstream iss(cmd_str);
    std::string cmd;
    if (!(iss >> cmd)) return;

    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    auto parse_hex_or_dec = [](const std::string& s) -> uint32_t {
        if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
            return std::stoul(s.substr(2), nullptr, 16);
        }
        for (char ch : s) {
            if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                return std::stoul(s, nullptr, 16);
            }
        }
        return std::stoul(s, nullptr, 10);
    };

    if (cmd == "s" || cmd == "step") {
        std::string n_str;
        size_t n = 1;
        if (iss >> n_str) {
            try { n = parse_hex_or_dec(n_str); } catch (...) { n = 1; }
        }
        for (size_t i = 0; i < n && !cpu.is_halted(); ++i) {
            cpu.step();
            if (debugger.has_breakpoint(cpu.get_pc())) {
                log_message("Breakpoint reached at 0x" + to_hex16(cpu.get_pc()));
                break;
            }
        }
        center_disassembly_on_pc();
        log_message("Executed " + std::to_string(n) + " steps.");
    } else if (cmd == "r" || cmd == "run" || cmd == "c") {
        is_running_continuous = true;
        log_message("Running...");
    } else if (cmd == "b" || cmd == "break") {
        std::string addr_s;
        if (iss >> addr_s) {
            try {
                uint16_t addr = static_cast<uint16_t>(parse_hex_or_dec(addr_s));
                debugger.add_breakpoint(addr);
                log_message("Breakpoint set at 0x" + to_hex16(addr));
            } catch (...) {
                log_message("Invalid address format.");
            }
        }
    } else if (cmd == "d" || cmd == "del") {
        std::string addr_s;
        if (iss >> addr_s) {
            try {
                uint16_t addr = static_cast<uint16_t>(parse_hex_or_dec(addr_s));
                debugger.remove_breakpoint(addr);
                log_message("Breakpoint removed at 0x" + to_hex16(addr));
            } catch (...) {
                log_message("Invalid address format.");
            }
        }
    } else if (cmd == "reset") {
        cpu.reset();
        center_disassembly_on_pc();
        log_message("CPU reset.");
    } else if (cmd == "view" || cmd == "v") {
        std::string v;
        if (iss >> v) {
            std::transform(v.begin(), v.end(), v.begin(), ::tolower);
            if (v == "disasm" || v == "code") current_view = MainView::Disassembly;
            else if (v == "ram") current_view = MainView::InternalRAM;
            else if (v == "sfr") current_view = MainView::SFR;
            else if (v == "xdata" || v == "x") current_view = MainView::ExternalRAM;
            else if (v == "bp" || v == "breaks") current_view = MainView::Breakpoints;
            else if (v == "log") current_view = MainView::Log;
        }
    } else if (cmd == "load") {
        std::string type, path;
        if (iss >> type >> path) {
            std::transform(type.begin(), type.end(), type.begin(), ::tolower);
            if (type == "hex") {
                if (memory.load_hex(path)) {
                    filename = path;
                    cpu.reset();
                    center_disassembly_on_pc();
                    log_message("Loaded HEX: " + path);
                } else {
                    log_message("Failed to load HEX: " + path);
                }
            } else if (type == "bin") {
                if (memory.load_bin(path, 0x0000)) {
                    filename = path;
                    cpu.reset();
                    center_disassembly_on_pc();
                    log_message("Loaded BIN: " + path);
                } else {
                    log_message("Failed to load BIN: " + path);
                }
            }
        }
    } else if (cmd == "q" || cmd == "quit") {
        endwin();
        exit(0);
    } else {
        log_message("Unknown command: '" + cmd + "'. Try: step, run, b <addr>, del <addr>, reset, view, load, quit");
    }
}

void TUI::handle_input(int ch, bool& should_exit) {
    if (ch == KEY_RESIZE) {
        resize_all();
        return;
    }

    if (is_command_mode) {
        if (ch == 27) { // Escape cancels
            is_command_mode = false;
            command_buffer.clear();
        } else if (ch == '\n' || ch == KEY_ENTER) {
            execute_command(command_buffer);
            is_command_mode = false;
            command_buffer.clear();
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (!command_buffer.empty()) {
                command_buffer.pop_back();
            }
        } else if (ch >= 32 && ch <= 126) {
            command_buffer += static_cast<char>(ch);
        }
        return;
    }

    switch (ch) {
        case KEY_UP:
            menu_selected_idx = (menu_selected_idx - 1 + 10) % 10;
            break;

        case KEY_DOWN:
            menu_selected_idx = (menu_selected_idx + 1) % 10;
            break;

        case '\n':
        case KEY_ENTER:
            if (menu_selected_idx == 9) { // Quit
                should_exit = true;
            } else {
                execute_menu_action(menu_selected_idx);
            }
            break;

        case ' ':
        case 's':
        case KEY_F(7):
            execute_menu_action(0); // Step
            break;

        case 'r':
        case KEY_F(5):
            execute_menu_action(1); // Run / Pause
            break;

        case KEY_F(9):
            execute_menu_action(2); // Reset
            break;

        case KEY_F(1):
        case '1':
            current_view = MainView::Disassembly;
            menu_selected_idx = 3;
            center_disassembly_on_pc();
            break;

        case KEY_F(2):
        case '2':
            current_view = MainView::InternalRAM;
            menu_selected_idx = 4;
            break;

        case KEY_F(3):
        case '3':
            current_view = MainView::SFR;
            menu_selected_idx = 5;
            break;

        case KEY_F(4):
        case '4':
            current_view = MainView::ExternalRAM;
            menu_selected_idx = 6;
            scroll_offset = 0;
            break;

        case KEY_F(8):
            current_view = MainView::Breakpoints;
            menu_selected_idx = 7;
            break;

        case 'l':
        case 'L':
            current_view = MainView::Log;
            menu_selected_idx = 8;
            break;

        case 'c':
        case 'C':
            center_disassembly_on_pc();
            log_message("Centered disassembly on PC=0x" + to_hex16(cpu.get_pc()));
            break;

        case KEY_PPAGE: // Page Up
            if (scroll_offset >= 16) scroll_offset -= 16;
            else scroll_offset = 0;
            break;

        case KEY_NPAGE: // Page Down
            if (scroll_offset + 16 < 65536) scroll_offset += 16;
            break;

        case ':':
        case '/':
            is_command_mode = true;
            command_buffer.clear();
            break;

        case 'q':
        case 'Q':
            should_exit = true;
            break;
    }
}

void TUI::run() {
    bool should_exit = false;

    while (!should_exit) {
        if (g_terminal_resized || is_term_resized(LINES, COLS)) {
            g_terminal_resized = 0;
            resize_all();
        }

        render_all();

        if (is_running_continuous) {
            wtimeout(stdscr, 20);
            int ch = getch();
            if (ch == KEY_RESIZE || g_terminal_resized) {
                g_terminal_resized = 0;
                resize_all();
            } else if (ch != ERR) {
                is_running_continuous = false;
                log_message("Continuous run stopped by user keypress.");
            } else {
                for (int i = 0; i < 150 && is_running_continuous; ++i) {
                    if (cpu.is_halted()) {
                        is_running_continuous = false;
                        log_message("CPU halted.");
                        break;
                    }
                    cpu.step();
                    if (debugger.has_breakpoint(cpu.get_pc())) {
                        is_running_continuous = false;
                        center_disassembly_on_pc();
                        log_message("Hit breakpoint at 0x" + to_hex16(cpu.get_pc()));
                        break;
                    }
                }
            }
        } else {
            // 50ms timeout ensures regular terminal geometry check even without keystrokes
            wtimeout(stdscr, 50);
            int ch = getch();
            if (ch == KEY_RESIZE || g_terminal_resized) {
                g_terminal_resized = 0;
                resize_all();
            } else if (ch != ERR) {
                handle_input(ch, should_exit);
            }
        }
    }
}
