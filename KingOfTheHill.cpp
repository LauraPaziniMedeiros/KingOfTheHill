#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <queue>
#include "input_output.h"
#include <string>
#include <chrono>
using namespace std;

/*************** CONFIGURAÇÕES ***************/

#define GRID_SIZE 11
#define ZONE_SIZE 3

// Configurações do terminal
const string RED_COLOR = "\x1B[31m";
const string RESET_COLOR = "\x1B[0m";

const chrono::seconds WIN_DURATION{5}; // Tempo mínimo dentro da zona para a vitória

/*************** CONSTANTES ***************/

const int zone_start = (GRID_SIZE - ZONE_SIZE) / 2;
const int zone_end = zone_start + ZONE_SIZE;
string separator = "-";
vector<vector<char>> original_grid(GRID_SIZE, vector<char>(GRID_SIZE)); // Restaura valores originais na movimentação

/*************** VARIÁVEIS COMPARTILHADAS (REGIÕES CRÍTCAS) ***************/

// Variáveis relacionadas à zona crítica (hill) do jogo
condition_variable zone_cv; // Sinaliza quando um jogador entra na zona
int zone_state = -1; // -1 = zona vazia, 0 = jogador 0 está na zona, 1 = jogador 1 está na zona
int zone_change_counter = 0; // Contador para mudança de estado. É usado para acordar a zone_thread.
mutex zone_mtx; // Atua como um semáforo binário

// Variáveis relacionadas ao input e lógica de movimentação dos jogadores
vector<vector<char>> grid(GRID_SIZE, vector<char>(GRID_SIZE)); // Estado atual do tabuleiro
vector<pair<int, int>> players = {{0, 0}, {GRID_SIZE - 1, GRID_SIZE - 1}}; // Posição de cada jogador no tabuleiro
vector<queue<char>> player_queue(2); // Fila que guarda a sequência de movimentos (inputs) de cada jogador
condition_variable queue_cv; // Sinaliza um novo input na fila de movimentos
mutex player_mtx; // Atua como um semáforo binário

// Variáveis relacionadas à sinalização de fim de jogo entre as threads
bool game_over = false; // Indica que o jogo terminou
bool win = false; // Indica que o último jogador na zona venceu
mutex gameover_mtx; // Atua como um semáforo binário

// Variáveis relacionadas ao output do estado do jogo
bool print = false; // Alterada para true quando uma escrita é requisitada
condition_variable print_cv; // Sinaliza uma requisição de escrita no terminal
mutex print_mtx; // Atua como um semáforo binário

/*************** FUNÇÕES AUXILIARES ***************/

/**
 * @brief Imprime o estado atual do tabuleiro
 */
void draw_board(void) {
    clear();
    cout << separator << endl;

    for (int i = 0; i < GRID_SIZE; i++) {
        cout << "| ";
        for (int j = 0; j < GRID_SIZE; j++) {
            if (grid[i][j] == '*') {
                // Escreve a área crítica em vermelho, depois volta para a cor padrão
                cout << RED_COLOR << grid[i][j] << RESET_COLOR << " | ";
            } else {
                cout << grid[i][j] << " | ";
            }
            
        }
        cout << endl << separator << endl;
    }
    return;
}

/**
 * @brief Inicializa o tabuleiro antes do jogo começar
 */
void initialize_grid(void) {
    for(int i = 0; i < GRID_SIZE; i++) {
        for(int j = 0; j < GRID_SIZE; j++) {
            original_grid[i][j] = ' ';
        }
        separator += "----";
    }
    for(int i = zone_start; i < zone_end; i++) {
        for(int j = zone_start; j < zone_end; j++) {
            original_grid[i][j] = '*';
        }
    }
    grid = original_grid; // Copia o tabuleiro original para o atual
    grid[0][0] = '0';
    grid[GRID_SIZE-1][GRID_SIZE-1] = '1';

    draw_board();
    return;
}

/**
 * @brief Empurra o jogador adjacente à movimentação de outro jogador
 * @param r Posição do jogador na linha
 * @param c Posição do jogador na coluna
 * @param direction Direção do movimento do jogador
 * @param playar Símbolo do jogador no tabuleiro
 */
