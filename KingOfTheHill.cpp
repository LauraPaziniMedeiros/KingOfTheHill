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

// Configurações do tabuleiro
#define GRID_SIZE 10
#define ZONE_SIZE 3

// Configurações do terminal
const string RED_COLOR = "\x1B[31m";
const string RESET_COLOR = "\x1B[0m";

const chrono::seconds WIN_DURATION{5}; // Tempo mínimo dentro da zona para a vitória

// Constantes
const int zone_start = (GRID_SIZE - ZONE_SIZE) / 2;
const int zone_end = zone_start + ZONE_SIZE;
string separator = "-";
vector<vector<char>> original_grid(GRID_SIZE, vector<char>(GRID_SIZE)); // Restaura valores originais na movimentação

// Variáveis compartilhadas entre as threads
vector<vector<char>> grid(GRID_SIZE, vector<char>(GRID_SIZE)); // Estado atual do tabuleiro
vector<pair<int, int>> players = {{0, 0}, {GRID_SIZE - 1, GRID_SIZE - 1}}; // Posição de cada jogador no tabuleiro
bool game_over = false; // Indica que o jogo terminou
vector<queue<char>> player_queue(2); // Uma fila que guarda a sequência de movimentos de cada jogador
condition_variable queue_cv; // Sinaliza um novo input na fila de movimentos
condition_variable zone_cv; // Sinaliza quando um jogador entra na zona
int zone_state = -1; // -1 = zona vazia, 0 = jogador 0 está na zona, 1 = jogador 1 está na zona
int zone_change_counter = 0; // Contador para mudança de estado. É usado para acordar a zone_thread.

// Mutex que atuam como um semáforos binários e controlam o acesso das threads às variáveis compartilhadas
mutex gameover_mtx;
mutex player_mtx;
mutex zone_mtx;

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
    grid = original_grid;
    grid[0][0] = '0';
    grid[GRID_SIZE-1][GRID_SIZE-1] = '1';
    draw_board();
    return;
}

void input_thread(void) {
    while(!game_over) {
        char move = get_immediate_input();
        {
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
        } // player_lock fora de escopo
        // Sinaliza as threads de jogador aguardando um novo movimento que a fila não está mais vazia
        queue_cv.notify_all();
        // Verifica o input de saída
        if (move == 'x') {
            // Notifica todas as threads em espera que o jogo terminou
            unique_lock<mutex> gameover_lock(gameover_mtx);
            game_over = true;
            gameover_lock.unlock(); // Unlocks to notify all threads
            queue_cv.notify_all();
            zone_cv.notify_all();
            return;
        }
    }
}

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

bool in_zone(bool player_id) {
    if(player_id == -1) return false;
    int x = players[player_id].first;
    int y = players[player_id].second;
    return x >= zone_start && x < zone_end && y >= zone_start && y < zone_end;
}

void zone_thread() {
    int last_winning_player = -1;
    while(!game_over) {
        unique_lock<mutex> gameover_lock(gameover_mtx);
        gameover_lock.unlock();
        unique_lock<mutex> zone_lock(zone_mtx);
        // Aguarda um jogador entrar na zona
        zone_cv.wait(zone_lock, [] {return zone_state != -1 || game_over;});

        gameover_lock.lock();
        if(game_over) break;
        gameover_lock.unlock();

        // Zona está ocupada
        last_winning_player = zone_state;
        auto win_time = chrono::steady_clock::now() + WIN_DURATION;

        // Espera uma interrupção ou o tempo para a vitória do jogador
        int last_counter = zone_change_counter; // Guarda o último número de mudanças no estado da zona
        bool interruption = zone_cv.wait_until(zone_lock, win_time, [last_counter]{ // Acorda se houve uma mudança no estado da zona
            return zone_change_counter != last_counter || game_over;});

        gameover_lock.lock();
        if(game_over) break;
        gameover_lock.unlock();

        if(!interruption) {
            cout << "JOGADOR " << last_winning_player << " VENCEU!\n";
            gameover_lock.lock();
            game_over = true;
            gameover_lock.unlock();
            zone_lock.unlock();
            queue_cv.notify_all();
        }
        else {
            cout << "JOGADOR " << last_winning_player << " SAIU DA ZONA!\n";
            last_winning_player = -1;
            zone_lock.unlock();
        }
    }
}

void player_thread(bool player_id) {
    while(!game_over) {
        unique_lock<mutex> player_lock(player_mtx);
        unique_lock<mutex> gameover_lock(gameover_mtx);
        gameover_lock.unlock();

        // Aguarda até que a fila não esteja vazia OU que o jogo termine
        queue_cv.wait(player_lock, [player_id]{return !player_queue[player_id].empty() || game_over;}); 
            
        gameover_lock.lock();
        if(game_over) break;
        gameover_lock.unlock();

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

        // Verificação do estado atual da zona
        {
            unique_lock<mutex> zone_lock(zone_mtx);
            bool p0_inside = in_zone(0);
            bool p1_inside = in_zone(1);
            if(!p0_inside && !p1_inside) { 
                zone_state = -1;
                zone_change_counter++;
                zone_lock.unlock();
                zone_cv.notify_one();
            } 
            else if(zone_state == -1 || !in_zone(zone_state)) { 
                if(p0_inside) zone_state = 0;
                else zone_state = 1;
                zone_change_counter++;
                zone_lock.unlock();
                zone_cv.notify_one();
            }
        }
        
        draw_board();
    }
}

int main() {
    initialize_grid();
    thread t(input_thread);
    thread t1(player_thread, 0);
    thread t2(player_thread, 1);
    thread t3(zone_thread);
    t.join();
    t1.join();
    t2.join();
    t3.join();

    return 0;
}