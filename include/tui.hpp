#pragma once
#include "cpu.hpp"
#include "memory.hpp"
#include "debugger.hpp"
#include <string>
#include <vector>
#include <deque>
#include <ncurses.h>

enum class MainView {
    Disassembly = 0,
    InternalRAM = 1,
    SFR = 2,
    ExternalRAM = 3,
    Breakpoints = 4,
    Log = 5
};

class TUI {
public:
    static constexpr int UI_WIDTH = 80;
    static constexpr int UI_HEIGHT = 24;
    static constexpr int MENU_WIDTH = 24;
    static constexpr int MAIN_WIDTH = UI_WIDTH - MENU_WIDTH; // 56
    static constexpr int MID_HEIGHT = UI_HEIGHT - 3;         // 21

    TUI(CPU& cpu, Memory& mem, Debugger& dbg, const std::string& loaded_filename = "");
    ~TUI();

    // Runs the main ncurses event loop
    void run();

    // Add a message to the status bar and log history
    void log_message(const std::string& msg);

private:
    CPU& cpu;
    Memory& memory;
    Debugger& debugger;
    std::string filename;

    // View & navigation state
    MainView current_view;
    int menu_selected_idx;
    uint16_t scroll_offset;
    bool is_command_mode;
    bool is_running_continuous;
    std::string command_buffer;
    std::deque<std::string> message_log;
    std::string status_message;

    // Center offsets
    int offset_y;
    int offset_x;

    // Window pointers
    WINDOW* win_title;
    WINDOW* win_menu;
    WINDOW* win_main;
    WINDOW* win_status;
    WINDOW* win_cmd;

    // Initialization & Lifecycle
    void init_colors();
    void create_windows();
    void destroy_windows();
    void resize_all();
    void render_backdrop();

    // Render methods
    void render_all();
    void render_title();
    void render_menu();
    void render_main();
    void render_status();
    void render_cmd();

    // Main window sub-views
    void render_disassembly();
    void render_internal_ram();
    void render_sfr();
    void render_external_ram();
    void render_breakpoints();
    void render_log();

    // Input & Command dispatch
    void handle_input(int ch, bool& should_exit);
    void execute_command(const std::string& cmd);
    void execute_menu_action(int idx);
    void center_disassembly_on_pc();
};