void push_player(int r, int c, char direction, char player) {
    switch (direction) {
        case 'w': r = (r == 0) ? (GRID_SIZE - 1) : r - 1; break;
        case 's': r = (r + 1) % GRID_SIZE; break;
        case 'a': c = (c == 0) ? (GRID_SIZE - 1) : c - 1; break;
        case 'd': c = (c + 1) % GRID_SIZE; break;
        default: break;
    }
    grid[r][c] = player;
    players[player - '0'] = {r, c};
}

/**
 * @brief Verifica se o jogador está dentro da zona crítica
 * @param player_id ID do jogador
 * @return true se o jogador está dentro da zona, false caso contrário
 */
bool in_zone(bool player_id) {
    int x = players[player_id].first;
    int y = players[player_id].second;
    return x >= zone_start && x < zone_end && y >= zone_start && y < zone_end;
}

/*************** THREADS ***************/

/**
 * @brief Thread de output do jogo. Imprime o tabuleiro atual e o estado da zona crítica.
 */
void print_thread(void) {
    while(!game_over) {
        unique_lock<mutex> print_lock(print_mtx);
        // Aguarda um sinal para imprimir uma atualização no estado do jogo
        print_cv.wait(print_lock, []{return print || game_over || win;});

        { // Verifica se o jogo foi interrompido sem uma vitória
            unique_lock<mutex> gameover_lock(gameover_mtx);
            if(game_over && !win) break; 
        }
        { // Desenha o tabuleiro atual
            unique_lock<mutex> player_lock(player_mtx);
            draw_board();
        }
        { // Verifica a condição de vitória e imprime o estado atual da zona
            unique_lock<mutex> gameover_lock(gameover_mtx);
            if(win) {
                unique_lock<mutex> zone_lock(zone_mtx);
                if(zone_state != -1) {
                    cout << "PROCESSO " << zone_state << " FOI EXECUTADO COM SUCESSO! PRESSIONE [ENTER] PARA SAIR.\n";
                }
                break;
            }
            else {
                unique_lock<mutex> zone_lock(zone_mtx);
                if(zone_state != -1)
                    cout << "PROCESSO " << zone_state << " ESTÁ NA ZONA CRÍTICA!\n";
            }
            if(game_over) break;
        }
        print = false; // Reinicializa a variável de impressão
    }
}

/**
 * @brief Thread de input para registrar a movimentação dos jogadores e o fim de jogo
 */
void input_thread(void) {
    while(!game_over) {
        char move = get_immediate_input();
        lock_guard<mutex> player_lock(player_mtx);
        switch(move) {
            case 'w' : player_queue[0].push('w'); break;
            case 'a' : player_queue[0].push('a'); break;
            case 's' : player_queue[0].push('s'); break;
            case 'd' : player_queue[0].push('d'); break;
            case 'i' : player_queue[1].push('w'); break;
            case 'j' : player_queue[1].push('a'); break;
            case 'k' : player_queue[1].push('s'); break;
            case 'l' : player_queue[1].push('d'); break;
        }
        // Sinaliza as threads de jogador aguardando um novo movimento que a fila não está mais vazia
        queue_cv.notify_all();
        // Verifica o input de saída
        if (move == 'x') {
            { // Atualiza a variável de game over
               unique_lock<mutex> gameover_lock(gameover_mtx); 
               game_over = true;
            }
            // Notifica todas as threads em espera que o jogo terminou
            queue_cv.notify_all();
            zone_cv.notify_all();
            print_cv.notify_all();
            return;
        }
    }
}

/**
 * @brief Thread de verificação do estado atual da zona crítica. Conta o tempo que um jogador 
 * permaneceu dentro da zona e eventuais mudanças de estado.
 */
