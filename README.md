# Visão Geral e Sinopse do Jogo
“Em um computador distante, os processos 0 e 1 estão sendo executados simultaneamente e disputando os raros recursos existentes. Neste sistema, há uma zona crítica essencial para ambos, na qual o processo que primeiro entra se torna o dono dos recursos nela contidos e, caso consiga permanecer nela por tempo o suficiente, é executado com sucesso. No entanto, o processo deve lutar para permanecer na zona crítica, pois há outros processos à espreita que querem tomar seu lugar e não medirão esforços para isso…”

A mecânica baseia-se na brincadeira infantil **King of the Hill**.  
O objetivo central é ocupar uma *zona crítica* (a **Hill**) no tabuleiro.  
O primeiro processo a entrar torna-se o dono dos recursos e inicia uma contagem regressiva. O oponente deve então “empurrar” o atual rei para fora da zona, interrompendo o progresso e tomando o controle.  
**Vence quem permanecer na zona crítica pelo tempo necessário sem ser interrompido.**

# Como Jogar
O jogo foi desenvolvido em **C++** e utiliza um **Makefile** para facilitar a compilação.

### Compilação
```
$ make
```

### Execução
```
$ make run
```

Após isso, os jogadores poderão se mover:

- **Jogador 1:** `W`, `A`, `S`, `D`  
- **Jogador 2:** `I`, `J`, `K`, `L`  

O jogo termina quando um jogador vence ou quando o comando **X** é usado.

O programa utiliza a biblioteca padrão do **C++11 (ou superior)**, especialmente:

- `<thread>`
- `<mutex>`
- `<condition_variable>`
- `<chrono>`

# Explicação Conceitual

## 3.1 Threads
O jogo utiliza cinco threads:

1. Duas `player_thread`
2. Uma `zone_thread`
3. Uma `print_thread`
4. Uma `input_thread`

### 3.1.1 input_thread
Captura entradas do teclado e insere na fila `player_queue`.

### 3.1.2 player_thread
Consome movimentos, atualiza o tabuleiro e notifica outras threads.

### 3.1.3 zone_thread
Gerencia acesso e permanência na zona crítica e declara vitória.

### 3.1.4 print_thread
Imprime o tabuleiro sempre que houver atualização.

## 3.2 Regiões Críticas

### 3.2.1 Estado da zona
Protegido por `zone_mtx` e `zone_cv`.

### 3.2.2 Movimentação e tabuleiro
Protegido por `player_mtx` e `queue_cv`.

### 3.2.3 Output
Protegido por `print_mtx` e `print_cv`.

### 3.2.4 Condição de vitória
Protegido por `gameover_mtx`.
