#include "tui.hpp"
#include "disassembler.hpp"
#include "sfr.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
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
      offset_y(0), offset_x(0),
      win_title(nullptr), win_menu(nullptr), win_main(nullptr), win_status(nullptr), win_cmd(nullptr) {

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    init_colors();
    create_windows();

    center_disassembly_on_pc();
    log_message("8051 Emulator ready. Space/F7:Step | F5:Run | F1-F4:Views | ':' Command");
}

TUI::~TUI() {
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
    // Center the fixed 80x24 box within current terminal dimensions
    offset_y = (LINES > UI_HEIGHT) ? (LINES - UI_HEIGHT) / 2 : 0;
    offset_x = (COLS > UI_WIDTH) ? (COLS - UI_WIDTH) / 2 : 0;

    // Create fixed-size windows: Total 24 rows, 80 cols
    win_title  = newwin(1, UI_WIDTH, offset_y, offset_x);
    win_menu   = newwin(MID_HEIGHT, MENU_WIDTH, offset_y + 1, offset_x);
    win_main   = newwin(MID_HEIGHT, MAIN_WIDTH, offset_y + 1, offset_x + MENU_WIDTH);
    win_status = newwin(1, UI_WIDTH, offset_y + 1 + MID_HEIGHT, offset_x);
    win_cmd    = newwin(1, UI_WIDTH, offset_y + 2 + MID_HEIGHT, offset_x);

    keypad(win_cmd, TRUE);
    keypad(win_main, TRUE);
    keypad(win_menu, TRUE);
}

void TUI::destroy_windows() {
    if (win_title)  { delwin(win_title);  win_title = nullptr; }
    if (win_menu)   { delwin(win_menu);   win_menu = nullptr; }
    if (win_main)   { delwin(win_main);   win_main = nullptr; }
    if (win_status) { delwin(win_status); win_status = nullptr; }
    if (win_cmd)    { delwin(win_cmd);    win_cmd = nullptr; }
}

void TUI::render_backdrop() {
    werase(stdscr);
    if (LINES > UI_HEIGHT || COLS > UI_WIDTH) {
        if (offset_y >= 2) {
            std::string banner = "Intel 8051 Microprocessor Emulator";
            int x = std::max(0, (COLS - static_cast<int>(banner.length())) / 2);
            wattron(stdscr, A_BOLD | COLOR_PAIR(PAIR_HEADER));
            mvwprintw(stdscr, offset_y - 2, x, "%s", banner.c_str());
            wattroff(stdscr, A_BOLD | COLOR_PAIR(PAIR_HEADER));
        }
        if (offset_y + UI_HEIGHT + 1 < LINES) {
            std::string hint = "Press 'q' or ':q' to exit | Space:Step | F5:Run";
            int x = std::max(0, (COLS - static_cast<int>(hint.length())) / 2);
            mvwprintw(stdscr, offset_y + UI_HEIGHT + 1, x, "%s", hint.c_str());
        }
    }
}

void TUI::resize_all() {
    // 1. Tell curses about new terminal dimensions
    resizeterm(LINES, COLS);

    // 2. Recompute centering offsets
    offset_y = (LINES > UI_HEIGHT) ? (LINES - UI_HEIGHT) / 2 : 0;
    offset_x = (COLS > UI_WIDTH) ? (COLS - UI_WIDTH) / 2 : 0;

    // 3. Move existing fixed-size windows to new center position (instant, zero memory reallocation)
    mvwin(win_title,  offset_y, offset_x);
    mvwin(win_menu,   offset_y + 1, offset_x);
    mvwin(win_main,   offset_y + 1, offset_x + MENU_WIDTH);
    mvwin(win_status, offset_y + 1 + MID_HEIGHT, offset_x);
    mvwin(win_cmd,    offset_y + 2 + MID_HEIGHT, offset_x);

    // 4. Redraw surrounding backdrop and refresh
    render_backdrop();
    wnoutrefresh(stdscr);

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
    scroll_offset = (cpu.get_pc() >= 4) ? (cpu.get_pc() - 4) : 0;
}

void TUI::render_title() {
    if (!win_title) return;
    werase(win_title);
    wbkgd(win_title, COLOR_PAIR(PAIR_TITLE));

    std::string state_str = cpu.is_halted() ? "HALTED" : (is_running_continuous ? "RUNNING" : "PAUSED");
    std::string title = " 8051 MICROPROCESSOR EMULATOR ";
    std::string info = "[ROM: " + filename + "]  " + state_str + "  Cycles:" + std::to_string(cpu.get_cycles()) + " ";

    mvwprintw(win_title, 0, 0, "%s", title.c_str());
    if (static_cast<int>(title.length() + info.length()) < UI_WIDTH) {
        mvwprintw(win_title, 0, UI_WIDTH - static_cast<int>(info.length()) - 1, "%s", info.c_str());
    }
    wnoutrefresh(win_title);
}

