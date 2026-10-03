#include "board.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace std;

/*
    Interpreta o input do jogador.

    Formato: [flag|f|unflag|u]<letra><número>
    Exemplos: A0, B12, flagA0, fA0, unflagC3, uC3
*/
bool ParseInput(const string &input, int board_size, MoveInput &move)
{
    string s = input;

    move.action = MoveInput::REVEAL;
    size_t offset = 0;

    if (s.length() >= 6 && s.substr(0, 6) == "unflag")
    {
        move.action = MoveInput::UNFLAG;
        offset = 6;
    }
    else if (s.length() >= 4 && s.substr(0, 4) == "flag")
    {
        move.action = MoveInput::FLAG;
        offset = 4;
    }
    else if (s.length() >= 2 && s[0] == 'u' && COORDS.find(s[1]) != string::npos)
    {
        move.action = MoveInput::UNFLAG;
        offset = 1;
    }
    else if (s.length() >= 2 && s[0] == 'f' && COORDS.find(s[1]) != string::npos)
    {
        move.action = MoveInput::FLAG;
        offset = 1;
    }

    if (offset >= s.length())
        return false;

    // A letra indica a linha
    size_t row_idx = COORDS.find(s[offset]);
    if (row_idx == string::npos || (int)row_idx >= board_size)
        return false;
    move.row = (int)row_idx;

    // O resto são dígitos indicando a coluna
    string col_str = s.substr(offset + 1);
    if (col_str.empty())
        return false;

    try
    {
        move.col = stoi(col_str);
    }
    catch (...)
    {
        return false;
    }

    if (move.col < 0 || move.col >= board_size)
        return false;

    return true;
}

Board::Board(int size, int bomb_count)
    : _size(size), _bomb_count(bomb_count), _generated(false), _revealed_count(0)
{
    _grid.resize(_size, vector<Cell>(_size, {0, false, false}));
}

int Board::Size() const { return _size; }
bool Board::IsGenerated() const { return _generated; }

bool Board::InBounds(int row, int col) const
{
    return row >= 0 && row < _size && col >= 0 && col < _size;
}

// =========================================================
// Geração do tabuleiro
// =========================================================

/*
    Gera todas as bombas no campo, tendo certeza de não colocá-las
    num raio de um bloco do jogador (nos oito à sua volta).

    Depois, calcula a contagem de bombas vizinhas pra cada célula.
*/
void Board::Generate(int first_row, int first_col)
{
    int placed = 0;

    while (placed < _bomb_count)
    {
        int r = rand() % _size;
        int c = rand() % _size;

        // Não colocar bomba duplicata, nem perto da primeira jogada
        if (_grid[r][c].type == 9)
            continue;
        if (abs(r - first_row) <= 1 && abs(c - first_col) <= 1)
            continue;

        _grid[r][c].type = 9;
        placed++;
    }

    ComputeNeighborCounts();
    _generated = true;
}

/*
    Dar uma função à cada bloco, e já confirmar a soma de bombas
    nos 8 blocos à sua volta
*/
void Board::ComputeNeighborCounts()
{
    // As 8 direções: cima, baixo, esquerda, direita, e diagonais
    static const int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int i = 0; i < _size; i++)
    {
        for (int j = 0; j < _size; j++)
        {
            if (_grid[i][j].type == 9)
                continue;

            int sum = 0;
            for (int d = 0; d < 8; d++)
            {
                int ni = i + dx[d];
                int nj = j + dy[d];
                if (InBounds(ni, nj) && _grid[ni][nj].type == 9)
                {
                    sum++;
                }
            }
            _grid[i][j].type = sum;
        }
    }
}

// =========================================================
// Revelar células
// =========================================================

/*
    Ler para cada movimento, se as casas ao lado também serão reveladas...

    Se a célula tem vizinhos com bomba (tipo 1-8), revela só ela e para.
    Se a célula é vazia (tipo 0), revela e continua recursivamente
    para os 8 vizinhos.
*/
void Board::FloodReveal(int row, int col)
{
    if (!InBounds(row, col))
        return;
    if (_grid[row][col].revealed)
        return;
    if (_grid[row][col].flagged)
        return;

    _grid[row][col].revealed = true;
    _revealed_count++;

    // Se tem vizinhos com bomba, para aqui
    if (_grid[row][col].type != 0)
        return;

    // Senão, revela os 8 vizinhos
    static const int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int d = 0; d < 8; d++)
    {
        FloodReveal(row + dx[d], col + dy[d]);
    }
}

bool Board::Reveal(int row, int col)
{
    if (!InBounds(row, col))
        return false;
    if (_grid[row][col].revealed)
        return false;
    if (_grid[row][col].flagged)
        return false;

    // Pisou em bomba
    if (_grid[row][col].type == 9)
        return true;

    FloodReveal(row, col);
    return false;
}

void Board::SetFlag(int row, int col, bool flagged)
{
    if (!InBounds(row, col))
        return;
    if (_grid[row][col].revealed)
        return;
    _grid[row][col].flagged = flagged;
}

bool Board::IsComplete() const
{
    return _revealed_count >= (_size * _size - _bomb_count);
}

/*
    Retorna o tabuleiro como string.

    Cores ANSI para os números (type + 30):
    1=vermelho, 2=verde, 3=amarelo, 4=azul, 5=magenta,
    6=ciano, 7=branco, 8=cinza
*/
string Board::Render() const
{
    char buf[64];
    string out = "";

    // Cabeçalho com números das colunas
    out += "\n      ";
    for (int c = 0; c < _size; c++)
    {
        snprintf(buf, sizeof(buf), "%d", c);
        out += buf;
        if (c < 10)
            out += "   ";
        else
            out += "  ";
    }
    out += "\n    ";

    for (int c = 0; c < _size; c++)
    {
        out += "+---";
    }
    out += "+\n";

    for (int r = 0; r < _size; r++)
    {
        // Letra da linha + conteúdo das células
        out += " ";
        out += string(1, COORDS[r]);
        out += "  ";

        for (int c = 0; c < _size; c++)
        {
            const Cell &cell = _grid[r][c];

            if (cell.flagged && !cell.revealed)
            {
                out += "│\033[1;31m ⚑ \033[0m";
            }
            else if (!cell.revealed)
            {
                out += "│\033[1m X \033[0m";
            }
            else if (cell.type == 0)
            {
                out += "│   ";
            }
            else
            {
                int color_code = (cell.type == 8) ? 90 : (cell.type + 30);
                snprintf(buf, sizeof(buf), "│\033[%dm %d \033[0m", color_code, cell.type);
                out += buf;
            }
        }
        out += "│\n";

        // Borda inferior da linha
        out += "    ";
        for (int c = 0; c < _size; c++)
        {
            out += "+---";
        }
        out += "+\n";
    }

    return out;
}
