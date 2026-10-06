#pragma once

#include "board.hpp"

#include <array>
#include <string>

// Guarda a frota e aplica as ações individuais de um participante.
struct Player {
    int id;
    std::string name;
    bool connected = true;
    bool ready = false;
    bool own_view = true;
    int page = 1;
    std::array<int, 4> placed{{0, 0, 0, 0}};
    Board board;

    // Cria o jogador com um tabuleiro do tamanho escolhido.
    Player(int client_id, std::string player_name, int size);

    // Informa se o jogador já posicionou algum navio.
    bool HasPlacedShips() const;

    // Confere se todos os navios exigidos pelo modo foram posicionados.
    bool HasCompleteFleet(const FleetMode& mode) const;

    // Verifica se ainda há vaga para um navio desse tipo.
    bool CanPlaceShip(int type, const FleetMode& mode) const;

    // Posiciona o navio e atualiza a quantidade colocada.
    bool PlaceShip(int type, int x, int y, bool vertical);

    // Lista os navios que ainda precisam ser posicionados.
    std::string RemainingShips(const FleetMode& mode) const;

    // Reinicia o tabuleiro e volta à primeira página.
    void ResetBoard(int size);

    // Mostra o tabuleiro adversário ao começar a batalha.
    void BeginBattleView();

    // Define qual tabuleiro e página o jogador deseja ver.
    void SetView(bool show_own, int selected_page);

    // Limita a página selecionada ao intervalo disponível.
    int VisiblePage(int max_page) const;
};