void TUI::render_menu() {
    if (!win_menu) return;
    werase(win_menu);
    box(win_menu, 0, 0);

    wattron(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_menu, 0, 2, "[ MENU ]");
    wattroff(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    struct MenuOption {
        std::string label;
        std::string key;
    };

    const std::vector<MenuOption> options = {
        {"Step (1)",       "F7/Spc"},
        {"Run / Pause",    "F5/r"},
        {"Reset CPU",      "F9"},
        {"Disasm View",    "F1"},
        {"RAM Dump",       "F2"},
        {"SFR Dump",       "F3"},
        {"XDATA Dump",     "F4"},
        {"Breakpoints",    "F8"},
        {"Execution Log",  "L"},
        {"Quit",           "q"}
    };

    int y = 1;
    for (size_t i = 0; i < options.size(); ++i, ++y) {
        bool selected = (static_cast<int>(i) == menu_selected_idx);

        if (selected) {
            wattron(win_menu, COLOR_PAIR(PAIR_MENU_SEL) | A_REVERSE | A_BOLD);
        }

        std::string line = (selected ? ">" : " ") + options[i].label;
        std::string key_str = options[i].key;

        mvwprintw(win_menu, y, 1, "%-*s", MENU_WIDTH - 2, "");
        mvwprintw(win_menu, y, 1, "%s", line.c_str());
        mvwprintw(win_menu, y, MENU_WIDTH - 1 - key_str.length(), "%s", key_str.c_str());

        if (selected) {
            wattroff(win_menu, COLOR_PAIR(PAIR_MENU_SEL) | A_REVERSE | A_BOLD);
        }
    }

    // Divider
    mvwhline(win_menu, y, 1, ACS_HLINE, MENU_WIDTH - 2);
    wattron(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_menu, y, 2, "[ REGISTERS ]");
    wattroff(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    y++;

    mvwprintw(win_menu, y++, 2, "PC: 0x%04X  SP:0x%02X", cpu.get_pc(), cpu.get_sp());
    mvwprintw(win_menu, y++, 2, "DPTR: 0x%04X", cpu.get_dptr());
    mvwprintw(win_menu, y++, 2, "ACC: 0x%02X   B:0x%02X", cpu.get_acc(), cpu.get_b());
    mvwprintw(win_menu, y++, 2, "PSW: 0x%02X   [Bank %d]", cpu.get_psw(), cpu.get_active_bank());
    mvwprintw(win_menu, y++, 2, "CY:%d AC:%d OV:%d P:%d", cpu.get_cy(), cpu.get_ac(), cpu.get_ov(), cpu.get_p());

    mvwhline(win_menu, y, 1, ACS_HLINE, MENU_WIDTH - 2);
    wattron(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_menu, y, 2, "[ R0 - R7 ]");
    wattroff(win_menu, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    y++;

    mvwprintw(win_menu, y++, 2, "R0:0x%02X  R1:0x%02X", cpu.get_rn(0), cpu.get_rn(1));
    mvwprintw(win_menu, y++, 2, "R2:0x%02X  R3:0x%02X", cpu.get_rn(2), cpu.get_rn(3));

    wnoutrefresh(win_menu);
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

    wnoutrefresh(win_main);
}

void TUI::render_disassembly() {
    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ DISASSEMBLY / CODE VIEW ] (PgUp/Dn:Scroll, c:PC)");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    uint16_t curr_addr = scroll_offset;
    for (int y = 1; y < MID_HEIGHT - 1; ++y) {
        DisassemblyResult res = Disassembler::disassemble(memory, curr_addr);
        bool is_pc = (curr_addr == cpu.get_pc());
        bool is_bp = debugger.has_breakpoint(curr_addr);

        if (is_pc) {
            wattron(win_main, COLOR_PAIR(PAIR_PC_LINE) | A_BOLD);
        }

        mvwprintw(win_main, y, 1, "%-*s", MAIN_WIDTH - 2, "");

        if (is_bp) {
            wattron(win_main, COLOR_PAIR(PAIR_BP) | A_BOLD);
            mvwprintw(win_main, y, 1, "[*]");
            wattroff(win_main, COLOR_PAIR(PAIR_BP) | A_BOLD);
        } else {
            mvwprintw(win_main, y, 1, "   ");
        }

        mvwprintw(win_main, y, 4, "%s %04X: %-8s %-5s %-15s",
                  (is_pc ? "=>" : "  "),
                  res.address,
                  res.bytes_hex.c_str(),
                  res.mnemonic.c_str(),
                  res.operands.c_str());

        mvwprintw(win_main, y, MAIN_WIDTH - 10, "[%d cyc]", res.cycles);

        if (is_pc) {
            wattroff(win_main, COLOR_PAIR(PAIR_PC_LINE) | A_BOLD);
        }

        curr_addr += res.length;
    }
}

void TUI::render_internal_ram() {
    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ INTERNAL RAM (0x00-0x7F / 0x80-0xFF) ]");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    wattron(win_main, COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 1, 2, "Addr 00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F");
    wattroff(win_main, COLOR_PAIR(PAIR_HEADER));

    for (int row = 0; row < 16 && (2 + row) < MID_HEIGHT - 1; ++row) {
        int y = 2 + row;
        uint8_t base_addr = row * 16;

        mvwprintw(win_main, y, 2, "%02X: ", base_addr);

        for (int col = 0; col < 16; ++col) {
            uint8_t addr = base_addr + col;
            uint8_t val = (addr < 0x80) ? memory.read_direct(addr) : memory.read_indirect(addr);

            int x = 6 + (col * 3) + (col >= 8 ? 1 : 0);

            if (val != 0) wattron(win_main, COLOR_PAIR(PAIR_NONZERO) | A_BOLD);
            mvwprintw(win_main, y, x, "%02X", val);
            if (val != 0) wattroff(win_main, COLOR_PAIR(PAIR_NONZERO) | A_BOLD);
        }
    }
}

void TUI::render_sfr() {
    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ SPECIAL FUNCTION REGISTERS ]");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    struct SfrInfo {
        uint8_t addr;
        std::string name;
    };

    const std::vector<SfrInfo> sfrs = {
        {sfr::P0,   "P0"},   {sfr::SP,   "SP"},   {sfr::DPL,  "DPL"},
        {sfr::DPH,  "DPH"},  {sfr::PCON, "PCON"}, {sfr::TCON, "TCON"},
        {sfr::TMOD, "TMOD"}, {sfr::TL0,  "TL0"},  {sfr::TL1,  "TL1"},
        {sfr::TH0,  "TH0"},  {sfr::TH1,  "TH1"},  {sfr::P1,   "P1"},
        {sfr::SCON, "SCON"}, {sfr::SBUF, "SBUF"}, {sfr::P2,   "P2"},
        {sfr::IE,   "IE"},   {sfr::P3,   "P3"},   {sfr::IP,   "IP"},
        {sfr::PSW,  "PSW"},  {sfr::ACC,  "ACC"},  {sfr::B,    "B"}
    };

    wattron(win_main, COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 1, 2, "Addr Name Hex  Binary     Addr Name Hex  Binary");
    wattroff(win_main, COLOR_PAIR(PAIR_HEADER));

    size_t half = (sfrs.size() + 1) / 2;
    for (size_t i = 0; i < half; ++i) {
        int y = 2 + i;
        if (y >= MID_HEIGHT - 1) break;

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
}

