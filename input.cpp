#include "input.hpp"
#include <iostream>

#ifdef _WIN32
#include <conio.h>
#include <Windows.h>

	KeyPress readKey() {
		int c = _getch();
		if (c == 9)   return { Key::Tab };
		if (c == 13)  return { Key::Enter };
		if (c == 8)   return { Key::Backspace };
		if (c == 224) {
			switch (_getch()) {
			case 72: return { Key::ArrowUp };
			case 80: return { Key::ArrowDown };
			case 75: return { Key::ArrowLeft };
			case 77: return { Key::ArrowRight };
			}
		}
		return { Key::Character, (char)c };
	}

	void enableRawMode() {
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD mode;
		GetConsoleMode(hOut, &mode);
		SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
	void disableRawMode() {
		std::cout << "\033[?25h";  // ensure cursor is visible
		std::cout.flush();
	}

#else
#include <termios.h>
#include <unistd.h>
#include <cstdlib>

	KeyPress readKey() {
		// termios raw mode + read
		char c;
		read(STDIN_FILENO, &c, 1);
		if (c == 9)    return { Key::Tab };
		if (c == 10)   return { Key::Enter };
		if (c == 127)  return { Key::Backspace };
		if (c == '\x1b') {
			char seq[2];
			read(STDIN_FILENO, &seq[0], 1);
			read(STDIN_FILENO, &seq[1], 1);
			if (seq[0] == '[') {
				switch (seq[1]) {
				case 'A': return { Key::ArrowUp };
				case 'B': return { Key::ArrowDown };
				case 'C': return { Key::ArrowRight };
				case 'D': return { Key::ArrowLeft };
				}
			}
		}
		return { Key::Character, c };
	}

	static struct termios original_termios;

	static void restoreTerminal() {
		std::cout << "\033[?25h";
		std::cout.flush();
		tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
	}

	void enableRawMode() {
		tcgetattr(STDIN_FILENO, &original_termios);
		atexit(restoreTerminal);
		struct termios raw = original_termios;
		raw.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	}

	void disableRawMode() {
		tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
		std::cout << "\033[?25h";  // ensure cursor is visible
		std::cout.flush();
	}
#endif