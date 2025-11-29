#ifdef _WIN32
#include <conio.h> // Windows header for _getch()

char get_immediate_input() {
    return _getch(); // Windows function to get char immediately
}

#else
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

// Linux/macOS implementation using termios
char get_immediate_input() {
    struct termios oldt, newt;
    int ch;
    
    // 1. Get current terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    
    // 2. Disable canonical mode (buffered i/o) and echo
    newt.c_lflag &= ~(ICANON | ECHO);
    
    // 3. Apply new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    // 4. Read the character
    ch = getchar();
    
    // 5. Restore old settings (important!)
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    
    return ch;
}

void clear() {
    // \x1B[2J  -> Limpa a tela visível
    // \x1B[3J  -> Limpa o histórico de prints
    // \x1B[H   -> Volta o cursor para o início
    std::cout << "\x1B[2J\x1B[3J\x1B[H"; 
    
    std::cout << std::flush;
}
#endif