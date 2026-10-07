#pragma once

#include <vector>
#include <string>

// Letras para coordenadas das linhas (suporta até 81 linhas pro Ultranightmare)
inline const std::string COORDS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$=^&*()-_=+[]{}|:;?/>.<,~`";

// Estrutura de cada célula do tabuleiro
struct Cell
{
    int type; // 0 = vazio, 1-8 = contagem de bombas vizinhas, 9 = bomba
    bool revealed;
    bool flagged;
    bool exploded;
};

struct MoveInput
{
    int row;
    int col;
    enum Action
    {
        REVEAL,
        FLAG,
        UNFLAG
    } action;
};

// Interpreta o input do jogador. Formato: [flag|f|unflag|u]<letra><número>
bool ParseInput(const std::string &input, int board_size, MoveInput &move);

class Board
{
public:
    Board(int size, int bomb_count);

    // Gera o campo de minas, garantindo área segura ao redor da primeira jogada (row, col)
    void Generate(int first_row, int first_col);

    // Revela uma célula. Retorna true se pisou em bomba.
    bool Reveal(int row, int col);

    // Revela todas as bombas quando o jogo é perdido
    void RevealMines(int hit_row, int hit_col);

    // Marca/desmarca flag numa célula
    void SetFlag(int row, int col, bool flagged);

    // Checa se todas as células não-bomba foram reveladas
    bool IsComplete() const;

    // Retorna o tabuleiro como string (para enviar pela rede ou imprimir no terminal)
    std::string Render() const;

    int Size() const;
    bool IsGenerated() const;

private:
    int _size;
    int _bomb_count;
    bool _generated;
    int _revealed_count;
    std::vector<std::vector<Cell>> _grid;

    bool InBounds(int row, int col) const;

    // Para cada célula que não é bomba, conta quantas bombas existem nos 8 blocos à sua volta
    void ComputeNeighborCounts();

    // Flood-fill: revela recursivamente as células vazias adjacentes
    void FloodReveal(int row, int col);
};
