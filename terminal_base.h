#ifndef TERMINAL_BASE_H
#define TERMINAL_BASE_H

/*
 * This file contains all the platform specific code regarding terminal input
 * and output. The rest of the code does not have any platform specific
 * details. This file is designed in a way to contain the least number of
 * building blocks, that the rest of the code can use to build all the
 * features.
 */

#include <stdexcept>

#ifdef _WIN32
#    include <conio.h>
#    include <windows.h>
#else
#    include <sys/ioctl.h>
#    include <termios.h>
#    include <unistd.h>
#endif

namespace Term {

/* Note: the code that uses Terminal must be inside try/catch block, otherwise
 * the destructors will not be called when an exception happens and the
 * terminal will not be left in a good state. Terminal uses exceptions when
 * something goes wrong.
 */
class BaseTerminal {
private:
#ifdef _WIN32
    HANDLE hout;
    HANDLE hin;
    DWORD dwOriginalOutMode;
    DWORD dwOriginalInMode;
#else
    struct termios orig_termios;
#endif

public:
    BaseTerminal(bool disable_ctrl_c=true)
    {
#ifdef _WIN32
        hout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hout == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("GetStdHandle(STD_OUTPUT_HANDLE) failed");
        }
        hin = GetStdHandle(STD_INPUT_HANDLE);
        if (hin == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("GetStdHandle(STD_INPUT_HANDLE) failed");
        }

        if (!GetConsoleMode(hout, &dwOriginalOutMode)) {
            throw std::runtime_error("GetConsoleMode() failed");
        }
        if (!GetConsoleMode(hin, &dwOriginalInMode)) {
            throw std::runtime_error("GetConsoleMode() failed");
        }

        DWORD dwRequestedOutModes = ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        DWORD dwRequestedInModes = ENABLE_VIRTUAL_TERMINAL_INPUT;

        DWORD dwOutMode = dwOriginalOutMode | dwRequestedOutModes;
        if (!SetConsoleMode(hout, dwOutMode)) {
            throw std::runtime_error("SetConsoleMode() failed");
        }
        DWORD dwInMode = dwOriginalInMode | dwRequestedInModes;
        dwInMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        if (!SetConsoleMode(hin, dwInMode)) {
            throw std::runtime_error("SetConsoleMode() failed");
        }
#else
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
            throw std::runtime_error("tcgetattr() failed");
        }

        // Put terminal in raw mode
        struct termios raw = orig_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        // This disables output post-processing, requiring explicit \r\n. We
        // keep it enabled, so that in C++, one can still just use std::endl
        // for EOL instead of "\r\n".
        //raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
        if (disable_ctrl_c) {
            raw.c_lflag &= ~(ISIG);
        }
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
            throw std::runtime_error("tcsetattr() failed");
        }
#endif
    }

    virtual ~BaseTerminal() noexcept(false)
    {
#ifdef _WIN32
        if (!SetConsoleMode(hout, dwOriginalOutMode)) {
            throw std::runtime_error("SetConsoleMode() failed in destructor");
        }
        if (!SetConsoleMode(hin, dwOriginalInMode)) {
            throw std::runtime_error("SetConsoleMode() failed in destructor");
        }
#else
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
            throw std::runtime_error("tcsetattr() failed in destructor");
        }
#endif
    }

    // Returns true if a character is read, otherwise immediately returns false
    bool read_raw(char* s) const
    {
#ifdef _WIN32
        char buf[1];
        DWORD nread;
        if (kbhit()) {
            if (!ReadFile(hin, buf, 1, &nread, nullptr)) {
                throw std::runtime_error("ReadFile() failed");
            }
            if (nread == 1) {
                *s = buf[0];
                return true;
            } else {
                throw std::runtime_error("kbhit() and ReadFile() inconsistent");
            }
        } else {
            return false;
        }
#else
        int nread = read(STDIN_FILENO, s, 1);
        if (nread == -1 && errno != EAGAIN) {
            throw std::runtime_error("read() failed");
        }
        return (nread == 1);
#endif
    }

    void get_term_size(int& rows, int& cols) const
    {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO inf;
        GetConsoleScreenBufferInfo(hout, &inf);
        cols = inf.srWindow.Right - inf.srWindow.Left + 1;
        rows = inf.srWindow.Bottom - inf.srWindow.Top + 1;
#else
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
            throw std::runtime_error("ioctl() failed");
        } else {
            cols = ws.ws_col;
            rows = ws.ws_row;
        }
#endif
    }
};

} // namespace Term

#endif // TERMINAL_BASE_H