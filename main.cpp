#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <curses.h>
#include <csignal>
#include <unistd.h>
#include <unordered_set>

std::vector<std::string> buffer;
std::vector<std::string> clipboard;
int currentLine = 0;
int currentColumn = 0;
std::string filename;
bool running = true;

enum Mode { NORMAL, INSERT, COMMAND };
Mode currentMode = NORMAL;

std::unordered_set<std::string> keywords;
std::string fileExtension;

void LoadFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }

    buffer.clear();
    std::string line;
    while (std::getline(file, line)) {
        buffer.push_back(line);
    }

    file.close();

    size_t extPos = filename.find_last_of('.');
    fileExtension = (extPos != std::string::npos) ? filename.substr(extPos + 1) : "";

    keywords.clear();
    if (fileExtension == "c" || fileExtension == "cpp" || fileExtension == "h") {
        keywords = {"int", "float", "return", "if", "else", "while", "for", "class", "public", "private", "protected"};
    } else if (fileExtension == "py") {
        keywords = {"def", "return", "if", "else", "elif", "while", "for", "class", "import", "from"};
    } else if (fileExtension == "js") {
        keywords = {"function", "return", "if", "else", "for", "while", "var", "let", "const"};
    } else if (fileExtension == "rb") {
        keywords = {"def", "end", "class", "module", "if", "else", "elsif", "while", "for", "do", "begin", "rescue", "ensure", "yield", "return"};
    } else if (fileExtension == "asm") {
        keywords = {"mov", "add", "sub", "jmp", "cmp", "je", "jne", "jz", "jnz", "push", "pop", "call", "ret", "int"};
    }
}

void SaveFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }

    for (const auto& line : buffer) {
        file << line << std::endl;
    }

    file.close();
}

void DisplayStatus() {
    move(LINES - 1, 0);
    clrtoeol();
    printw("-- %s -- %s %d,%d",
           (currentMode == NORMAL) ? "NORMAL" : (currentMode == INSERT) ? "INSERT" : "COMMAND",
           filename.c_str(), currentLine + 1, currentColumn + 1);
}

void DisplayBuffer() {
    clear();

    int y = 0;
    for (const auto& line : buffer) {
        int x = 0;
        std::string word;
        for (const char& ch : line) {
            if (isalnum(ch) || ch == '_') {
                word += ch;
            } else {
                if (keywords.find(word) != keywords.end()) {
                    attron(COLOR_PAIR(1));
                    mvprintw(y, x - word.size(), "%s", word.c_str());
                    attroff(COLOR_PAIR(1));
                } else {
                    mvprintw(y, x - word.size(), "%s", word.c_str());
                }
                word.clear();
                mvaddch(y, x, ch);
            }
            ++x;
        }
        if (!word.empty() && keywords.find(word) != keywords.end()) {
            attron(COLOR_PAIR(1));
            mvprintw(y, x - word.size(), "%s", word.c_str());
            attroff(COLOR_PAIR(1));
        } else if (!word.empty()) {
            mvprintw(y, x - word.size(), "%s", word.c_str());
        }
        ++y;
    }

    move(currentLine, currentColumn);
    refresh();
}

void MoveCursor(int dx, int dy) {
    int newColumn = currentColumn + dx;
    int newLine = currentLine + dy;

    if (newLine < 0) {
        newLine = 0;
    } else if (newLine >= buffer.size()) {
        newLine = buffer.size() - 1;
    }

    if (newColumn < 0) {
        newColumn = 0;
    } else if (newColumn >= buffer[newLine].size()) {
        newColumn = buffer[newLine].size();
    }

    currentLine = newLine;
    currentColumn = newColumn;
}

void MoveToNextWord() {
    while (currentColumn < buffer[currentLine].size() && isalnum(buffer[currentLine][currentColumn])) {
        currentColumn++;
    }
    while (currentColumn < buffer[currentLine].size() && !isalnum(buffer[currentLine][currentColumn])) {
        currentColumn++;
    }
    if (currentColumn >= buffer[currentLine].size() && currentLine < buffer.size() - 1) {
        currentLine++;
        currentColumn = 0;
    }
}

void MoveToPreviousWord() {
    while (currentColumn > 0 && !isalnum(buffer[currentLine][currentColumn - 1])) {
        currentColumn--;
    }
    while (currentColumn > 0 && isalnum(buffer[currentLine][currentColumn - 1])) {
        currentColumn--;
    }
}

