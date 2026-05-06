// =============================================================================
// IOTServer_v1.cpp — Servidor UDP básico (Fase 1: Hello World)
// Plataforma: Ubuntu x86_64
//
// COMPILACIÓN:
//   g++ -std=c++17 -Wall -Wextra -o IOTServer_v1 IOTServer_v1.cpp
//
// USO:
//   ./IOTServer_v1 <puerto_udp>
//   Ejemplo: ./IOTServer_v1 5000
// =============================================================================

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <string>

// Cabeceras POSIX para sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// ─── Constantes de protocolo ────────────────────────────────────────────────
static constexpr int    BUFFER_SIZE   = 256;
static constexpr char   MSG_EXPECTED[]= "Hello Server";
static constexpr char   MSG_OK[]      = "Hello RPI";
static constexpr char   MSG_WRONG[]   = "Wrong Message";

// ─── main ───────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    // 1) Validar argumentos de línea de comandos (REGLA: cero hardcoding)
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <puerto_udp>\n";
        return EXIT_FAILURE;
    }

    const int PORT = std::atoi(argv[1]);
    if (PORT <= 0 || PORT > 65535) {
        std::cerr << "[ERROR] Puerto inválido: " << argv[1] << "\n";
        return EXIT_FAILURE;
    }

    // ── 2) Crear socket UDP ──────────────────────────────────────────────────
    // AF_INET  = IPv4
    // SOCK_DGRAM = protocolo UDP (sin conexión)
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[ERROR] socket()");
        return EXIT_FAILURE;
    }
    std::cout << "[SERVER] Socket UDP creado (fd=" << sockfd << ")\n";

    // ── 3) Configurar dirección local y hacer bind ───────────────────────────
    // El bind asocia el socket a una IP y puerto locales para escuchar.
    sockaddr_in servAddr{};
    servAddr.sin_family      = AF_INET;          // IPv4
    servAddr.sin_port        = htons(PORT);      // Puerto en network byte-order
    servAddr.sin_addr.s_addr = INADDR_ANY;       // Aceptar conexiones en todas las NICs

    if (bind(sockfd, reinterpret_cast<sockaddr*>(&servAddr), sizeof(servAddr)) < 0) {
        perror("[ERROR] bind()");
        close(sockfd);
        return EXIT_FAILURE;
    }
    std::cout << "[SERVER] Escuchando en puerto " << PORT << "...\n";

    // ── 4) Esperar un único datagrama del cliente ────────────────────────────
    char buffer[BUFFER_SIZE] = {};
    sockaddr_in clientAddr{};
    socklen_t   clientLen = sizeof(clientAddr);

    // recvfrom() bloquea hasta recibir un paquete UDP.
    // Rellena clientAddr con la IP/puerto del remitente (necesario para responder).
    ssize_t bytesRecv = recvfrom(
        sockfd,
        buffer, BUFFER_SIZE - 1,
        0,                                               // flags
        reinterpret_cast<sockaddr*>(&clientAddr),        // dirección del cliente
        &clientLen
    );

    if (bytesRecv < 0) {
        perror("[ERROR] recvfrom()");
        close(sockfd);
        return EXIT_FAILURE;
    }
    buffer[bytesRecv] = '\0';   // Asegurar terminación de cadena

    // Mostrar información del cliente
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
    std::cout << "[SERVER] Mensaje recibido de " << clientIP
              << ":" << ntohs(clientAddr.sin_port)
              << " → \"" << buffer << "\"\n";

    // ── 5) Validar mensaje y construir respuesta ─────────────────────────────
    const char* response = (std::strcmp(buffer, MSG_EXPECTED) == 0)
                           ? MSG_OK
                           : MSG_WRONG;

    std::cout << "[SERVER] Respuesta enviada: \"" << response << "\"\n";

    // ── 6) Enviar respuesta con sendto() ─────────────────────────────────────
    // sendto() envía un datagrama UDP a la dirección del cliente capturada en recvfrom().
    ssize_t bytesSent = sendto(
        sockfd,
        response, std::strlen(response),
        0,
        reinterpret_cast<sockaddr*>(&clientAddr),
        clientLen
    );

    if (bytesSent < 0) {
        perror("[ERROR] sendto()");
    }

    // ── 7) Cerrar socket (protocolo: single exchange) ────────────────────────
    close(sockfd);
    std::cout << "[SERVER] Socket cerrado. Fin.\n";
    return EXIT_SUCCESS;
}