void TUI::render_external_ram() {
    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ EXTERNAL RAM (XDATA) - PgUp/PgDn ]");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    wattron(win_main, COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 1, 2, "Addr 00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F");
    wattroff(win_main, COLOR_PAIR(PAIR_HEADER));

    for (int row = 0; row < MID_HEIGHT - 3; ++row) {
        int y = 2 + row;
        uint32_t base = scroll_offset + (row * 16);
        if (base >= 65536) break;

        mvwprintw(win_main, y, 2, "%04X:", base);

        for (int col = 0; col < 16; ++col) {
            uint32_t addr = base + col;
            uint8_t val = (addr < 65536) ? memory.read_xdata(addr) : 0;
            int x = 8 + (col * 3) + (col >= 8 ? 1 : 0);

            if (val != 0) wattron(win_main, COLOR_PAIR(PAIR_NONZERO) | A_BOLD);
            mvwprintw(win_main, y, x, "%02X", val);
            if (val != 0) wattroff(win_main, COLOR_PAIR(PAIR_NONZERO) | A_BOLD);
        }
    }
}

void TUI::render_breakpoints() {
    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ ACTIVE BREAKPOINTS ]");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    const auto& bps = debugger.get_breakpoints();
    if (bps.empty()) {
        mvwprintw(win_main, 3, 4, "No breakpoints set.");
        mvwprintw(win_main, 5, 4, "Type ':b <addr>' at bottom to add a breakpoint.");
        return;
    }

    int y = 2;
    for (uint16_t bp : bps) {
        if (y >= MID_HEIGHT - 1) break;
        DisassemblyResult res = Disassembler::disassemble(memory, bp);
        wattron(win_main, COLOR_PAIR(PAIR_BP) | A_BOLD);
        mvwprintw(win_main, y, 3, "[*]");
        wattroff(win_main, COLOR_PAIR(PAIR_BP) | A_BOLD);

        mvwprintw(win_main, y, 7, "0x%04X: %-8s %-5s %-15s",
                  bp, res.bytes_hex.c_str(), res.mnemonic.c_str(), res.operands.c_str());
        y++;
    }
}