void DeleteToNextWord() {
    if (currentLine < buffer.size()) {
        int startColumn = currentColumn;
        std::string& line = buffer[currentLine];

        MoveToNextWord();
        // Проверка на выход за границы
        if (currentColumn <= line.size()) {
            line.erase(startColumn, currentColumn - startColumn);
            currentColumn = startColumn;
            DisplayBuffer();
        }
    }
}

void ChangeToNextWord() {
    DeleteToNextWord();
    currentMode = INSERT;
    move(LINES - 1, 0);
    clrtoeol();
    printw("-- INSERT --");
}

void CopyLine() {
    clipboard.clear();
    // Проверка на выход за границы
    if (currentLine < buffer.size()) {
        clipboard.push_back(buffer[currentLine]);
    }
}

void CutLine() {
    if (currentLine < buffer.size()) {
        clipboard.clear();
        clipboard.push_back(buffer[currentLine]);
        buffer.erase(buffer.begin() + currentLine);
        if (currentLine >= buffer.size()) {
            currentLine = buffer.size() - 1;
        }
        // Проверка на выход за границы
        if (currentLine < 0) {
            currentLine = 0;
        }
        currentColumn = 0;
        DisplayBuffer();
    }
}

void PasteLineBefore() {
    if (!clipboard.empty()) {
        // Проверка на выход за границы
        if (currentLine >= 0 && currentLine < buffer.size()) {
            buffer.insert(buffer.begin() + currentLine, clipboard[0]);
        } else {
            buffer.insert(buffer.begin(), clipboard[0]);
        }
        currentColumn = 0;
        DisplayBuffer();
    }
}

void PasteLineAfter() {
    if (!clipboard.empty()) {
        if (currentLine + 1 < buffer.size()) {
            buffer.insert(buffer.begin() + currentLine + 1, clipboard[0]);
        } else {
            buffer.push_back(clipboard[0]);
        }
        currentLine++;
        currentColumn = 0;
        DisplayBuffer();
    }
}

void ProcessNormalMode(int ch) {
    switch (ch) {
        case 'h':
            MoveCursor(-1, 0);
            break;
        case 'j':
            MoveCursor(0, 1);
            break;
        case 'k':
            MoveCursor(0, -1);
            break;
        case 'l':
            MoveCursor(1, 0);
            break;
        case 'w':
            MoveToNextWord();
            break;
        case 'b':
            MoveToPreviousWord();
            break;
        case 'x':
            DeleteToNextWord();
            break;
        case 'c':
            ChangeToNextWord();
            break;
        case 'y':
            CopyLine();
            break;
        case 'd':
            CutLine();
            break;
        case 'p':
            PasteLineAfter();
            break;
        case 'P':
            PasteLineBefore();
            break;
        case 'i':
            currentMode = INSERT;
            move(LINES - 1, 0);
            clrtoeol();
            printw("-- INSERT --");
            break;
        case ':':
            currentMode = COMMAND;
            move(LINES - 1, 0);
            clrtoeol();
            printw(":");
            refresh();
            break;
    }
}

void ProcessInsertMode(int ch) {
    switch (ch) {
        case 27: // ESC
            currentMode = NORMAL;
            move(LINES - 1, 0);
            clrtoeol();
            printw("-- NORMAL --");
            break;
        case KEY_BACKSPACE:
        case 127:
            if (currentColumn > 0) {
                buffer[currentLine].erase(--currentColumn, 1);
            }
            break;
        default:
            buffer[currentLine].insert(currentColumn++, 1, ch);
            break;
    }
    DisplayBuffer();
}

void ProcessCommandMode(int ch) {
    static std::string command;
    switch (ch) {
        case 10: // Enter
            if (command == "w") {
                SaveFile(filename);
            } else if (command == "q") {
                running = false;
            }
            command.clear();
            currentMode = NORMAL;
            break;
        case 27: // ESC
            command.clear();
            currentMode = NORMAL;
            break;
        default:
            command += ch;
            break;
    }
    move(LINES - 1, 0);
    clrtoeol();
    printw(":%s", command.c_str());
    refresh();
}

void SignalHandler(int signal) {
    endwin();
    std::cerr << "Interrupt signal (" << signal << ") received.\n";
    exit(signal);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    filename = argv[1];
    LoadFile(filename);

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_BLUE, COLOR_BLACK);

    signal(SIGINT, SignalHandler);

    while (running) {
        DisplayBuffer();
        DisplayStatus();

        int ch = getch();
        switch (currentMode) {
            case NORMAL:
                ProcessNormalMode(ch);
                break;
            case INSERT:
                ProcessInsertMode(ch);
                break;
            case COMMAND:
                ProcessCommandMode(ch);
                break;
        }
    }

    endwin();
    return 0;
}
