#include <iostream>
int main() {
    std::cout << "\033[2J\033[H";
    std::cout << "1234567890123456789012345678901234567890\r\n";
    std::cout << "┌──────────────────────────────────────┐\r\n";
    // Pretend there was garbage here before:
    std::cout << "│ XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX │\r"; 
    std::cout << "│ \033[1;31mHello Colors!\033[0m\033[K\033[40G│\r\n";
    std::cout << "└──────────────────────────────────────┘\r\n";
    return 0;
}
