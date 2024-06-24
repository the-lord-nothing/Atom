#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <curses.h>
#include <csignal>
#include <unistd.h>
#include <stack>

std::vector<std::string> buffer;
std::stack<std::pair<int, std::string>> undoStack; // Стек для отмены
std::stack<std::pair<int, std::string>> redoStack; // Стек для повтора
std::vector<std::string> clipboard; // Буфер для копирования строк
int currentLine = 0;
int currentColumn = 0;
std::string filename;

bool running = true;

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

void DisplayBuffer() {
    clear(); // Очистка окна

    int y = 0;
    for (const auto& line : buffer) {
        int x = 0;
        for (const char& ch : line) {
            if (ch == 'i' || ch == 'o' || ch == 'f' || ch == 'r') { // Пример ключевых слов
                attron(COLOR_PAIR(1));
                mvaddch(y, x, ch);
                attroff(COLOR_PAIR(1));
            } else {
                mvaddch(y, x, ch);
            }
            ++x;
        }
        ++y;
    }

    move(currentLine, currentColumn);
    refresh();
}

enum Mode { NORMAL, INSERT, COMMAND };
Mode currentMode = NORMAL;

void MoveCursor(int dx, int dy) {
    int newColumn = currentColumn + dx;
    int newLine = currentLine + dy;

    // Ограничения на перемещение курсора
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

void PasteLine() {
    if (!clipboard.empty()) {
        undoStack.push({currentLine, buffer[currentLine]});
        buffer.insert(buffer.begin() + currentLine, clipboard.begin(), clipboard.end());
        DisplayBuffer();
    }
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
        // TODO: Поиск
    } else if (command == ":s") {
        // TODO: Замена
    } else if (command.substr(0, 2) == "e ") {
        filename = command.substr(2);
        LoadFile(filename);
        DisplayBuffer();
    }
    // Добавление других команд
}

void ProcessInput() {
    std::string commandBuffer;
    int ch;
    int x = 0, y = 0;

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
                case 'd':
                    if (getch() == 'd') { // dd для удаления строки
                        if (currentLine < buffer.size()) {
                            undoStack.push({currentLine, buffer[currentLine]});
                            buffer.erase(buffer.begin() + currentLine);
                            if (currentLine >= buffer.size()) {
                                currentLine = buffer.size() - 1;
                            }
                            DisplayBuffer();
                        }
                    }
                    break;
                case 'y':
                    if (getch() == 'y') { // yy для копирования строки
                        CopyLine();
                    }
                    break;
                case 'p':
                    PasteLine();
                    break;
                case 'u':
                    Undo();
                    break;
                case '/':
                    // TODO: Поиск
                    break;
                case 'n':
                    // TODO: Переход к следующему совпадению при поиске
                    break;
                case 'N':
                    // TODO: Переход к предыдущему совпадению при поиске
                    break;
                case 26: // Ctrl+Z
                    running = false;
                    break;
                case 18: // Ctrl+R
                    Redo();
                    break;
            }
        } else if (currentMode == INSERT) {
            if (ch == 27) { // ESC key
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
            if (ch == '\r') { // Enter key
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
    }
}

int main() {
    signal(SIGTSTP, SignalHandler);

    // Запрос имени файла у пользователя
    std::cout << "Enter filename: ";
    std::getline(std::cin, filename);
    LoadFile(filename);

    // Инициализация ncurses
    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK); // Настройка цветовой пары

    DisplayBuffer();
    ProcessInput();

    // Завершение работы ncurses
    endwin();

    return 0;
}
