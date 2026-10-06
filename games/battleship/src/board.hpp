#pragma once

#include <array>
#include <string>
#include <vector>

// Resultado de um tiro válido ou rejeitado.
enum class Shot {
    Invalid,
    Miss,
    Hit,
    Sunk
};

// Tamanho do tabuleiro e quantidade de navios por tipo.
struct FleetMode {
    const char* name;
    int size;
    std::array<int, 4> ships;
};

extern const std::array<FleetMode, 3> MODES;
extern const std::array<int, 4> SHIP_LENGTHS;
extern const std::array<const char*, 4> SHIP_NAMES;

// Aplica as regras de posicionamento e ataque sobre um tabuleiro.
class Board {
public:
    explicit Board(int size = 10);

    // Retorna a largura e a altura do tabuleiro quadrado.
    int Size() const { return size_; }

    // Posiciona o navio sem permitir contato com outra embarcação.
    bool Place(int type, int x, int y, bool vertical);

    // Registra o tiro e informa se acertou ou afundou um navio.
    Shot Fire(int x, int y);

    // A vitória ocorre quando não restam casas de navio intactas.
    bool AllSunk() const;

    // Oculta navios não atingidos quando a vista é do adversário.
    std::vector<std::string> Render(bool reveal_ships, int page) const;

private:
    int size_;
    // Zero: água; negativo: navio intacto; 2: tiro na água; 3: acerto.
    std::vector<std::vector<int>> cells_;
    int next_ship_id_ = 1;
};

// Converte coordenadas como A1 e Z26 para índices iniciados em zero.
bool ParseCoordinate(const std::string& value, int size, int& x, int& y);
