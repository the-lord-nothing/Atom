#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <curses.h>
#include <csignal>
#include <unistd.h>
#include <stack>
#include <unordered_set>
#include <regex>

std::vector<std::string> buffer;
std::stack<std::pair<int, std::string>> undoStack;
std::stack<std::pair<int, std::string>> redoStack;
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
                    mvprintw(y, x - word.size(), word.c_str());
                    attroff(COLOR_PAIR(1));
                } else {
                    mvprintw(y, x - word.size(), word.c_str());
                }
                word.clear();
                mvaddch(y, x, ch);
            }
            ++x;
        }
        if (!word.empty() && keywords.find(word) != keywords.end()) {
            attron(COLOR_PAIR(1));
            mvprintw(y, x - word.size(), word.c_str());
            attroff(COLOR_PAIR(1));
        } else if (!word.empty()) {
            mvprintw(y, x - word.size(), word.c_str());
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
        undoStack.push({currentLine, line});
        line.erase(startColumn, currentColumn - startColumn);
        currentColumn = startColumn;
        DisplayBuffer();
    }
}

void ChangeToNextWord() {
    DeleteToNextWord();
    currentMode = INSERT;
    move(LINES - 1, 0);
    clrtoeol();
    printw("-- INSERT --");
}

void Undo() {
    if (!undoStack.empty()) {
        auto action = undoStack.top();
        undoStack.pop();

        int line = action.first;
        std::string& text = action.second;

        redoStack.push({line, buffer[line]});
        buffer[line] = text;
        DisplayBuffer();
    }
}

void Redo() {
    if (!redoStack.empty()) {
        auto action = redoStack.top();
        redoStack.pop();

        int line = action.first;
        std::string& text = action.second;

        undoStack.push({line, buffer[line]});
        buffer[line] = text;
        DisplayBuffer();
    }
}

void CopyLine() {
    clipboard.clear();
    clipboard.push_back(buffer[currentLine]);
}

void CutLine() {
    if (currentLine < buffer.size()) {
        undoStack.push({currentLine, buffer[currentLine]});
        clipboard.clear();
        clipboard.push_back(buffer[currentLine]);
        buffer.erase(buffer.begin() + currentLine);
        if (currentLine >= buffer.size()) {
            currentLine = buffer.size() - 1;
        }
        DisplayBuffer();
    }
}

void PasteLineAfter() {
    if (!clipboard.empty()) {
        undoStack.push({currentLine + 1, buffer[currentLine]});
        buffer.insert(buffer.begin() + currentLine + 1, clipboard.begin(), clipboard.end());
        ++currentLine;
        DisplayBuffer();
    }
}

void PasteLineBefore() {
    if (!clipboard.empty()) {
        undoStack.push({currentLine, buffer[currentLine]});
        buffer.insert(buffer.begin() + currentLine, clipboard.begin(), clipboard.end());
        DisplayBuffer();
    }
}

void Replace() {
    echo();
    curs_set(1);

    mvprintw(LINES - 1, 0, ":s/");
    char findQuery[256];
    getnstr(findQuery, 255);

    mvprintw(LINES - 1, 0, ":s/%s/", findQuery);
    char replaceQuery[256];
    getnstr(replaceQuery, 255);

    noecho();
    curs_set(0);

    for (auto& line : buffer) {
        size_t pos = 0;
        while ((pos = line.find(findQuery, pos)) != std::string::npos) {
            line.replace(pos, strlen(findQuery), replaceQuery);
            pos += strlen(replaceQuery);
        }
    }

    DisplayBuffer();
}

    DisplayBuffer();
}

void Search() {
    echo();
    curs_set(1);

    mvprintw(LINES - 1, 0, "/");
    char query[256];
    getnstr(query, 255);
    std::string searchQuery = query;
    int searchPos = -1;

    noecho();
    curs_set(0);

    for (int i = currentLine; i < buffer.size(); ++i) {
        size_t pos = buffer[i].find(searchQuery, (i == currentLine && searchPos != -1) ? searchPos + 1 : 0);
        if (pos != std::string::npos) {
            currentLine = i;
            currentColumn = pos;
            DisplayBuffer();
            return;
        }
    }

    move(LINES - 1, 0);
    clrtoeol();
    printw("Pattern not found: %s", searchQuery.c_str());
    refresh();
}

void NextMatch() {
    if (lastSearchQuery.empty()) {
        move(LINES - 1, 0);
        clrtoeol();
        printw("No previous search query");
        refresh();
        return;
    }

    for (int i = currentLine; i < buffer.size(); ++i) {
        size_t startPos = (i == currentLine) ? currentColumn + 1 : 0;
        size_t pos = buffer[i].find(lastSearchQuery, startPos);
        if (pos != std::string::npos) {
            currentLine = i;
            currentColumn = pos;
            DisplayBuffer();
            return;
        }
    }

    move(LINES - 1, 0);
    clrtoeol();
    printw("Pattern not found: %s", lastSearchQuery.c_str());
    refresh();
}

