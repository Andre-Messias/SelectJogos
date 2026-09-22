#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sys/wait.h>

// Função segura para ler do buffer até o '\n'
std::string LerAteQuebraDeLinha(int socket_fd, std::string& buffer) {
    size_t pos;
    while ((pos = buffer.find('\n')) == std::string::npos) {
        char temp[256];
        int bytes_read = read(socket_fd, temp, sizeof(temp) - 1);
        if (bytes_read <= 0) {
            if (buffer.empty()) return ""; 
            std::string ultima_linha = buffer;
            buffer.clear();
            return ultima_linha;
        }
        temp[bytes_read] = '\0';
        buffer += temp;
    }
    std::string linha = buffer.substr(0, pos);
    buffer.erase(0, pos + 1);
    return linha;
}

void ReceberMensagens(int server_fd) {
    std::string buffer = "";
    while (true) {
        std::string msg = LerAteQuebraDeLinha(server_fd, buffer);
        if (msg.empty()) {
            std::cout << "\n[Conexão encerrada pelo Servidor]\n";
            exit(0);
        }
        std::cout << "\r[Servidor diz]: " << msg << "\nVocê: ";
        std::cout.flush();
    }
}

// Alterado para retornar o PID (Process ID) do servidor criado
pid_t InicializarServidor(const char* ip, const char* porta) {
    pid_t pid = fork();
    
    if (pid < 0) {
        std::cerr << "Falha ao criar o processo do servidor (fork falhou).\n";
        exit(1);
    } 
    else if (pid == 0) {
        execl("./server", "./server", ip, porta, nullptr);
        std::cerr << "Erro: Não foi possível executar './server'.\n";
        exit(1);
    }
    
    return pid; // Retorna o ID do processo filho para o Pai (Cliente)
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Uso correto: " << argv[0] << " <IP_PARA_HOSPEDAR> <PORTA>\n";
        return 1;
    }

    const char* ip = argv[1];
    const char* porta_str = argv[2];
    int porta = std::stoi(porta_str);

    // 1. Inicia o servidor e guarda o ID do processo
    pid_t server_pid = InicializarServidor(ip, porta_str);
    std::cout << "Aguardando o servidor inicializar...\n";

    // 2. Configura o Socket do cliente
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(porta);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    // 3. Loop de espera inteligente (Wait Loop)
    while (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        int status;
        
        // waitpid com WNOHANG apenas espia o processo. 
        // Se retornar o mesmo ID do servidor, significa que o servidor morreu/fechou.
        if (waitpid(server_pid, &status, WNOHANG) == server_pid) {
            std::cerr << "\n[Erro Fatal] O servidor falhou ao iniciar ou fechou inesperadamente.\n";
            std::cerr << "Verifique se a porta " << porta << " já está em uso.\n";
            return 1;
        }
        
        // Dorme por 100ms antes de tentar novamente para não sobrecarregar a CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Conectado com sucesso! Pode enviar mensagens.\n\n";

    std::thread thread_receber(ReceberMensagens, client_fd);
    thread_receber.detach();

    std::string input;
    while (true) {
        std::cout << "Você: ";
        std::getline(std::cin, input);
        
        if (!input.empty()) {
            input += "\n"; 
            write(client_fd, input.c_str(), input.length());
        }
    }

    close(client_fd);
    return 0;
}