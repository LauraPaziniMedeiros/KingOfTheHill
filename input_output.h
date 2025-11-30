#ifdef _WIN32
#include <conio.h>

char get_immediate_input() {
    return _getch();
}

#else
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

char get_immediate_input() {
    struct termios oldt, newt;
    int ch;
    
    // Guarda as configurações atuais do terminal
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    
    // Desativa o modo padrão (evita esperar Enter) e desativa o eco na tela
    newt.c_lflag &= ~(ICANON | ECHO);
    
    // Aplica as novas configurações imediatamente
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    // Lê um caracter imediatamente do teclado
    ch = getchar();
    
    // Restaura as configurações originais do terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    
    return ch; // Retorna o caracter lido
}


void clear() {
    // \x1B[2J  -> Limpa a tela visível
    // \x1B[3J  -> Limpa o histórico de prints
    // \x1B[H   -> Volta o cursor para o início
    std::cout << "\x1B[2J\x1B[3J\x1B[H"; 
    
    std::cout << std::flush;
}
#endif