void TUI::render_log() {
    wattron(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));
    mvwprintw(win_main, 0, 2, "[ EXECUTION LOG & MESSAGES ]");
    wattroff(win_main, A_BOLD | COLOR_PAIR(PAIR_HEADER));

    int max_lines = MID_HEIGHT - 2;
    int start_idx = static_cast<int>(message_log.size()) - max_lines;
    if (start_idx < 0) start_idx = 0;

    int y = 1;
    for (size_t i = start_idx; i < message_log.size() && y < MID_HEIGHT - 1; ++i, ++y) {
        std::string line = message_log[i];
        if (static_cast<int>(line.length()) > MAIN_WIDTH - 4) {
            line = line.substr(0, MAIN_WIDTH - 4);
        }
        mvwprintw(win_main, y, 2, "%s", line.c_str());
    }
}

void TUI::render_status() {
    if (!win_status) return;
    werase(win_status);
    wbkgd(win_status, COLOR_PAIR(PAIR_STATUS));

    DisassemblyResult cur = Disassembler::disassemble(memory, cpu.get_pc());
    std::string line = " [STATUS] PC:0x" + to_hex16(cpu.get_pc()) +
                       " | Next: " + cur.mnemonic + " " + cur.operands +
                       " | " + status_message;

    if (static_cast<int>(line.length()) > UI_WIDTH - 2) {
        line = line.substr(0, UI_WIDTH - 2);
    }
    mvwprintw(win_status, 0, 0, "%s", line.c_str());
    wnoutrefresh(win_status);
}

void TUI::render_cmd() {
    if (!win_cmd) return;
    werase(win_cmd);

    if (is_command_mode) {
        curs_set(1);
        wattron(win_cmd, COLOR_PAIR(PAIR_CMD) | A_BOLD);
        std::string disp = ":" + command_buffer;
        if (static_cast<int>(disp.length()) > UI_WIDTH - 2) {
            disp = disp.substr(disp.length() - (UI_WIDTH - 2));
        }
        mvwprintw(win_cmd, 0, 0, "%s", disp.c_str());
        wattroff(win_cmd, COLOR_PAIR(PAIR_CMD) | A_BOLD);
        wmove(win_cmd, 0, std::min(UI_WIDTH - 2, static_cast<int>(disp.length())));
    } else {
        curs_set(0);
        std::string tip = " [CMD] Press ':' for command | Space:Step | F5:Run | F1-F4:Views | q:Quit";
        mvwprintw(win_cmd, 0, 0, "%.*s", UI_WIDTH - 2, tip.c_str());
    }
    wnoutrefresh(win_cmd);
}

void TUI::render_all() {
    render_title();
    render_menu();
    render_main();
    render_status();
    render_cmd();
    doupdate();
}

void TUI::execute_menu_action(int idx) {
    switch (idx) {
        case 0: // Step
            if (!cpu.is_halted()) {
                uint8_t c = cpu.step();
                center_disassembly_on_pc();
                log_message("Stepped (" + std::to_string(c) + " cycles).");
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
            log_message("CPU reset.");
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
                log_message("Breakpoint at 0x" + to_hex16(cpu.get_pc()));
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
            log_message("Centered on PC=0x" + to_hex16(cpu.get_pc()));
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

    // Initial render with backdrop
    render_backdrop();
    wnoutrefresh(stdscr);
    render_all();

    while (!should_exit) {
        WINDOW* input_win = win_cmd ? win_cmd : win_main;
        if (!input_win) {
            usleep(50000);
            continue;
        }

        if (is_running_continuous) {
            wtimeout(input_win, 25);
            int ch = wgetch(input_win);
            if (ch == KEY_RESIZE) {
                resize_all();
            } else if (ch != ERR) {
                is_running_continuous = false;
                log_message("Continuous run paused.");
                render_all();
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
                render_all();
            }
        } else {
            wtimeout(input_win, -1);
            int ch = wgetch(input_win);
            if (ch == KEY_RESIZE) {
                resize_all();
            } else if (ch != ERR) {
                handle_input(ch, should_exit);
                if (!should_exit) {
                    render_all();
                }
            }
        }
    }
}
