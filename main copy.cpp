#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

#include "config.hpp"
#include "player.hpp"
#include "server.hpp"

struct GameEvent {
    std::string command;
    std::string message_id;
    int client_id;
    int socket_fd;
    std::string payload; 
};

std::queue<GameEvent> event_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;

// O Jogo mantém seu próprio mapa LÓGICO de qual Jogador está em qual Socket
std::unordered_map<int, int> client_sockets;
std::vector<Player> players;
int actions_received = 0;


// =========================================================
// THREAD DA REDE (Alimenta a Fila)
// =========================================================
void OnNetworkMessage(int socket_fd, const std::string& raw_token) {
    std::istringstream iss(raw_token);
    GameEvent ev;
    ev.socket_fd = socket_fd;

    if (!(iss >> ev.command >> ev.message_id >> ev.client_id)) {
        return; 
    }
    
    std::string extra;
    while (iss >> extra) {
        ev.payload += extra + " ";
    }
    
    // Remove o último espaço extra do payload, se existir
    if (!ev.payload.empty()) ev.payload.pop_back();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        event_queue.push(ev);
    }
    queue_cv.notify_one();
}


// =========================================================
// THREAD DO JOGO: Recebe a interface Server para responder
// =========================================================
void GameProcessor(Server& game_room) {
    std::cout << "[Game] Logica iniciada. Aguardando mensagens da GameRoom...\n";

    while (actions_received < 2) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, [] { return !event_queue.empty(); });

        GameEvent ev = event_queue.front();
        event_queue.pop();
        lock.unlock(); 

        if (ev.command == "ConnectClient") {
            client_sockets[ev.client_id] = ev.socket_fd;
            
            bool exists = false;
            for (auto& p : players) if (p.getId() == (uint)ev.client_id) exists = true;
            if (!exists) players.push_back(Player(ev.client_id));

            std::cout << "[Game] Jogador " << ev.client_id << " entrou na GameRoom.\n";
            
            // Usando o método do Server!
            game_room.SendMessage(ev.socket_fd, "Response " + ev.message_id + " Success\n");
        } 
        else if (ev.command == "PlayerAction") {
            try {
                int number = std::stoi(ev.payload);
                bool player_found = false;

                for (auto& p : players) {
                    if (p.getId() == (uint)ev.client_id) {
                        p.setNumber(number);
                        player_found = true;
                        actions_received++;
                        break;
                    }
                }

                if (player_found) {
                    std::cout << "[Game] Jogador " << ev.client_id << " jogou a acao: " << number << "\n";
                    game_room.SendMessage(ev.socket_fd, "Response " + ev.message_id + " Success\n");
                } else {
                    game_room.SendMessage(ev.socket_fd, "Response " + ev.message_id + " Fail \"Jogador Nao Conectado\"\n");
                }
            } catch (...) {
                game_room.SendMessage(ev.socket_fd, "Response " + ev.message_id + " Fail \"Acao Invalida\"\n");
            }
        }
    }

    // Calcula vencedor
    std::string resultado;
    if (players[0].getNumber() > players[1].getNumber()) {
        resultado = "Player " + std::to_string(players[0].getId()) + " venceu!";
    } else if (players[0].getNumber() < players[1].getNumber()) {
        resultado = "Player " + std::to_string(players[1].getId()) + " venceu!";
    } else {
        resultado = "Deu empate!";
    }

    std::cout << "[Game] Partida encerrada: " << resultado << "\n";
    
    // Pede para a GameRoom avisar todo mundo conectado nela!
    game_room.Broadcast("LogChannel All \"" + resultado + "\"\n");
}


// =========================================================
// MAIN
// =========================================================
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <PORT>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    
    // Inicializa a GameRoom
    Server game_room(port);
    game_room.OnMessageReceived(OnNetworkMessage);

    // Inicia o Game, passando a referência da GameRoom para ele poder enviar mensagens
    std::thread game_thread(GameProcessor, std::ref(game_room));

    // Roda o servidor em background
    std::thread server_thread([&game_room]() {
        game_room.Start();
    });
    server_thread.detach();

    // Aguarda o jogo acabar
    game_thread.join();
    
    std::cout << "Desligando sistema...\n";
    return 0;
}