void zone_thread() {
    int last_winning_player = -1;
    while(!game_over) {
        unique_lock<mutex> zone_lock(zone_mtx);
        // Aguarda um jogador entrar na zona
        zone_cv.wait(zone_lock, [] {return zone_state != -1 || game_over;});

        {
            unique_lock<mutex> gameover_lock(gameover_mtx);
            if(game_over) break; 
        }

        // Zona está ocupada
        last_winning_player = zone_state;
        auto win_time = chrono::steady_clock::now() + WIN_DURATION; // Começa um contador para o tempo de vitória

        // Espera uma interrupção ou o tempo para a vitória do jogador
        int last_counter = zone_change_counter;
        bool interruption = zone_cv.wait_until(zone_lock, win_time, [last_counter]{
            return zone_change_counter != last_counter || game_over;});

        {
            unique_lock<mutex> gameover_lock(gameover_mtx);
            if(game_over) break; 
        }

        if(!interruption && last_winning_player == zone_state) { // Vitória
            { // Atualiza as variáveis de fim de jogo
                unique_lock gameover_lock(gameover_mtx);
                game_over = true;
                win = true;
            }
            // Envia mensagem de vitória para a thread de print
            {
                lock_guard<mutex> print_lock(print_mtx);
                print = true;
            }
            print_cv.notify_all();
            // Notifica todas as threads em espera que o jogo terminou
            zone_lock.unlock();
            queue_cv.notify_all();
        }
        else { // Reinicializa a zona            
            last_winning_player = zone_state;
            zone_lock.unlock();
        }
    }
}

/**
 * @brief Thread de movimentação dos jogadores e atualização do estado do tabuleiro.
 */
void player_thread(bool player_id) {
    while(!game_over) {
        unique_lock<mutex> player_lock(player_mtx);

        // Aguarda até que a fila não esteja vazia OU que o jogo termine
        queue_cv.wait(player_lock, [player_id]{return !player_queue[player_id].empty() || game_over;}); 
            
        {
            unique_lock<mutex> gameover_lock(gameover_mtx);
            if(game_over) break; 
        }

        char move = player_queue[player_id].front();
        player_queue[player_id].pop();

        // Registra o movimento
        int r = players[player_id].first;
        int c = players[player_id].second;
        switch (move) {
            case 'w': r = (r == 0) ? (GRID_SIZE - 1) : r - 1; break;
            case 's': r = (r + 1) % GRID_SIZE; break;
            case 'a': c = (c == 0) ? (GRID_SIZE - 1) : c - 1; break;
            case 'd': c = (c + 1) % GRID_SIZE; break;
            default: break;   
        }    

        // Empurra o jogador
        if(grid[r][c] != ' ' && grid[r][c] != '*')
            push_player(r, c, move, grid[r][c]);

        // Atualiza o tabuleiro
        grid[r][c] = player_id + '0';
        grid[players[player_id].first][players[player_id].second] = original_grid[players[player_id].first][players[player_id].second];
        players[player_id] = {r, c};   

        { // Verificação do estado atual da zona
            unique_lock<mutex> zone_lock(zone_mtx);
            bool p0_inside = in_zone(0);
            bool p1_inside = in_zone(1);
            if(!p0_inside && !p1_inside) { // Nenhum dos jogadores está na zona
                zone_state = -1;
                // zone_change_counter++;
                zone_lock.unlock();
                zone_cv.notify_one();
            } // Pelo menos um dos jogadores está na zona
            else if(zone_state == -1 || !in_zone(zone_state)) { // Atualiza o zone_state se a zona estiver livre
                if(p0_inside) zone_state = 0;
                else zone_state = 1;
                zone_change_counter++;
                zone_lock.unlock();
                zone_cv.notify_one();
            }
        }
        
        { // Imprime a mudança de estado no jogo
            unique_lock<mutex> print_lock(print_mtx);
            print = true;
        }
        print_cv.notify_one();
    }
}

int main() {
    // Iniciaiza o tabuleiro antes do jogo começar
    initialize_grid();
    
    thread t_print(print_thread);
    thread t_input(input_thread);
    thread t_player0(player_thread, 0);
    thread t_player1(player_thread, 1);
    thread t_zone(zone_thread);
    
    t_print.join();
    t_input.join();
    t_player0.join();
    t_player1.join();
    t_zone.join();

    return 0;
}