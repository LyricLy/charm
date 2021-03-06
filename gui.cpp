/*
 * gui.cpp
 *
 *  Created on: Apr 14, 2018
 *      Author: iconmaster
 */

#include "gui.h"
#include "Debug.h"
#include "PredefinedFunctions.h"

#include <readline/readline.h>
#include <readline/history.h>
#include <ncurses.h>

// constants
#define CONTROL_C 3
#define CONTROL_L 12

#define STACK_LEFT_MARGIN 4

// global variables
Parser* parser;
Runner* runner;

static WINDOW* stack_win;
static WINDOW* readline_win;

static int last_char;
static bool have_input;

static bool had_output = false;
static std::string accumulated_output;

// private functions
static void init_stack_win() {
	int w, h;
	getmaxyx(stack_win, h, w);

	for (int i = 0; i < h; i++) {
		mvwprintw(stack_win, i, 0, "%d:", h-i-1);
		wclrtoeol(stack_win);
	}
}

static void update_stack_win() {
	int w, h;
	getmaxyx(stack_win, h, w);

	int depth = runner->getCurrentStack()->stack.size();

	for (int i = 0; i < h; i++) {
		int stack_index = depth - (h-i-1) - 1;

		wmove(stack_win, i, STACK_LEFT_MARGIN);
		wclrtoeol(stack_win);

		if (stack_index >= 0 && stack_index < (int) runner->getCurrentStack()->stack.size()) {
			// TODO: handle long lines of input
			mvwprintw(stack_win, i, STACK_LEFT_MARGIN, "%s", charmFunctionToString(runner->getCurrentStack()->stack[stack_index]).c_str());
		}
	}

	wrefresh(stack_win);
}

static void display_error(const char* what) {
	int curs = curs_set(0);

	update_stack_win();
	WINDOW* error_win = derwin(stack_win, 4, COLS-4, LINES/2-2, 2);

	werase(error_win);
	box(error_win, 0, 0);
	mvwprintw(error_win, 1, 1, "ERROR:");
	mvwprintw(error_win, 2, 1, "%s", what);

	touchwin(error_win); wrefresh(error_win);
	wgetch(error_win);
	werase(error_win); delwin(error_win);
	init_stack_win(); update_stack_win();

	curs_set(curs);
}

static int readline_getc(FILE* dummy) {
	have_input = false;
	return last_char;
}

static int readline_input_available() {
	return have_input;
}

static void readline_redisplay() {
	// TODO: handle tabs and other characters with width > 1
	// TODO: handle input going off the edge of the screen
	werase(readline_win);

	mvwprintw(readline_win, 0, 0, "%s%s", rl_display_prompt, rl_line_buffer);
	wmove(readline_win, 0, strlen(rl_display_prompt) + rl_point);
}

static void readline_callback_handler(char* line) {
	add_history(line);
	werase(readline_win);

	try {
		runner->run(parser->lex(std::string(line)));

		if (had_output) {
			// display accumulated output
			werase(stack_win);
			mvwprintw(stack_win, 0, 0, "%s", accumulated_output.c_str());
			wrefresh(stack_win);

			werase(readline_win);
			mvwprintw(readline_win, 0, 0, "Press any key to continue...");
			wrefresh(readline_win);

			wgetch(readline_win);
			init_stack_win(); update_stack_win();

			accumulated_output = ""; had_output = false;
		}
	} catch (const std::runtime_error& e) {
		display_error(e.what());
	}

	update_stack_win();
}

static char* get_input_line_result;
static void get_input_line_callback_handler(char* line) {
	get_input_line_result = line;
}

static void set_prompt() {
	std::string stack_name = charmFunctionToString(runner->getCurrentStack()->name);
	std::string prompt = stack_name + "> ";
	rl_set_prompt(prompt.c_str());

	readline_redisplay();
}

static void exit_gui(int rc) {
	endwin();
	exit(rc);
}

static char* function_name_generator(const char* line, int state) {
	// TODO: we currently only pick the first result we find. But multimatches break Curses.
    if (state || strlen(line) == 0) return nullptr;

    int len = strlen(line);

    // match with functions defined in this runner
    auto fds = runner->functionDefinitions;
	for (const auto& fd : fds) {
		const char* name = fd.second.functionName.c_str();
		if (strncmp(name, line, len) == 0) {
			return strdup(name);
		}
	}

	// match predefined functions
	auto predefs = runner->pF->cppFunctionNames;
    for (const auto& entry : predefs) {
		const char* name = entry.first.c_str();
		if (strncmp(name, line, len) == 0) {
			return strdup(name);
		}
    }

	// no matches; return null
    return nullptr;
}

static char** function_name_completion(const char * line, int start, int end) {
    rl_attempted_completion_over = 1;
    return rl_completion_matches(line, function_name_generator);
}

// public interface
void display_output(std::string output) {
	had_output = true;
	accumulated_output += output;
}

std::string get_input_line() {
	rl_callback_handler_install("GETLINE> ", get_input_line_callback_handler);
	get_input_line_result = nullptr;

	while (get_input_line_result == nullptr) {
    	last_char = wgetch(readline_win);
    	have_input = true;
    	rl_callback_read_char();
	}

	rl_callback_handler_install("", readline_callback_handler);
	return std::string(get_input_line_result);
}

void charm_gui_init(Parser _parser, Runner _runner) {
	parser = &_parser;
	runner = &_runner;

	// initialize curses
	initscr();
	raw(); // we want to capture all characters (but NOT via keypad; readline handles that for us)
	noecho(); // headline handles echoing
	nonl(); // so we can actually process ^L

    if (has_colors()) {
    	start_color();
		use_default_colors();
    }

    stack_win = newwin(LINES-1, COLS, 0, 0);
    init_stack_win(); update_stack_win();

    readline_win = newwin(1, COLS, LINES-1, 0);

    // initialize readline
    rl_catch_signals = 0; // don't catch signals; Curses handles those
    rl_catch_sigwinch = 0;
    rl_deprep_term_function = NULL; // don't handle terminal i/o; Curses also handles that
    rl_prep_term_function = NULL;
    rl_change_environment = 0; // readline will overwrite LINES and COLS if you don't do this!

    // register readline callbacks
    rl_getc_function = readline_getc;
    rl_input_available_hook = readline_input_available;
    rl_redisplay_function = readline_redisplay;
    rl_attempted_completion_function = function_name_completion;
    rl_callback_handler_install("", readline_callback_handler);

    // do the main GUI loop
    while (true) {
    	set_prompt();
    	int c = wgetch(readline_win);

    	switch (c) {
    	case -1:
    	case CONTROL_C:
    		// ^C or EOF: quit
    		exit_gui(0);
    		break;
    	case KEY_RESIZE:
    		// we got a SIGWINCH; resize the GUI
    		wresize(stack_win, LINES-1, COLS); mvwin(stack_win, 0, 0);
    		wresize(readline_win, 1, COLS); mvwin(readline_win, LINES-1, 0);
    		// no break on purpose, so we refresh the screen too
    	case CONTROL_L:
    		// ^L: refresh the screen
    		werase(stack_win); werase(readline_win);
    		init_stack_win();
    		update_stack_win();
    		readline_redisplay();
    		wrefresh(stack_win); wrefresh(readline_win);
    		break;
    	default:
    		// let readline handle it
        	last_char = c;
        	have_input = true;
        	rl_callback_read_char();
    	}
    }
}