void PreviousMatch() {
    if (lastSearchQuery.empty()) {
        move(LINES - 1, 0);
        clrtoeol();
        printw("No previous search query");
        refresh();
        return;
    }

    for (int i = currentLine; i >= 0; --i) {
        size_t startPos = (i == currentLine) ? currentColumn - 1 : buffer[i].size() - 1;
        if (startPos == std::string::npos) continue;

        size_t pos = buffer[i].rfind(lastSearchQuery, startPos);
        if (pos != std::string::npos) {
            currentLine = i;
            currentColumn = pos;
            DisplayBuffer();
            return;
        }
    }

    move(LINES - 1, 0);
    clrtoeol();
    printw("Pattern not found: %s", lastSearchQuery.c_str());
    refresh();
}

void ProcessCommand(const std::string& command) {
    if (command == "w") {
        SaveFile(filename);
        move(LINES - 1, 0);
        clrtoeol();
        printw("File saved");
    } else if (command == "q") {
        endwin();
        exit(0);
    } else if (command == "u") {
        Undo();
    } else if (command == "r") {
        Redo();
    } else if (command == "/") {
        Search();
    } else if (command == "n") {
        NextMatch();
    } else if (command == "N") {
        PreviousMatch();
    } else if (command.substr(0, 2) == "s/") {
        Replace();
    } else if (command.substr(0, 2) == "e ") {
        filename = command.substr(2);
        LoadFile(filename);
        DisplayBuffer();
    }
}

void Search() {
    echo();
    curs_set(1);

    mvprintw(LINES - 1, 0, "/");
    char query[256];
    getnstr(query, 255);
    lastSearchQuery = query;  // Save the search query
    int searchPos = -1;

    noecho();
    curs_set(0);

    for (int i = currentLine; i < buffer.size(); ++i) {
        size_t pos = buffer[i].find(lastSearchQuery, (i == currentLine && searchPos != -1) ? searchPos + 1 : 0);
        if (pos != std::string::npos) {
            currentLine = i;
            currentColumn = pos;
            DisplayBuffer();
            return;
        }
    }

    move(LINES - 1, 0);
    clrtoeol();
    printw("Pattern not found: %s", lastSearchQuery.c_str());
    refresh();
}

void ProcessInput() {
    std::string commandBuffer;
    int ch;

    while (running) {
        ch = getch();

        if (currentMode == NORMAL) {
            switch (ch) {
                case 'i':
                    currentMode = INSERT;
                    move(LINES - 1, 0);
                    clrtoeol();
                    printw("-- INSERT --");
                    move(currentLine, currentColumn);
                    break;
                case ':':
                    currentMode = COMMAND;
                    move(LINES - 1, 0);
                    clrtoeol();
                    printw(":");
                    commandBuffer.clear();
                    break;
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
                case 'd':
                    ch = getch();
                    if (ch == 'w') {
                        DeleteToNextWord();
                    } else if (ch == 'd') {
                        CutLine();
                    }
                    break;
                case 'c':
                    ch = getch();
                    if (ch == 'w') {
                        ChangeToNextWord();
                    }
                    break;
                case 'y':
                    ch = getch();
                    if (ch == 'y') {
                        CopyLine();
                    }
                    break;
                case 'p':
                    PasteLineAfter();
                    break;
                case 'P':
                    PasteLineBefore();
                    break;
                case 'u':
                    Undo();
                    break;
                case '/':
                    Search();
                    break;
                case 'n':
                    NextMatch();
                    break;
                case 'N':
                    PreviousMatch();
                    break;
                case 26:  // CTRL+Z
                    running = false;
                    break;
                case 18:  // CTRL+R
                    Redo();
                    break;
            }
        } else if (currentMode == INSERT) {
            if (ch == 27) {  // Escape key
                currentMode = NORMAL;
                move(LINES - 1, 0);
                clrtoeol();
                printw("-- NORMAL --");
            } else {
                if (ch == KEY_BACKSPACE || ch == 127) {
                    if (!buffer[currentLine].empty() && currentColumn > 0) {
                        undoStack.push({currentLine, buffer[currentLine]});
                        buffer[currentLine].erase(--currentColumn, 1);
                        move(currentLine, currentColumn);
                        delch();
                    }
                } else {
                    undoStack.push({currentLine, buffer[currentLine]});
                    buffer[currentLine].insert(currentColumn++, 1, ch);
                    move(currentLine, currentColumn);
                    insch(ch);
                }
            }
        } else if (currentMode == COMMAND) {
            if (ch == '\r') {  // Enter key
                ProcessCommand(commandBuffer);
                commandBuffer.clear();
                currentMode = NORMAL;
                move(LINES - 1, 0);
                clrtoeol();
                printw("-- NORMAL --");
            } else {
                commandBuffer += ch;
                addch(ch);
            }
        }

        move(currentLine, currentColumn);
        refresh();
    }
}

void SignalHandler(int signum) {
    if (signum == SIGTSTP) {
        endwin();
        running = false;
        raise(SIGSTOP);
    } else if (signum == SIGINT) {
        endwin();
        exit(0);
    }
}

int main() {
    signal(SIGTSTP, SignalHandler);
    signal(SIGINT, SignalHandler);

    std::cout << "Enter filename: ";
    std::getline(std::cin, filename);
    LoadFile(filename);

    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);

    DisplayBuffer();
    ProcessInput();

    endwin();

    return 0;
}
