// =============================================================================
// IOTClient_v1.cpp — Cliente UDP básico (Fase 1: Hello World)
// Plataforma: Raspberry Pi (cross-compilación con arm-linux-gnueabihf-g++)
//
// COMPILACIÓN CRUZADA (ejecutar en Ubuntu host):
//   arm-linux-gnueabihf-g++ -std=c++17 -Wall -Wextra \
//       -o IOTClient_v1 IOTClient_v1.cpp
//
// COMPILACIÓN NATIVA (si se compila directamente en la RPi):
//   g++ -std=c++17 -Wall -Wextra -o IOTClient_v1 IOTClient_v1.cpp
//
// USO:
//   ./IOTClient_v1 <ip_servidor> <puerto_udp>
//   Ejemplo: ./IOTClient_v1 192.168.1.100 5000
// =============================================================================

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>   // timeval para SO_RCVTIMEO

// ─── Constantes ─────────────────────────────────────────────────────────────
static constexpr int  BUFFER_SIZE     = 256;
static constexpr char MSG_HELLO[]     = "Hello Server";
static constexpr int  RECV_TIMEOUT_S  = 5;   // Timeout de espera de ACK en segundos

// ─── main ───────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    // 1) Validar argumentos (REGLA: cero hardcoding de red)
    if (argc != 3) {
        std::cerr << "Uso: " << argv[0] << " <ip_servidor> <puerto_udp>\n";
        return EXIT_FAILURE;
    }

    const char* SERVER_IP   = argv[1];
    const int   SERVER_PORT = std::atoi(argv[2]);

    if (SERVER_PORT <= 0 || SERVER_PORT > 65535) {
        std::cerr << "[ERROR] Puerto inválido.\n";
        return EXIT_FAILURE;
    }

    // ── 2) Crear socket UDP ──────────────────────────────────────────────────
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[ERROR] socket()");
        return EXIT_FAILURE;
    }
    std::cout << "[CLIENT] Socket UDP creado.\n";

    // ── 3) Configurar timeout de recepción (REGLA: SO_RCVTIMEO) ─────────────
    // Esto evita que recvfrom() bloquee indefinidamente esperando el ACK.
    timeval tv{};
    tv.tv_sec  = RECV_TIMEOUT_S;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("[ERROR] setsockopt(SO_RCVTIMEO)");
        close(sockfd);
        return EXIT_FAILURE;
    }
    std::cout << "[CLIENT] Timeout de recepción configurado: "
              << RECV_TIMEOUT_S << "s\n";

    // ── 4) Configurar dirección del servidor destino ──────────────────────────
    sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port   = htons(SERVER_PORT);

    // inet_pton convierte la IP en formato texto a binario de red
    if (inet_pton(AF_INET, SERVER_IP, &servAddr.sin_addr) <= 0) {
        std::cerr << "[ERROR] IP inválida: " << SERVER_IP << "\n";
        close(sockfd);
        return EXIT_FAILURE;
    }

    // ── 5) Enviar saludo con sendto() ────────────────────────────────────────
    // sendto() no requiere conexión previa (UDP es connectionless).
    std::cout << "[CLIENT] Enviando: \"" << MSG_HELLO << "\" → "
              << SERVER_IP << ":" << SERVER_PORT << "\n";

    ssize_t bytesSent = sendto(
        sockfd,
        MSG_HELLO, std::strlen(MSG_HELLO),
        0,
        reinterpret_cast<sockaddr*>(&servAddr),
        sizeof(servAddr)
    );

    if (bytesSent < 0) {
        perror("[ERROR] sendto()");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // ── 6) Esperar respuesta del servidor ────────────────────────────────────
    // recvfrom() retornará -1 con errno=EAGAIN si se supera el timeout configurado.
    char buffer[BUFFER_SIZE] = {};
    sockaddr_in fromAddr{};
    socklen_t   fromLen = sizeof(fromAddr);

    ssize_t bytesRecv = recvfrom(
        sockfd,
        buffer, BUFFER_SIZE - 1,
        0,
        reinterpret_cast<sockaddr*>(&fromAddr),
        &fromLen
    );

    if (bytesRecv < 0) {
        perror("[ERROR] recvfrom() — timeout o error");
        close(sockfd);
        return EXIT_FAILURE;
    }

    buffer[bytesRecv] = '\0';
    std::cout << "[CLIENT] Respuesta recibida: \"" << buffer << "\"\n";

    // ── 7) Cerrar socket ─────────────────────────────────────────────────────
    close(sockfd);
    std::cout << "[CLIENT] Socket cerrado. Fin.\n";
    return EXIT_SUCCESS;
}
