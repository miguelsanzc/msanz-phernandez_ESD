// =============================================================================
// IOTServer_Final.cpp — Servidor UDP Final (Fases 2 y 3)
// Plataforma: Ubuntu x86_64
//
// Funcionalidad:
//   - Recibe bloques de 10 muestras (ax, ay, az, r, g, b) del cliente.
//   - Responde con ACK inmediato tras cada paquete de 10s.
//   - Acumula hasta 60 muestras por variable (6 paquetes × 10 muestras).
//   - Cada 60 muestras (1 minuto) calcula y muestra:
//       Media, Máximo, Mínimo, Desviación Estándar y color dominante RGB.
//
// COMPILACIÓN:
//   g++ -std=c++17 -Wall -Wextra -o IOTServer_Final IOTServer_Final.cpp -lm
//
// USO:
//   ./IOTServer_Final <puerto_udp>
//   Ejemplo: ./IOTServer_Final 5000
// =============================================================================

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <string>
#include <array>
#include <algorithm>
#include <numeric>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// =============================================================================
// ─── Estructura del paquete de datos (compartida con el cliente) ─────────────
// =============================================================================
// Cada paquete contiene exactamente SAMPLES_PER_PACKET muestras.
// Se usa una struct binaria para evitar dependencias de JSON (REGLA 5).
// __attribute__((packed)) garantiza que no haya padding entre campos,
// asegurando que el layout en memoria sea idéntico en cliente y servidor.
// =============================================================================
static constexpr int SAMPLES_PER_PACKET = 10;   // Muestras por paquete UDP (10s a 1S/s)
static constexpr int PACKETS_PER_MINUTE = 6;    // 6 paquetes × 10s = 60s
static constexpr int BUFFER_SIZE_STATS  = SAMPLES_PER_PACKET * PACKETS_PER_MINUTE; // 60

#pragma pack(push, 1)   // Deshabilitar padding para garantizar layout binario
struct SensorSample {
    float ax;   // Aceleración X en g
    float ay;   // Aceleración Y en g
    float az;   // Aceleración Z en g
    int   r;    // Canal Rojo normalizado [0-255]
    int   g;    // Canal Verde normalizado [0-255]
    int   b;    // Canal Azul normalizado [0-255]
};

struct DataPacket {
    uint32_t     seq;                              // Número de secuencia del paquete
    uint32_t     count;                            // Número de muestras válidas
    SensorSample samples[SAMPLES_PER_PACKET];      // Arreglo de 10 muestras
};

struct AckPacket {
    uint32_t seq;          // Eco del número de secuencia recibido
    char     msg[16];      // Mensaje de confirmación textual
};
#pragma pack(pop)

// =============================================================================
// ─── Clase de estadísticas por canal ────────────────────────────────────────
// =============================================================================
// Mantiene un buffer circular de hasta BUFFER_SIZE_STATS valores flotantes
// y calcula estadísticas descriptivas cuando se llena (1 minuto de datos).
// =============================================================================
template<int N>
class StatsBuffer {
public:
    StatsBuffer() : count_(0) {}

    // Agrega un valor al buffer. Retorna true si el buffer está lleno (minuto completo).
    bool push(float value) {
        if (count_ < N) {
            data_[count_++] = value;
        }
        return (count_ == N);
    }

    // Resetea el buffer para el próximo minuto de acumulación.
    void reset() { count_ = 0; }

    int size() const { return count_; }

    // ── Cálculo de estadísticas ──────────────────────────────────────────────
    float mean() const {
        if (count_ == 0) return 0.0f;
        float sum = 0.0f;
        for (int i = 0; i < count_; ++i) sum += data_[i];
        return sum / count_;
    }

    float maximum() const {
        if (count_ == 0) return 0.0f;
        float m = data_[0];
        for (int i = 1; i < count_; ++i) if (data_[i] > m) m = data_[i];
        return m;
    }

    float minimum() const {
        if (count_ == 0) return 0.0f;
        float m = data_[0];
        for (int i = 1; i < count_; ++i) if (data_[i] < m) m = data_[i];
        return m;
    }

    // Desviación estándar poblacional
    float stddev() const {
        if (count_ < 2) return 0.0f;
        float mu  = mean();
        float var = 0.0f;
        for (int i = 0; i < count_; ++i) {
            float diff = data_[i] - mu;
            var += diff * diff;
        }
        return std::sqrt(var / count_);
    }

private:
    std::array<float, N> data_;
    int count_;
};

// =============================================================================
// ─── Función: detectar color dominante ──────────────────────────────────────
// =============================================================================
static std::string dominantColor(float meanR, float meanG, float meanB)
{
    // Umbral para considerar "alta" saturación de un canal
    static constexpr float THRESHOLD = 100.0f;

    // Determinar canal dominante comparando medias
    if (meanR > meanG && meanR > meanB) {
        return (meanR > THRESHOLD) ? "ROJO" : "Rojizo";
    } else if (meanG > meanR && meanG > meanB) {
        return (meanG > THRESHOLD) ? "VERDE" : "Verdoso";
    } else if (meanB > meanR && meanB > meanG) {
        return (meanB > THRESHOLD) ? "AZUL" : "Azulado";
    } else if (meanR > THRESHOLD && meanG > THRESHOLD && meanB < THRESHOLD) {
        return "AMARILLO";
    } else if (meanR > THRESHOLD && meanB > THRESHOLD && meanG < THRESHOLD) {
        return "MAGENTA";
    } else if (meanG > THRESHOLD && meanB > THRESHOLD && meanR < THRESHOLD) {
        return "CIAN";
    } else if (meanR > THRESHOLD && meanG > THRESHOLD && meanB > THRESHOLD) {
        return "BLANCO";
    } else {
        return "OSCURO/INDEFINIDO";
    }
}

// =============================================================================
// ─── Función: imprimir estadísticas de un canal ──────────────────────────────
// =============================================================================
template<int N>
static void printStats(const char* label, StatsBuffer<N>& buf)
{
    std::printf("  %-6s → Media=%7.4f  Max=%7.4f  Min=%7.4f  StdDev=%7.4f\n",
                label,
                buf.mean(),
                buf.maximum(),
                buf.minimum(),
                buf.stddev());
}

// =============================================================================
// ─── main ────────────────────────────────────────────────────────────────────
// =============================================================================
int main(int argc, char* argv[])
{
    // 1) Validar argumentos
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <puerto_udp>\n";
        return EXIT_FAILURE;
    }

    const int PORT = std::atoi(argv[1]);
    if (PORT <= 0 || PORT > 65535) {
        std::cerr << "[ERROR] Puerto inválido.\n";
        return EXIT_FAILURE;
    }

    // ── 2) Crear socket UDP ──────────────────────────────────────────────────
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[ERROR] socket()");
        return EXIT_FAILURE;
    }

    // Opción SO_REUSEADDR: permite reiniciar el servidor rápidamente tras un cierre
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ── 3) Bind al puerto local ───────────────────────────────────────────────
    sockaddr_in servAddr{};
    servAddr.sin_family      = AF_INET;
    servAddr.sin_port        = htons(PORT);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, reinterpret_cast<sockaddr*>(&servAddr), sizeof(servAddr)) < 0) {
        perror("[ERROR] bind()");
        close(sockfd);
        return EXIT_FAILURE;
    }

    std::cout << "╔══════════════════════════════════════════════════╗\n"
              << "║        IOT SERVER FINAL — Puerto " << PORT << "          ║\n"
              << "╚══════════════════════════════════════════════════╝\n"
              << "[SERVER] Esperando paquetes de datos...\n\n";

    // ── 4) Buffers de estadísticas (60 muestras por variable) ─────────────────
    // Aceleración (en g, valor float)
    StatsBuffer<BUFFER_SIZE_STATS> bufAX, bufAY, bufAZ;
    // Color (valores 0-255, almacenados como float para reutilizar la clase)
    StatsBuffer<BUFFER_SIZE_STATS> bufR, bufG, bufB;

    // Contador de paquetes recibidos en la ventana de 1 minuto
    int packetCount = 0;

    // ── 5) Bucle principal de recepción ───────────────────────────────────────
    while (true)
    {
        DataPacket  packet{};
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);

        // ── 5a) recvfrom: bloquea hasta recibir un paquete completo ───────────
        ssize_t bytesRecv = recvfrom(
            sockfd,
            &packet, sizeof(packet),
            0,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &clientLen
        );

        if (bytesRecv < 0) {
            perror("[WARN] recvfrom()");
            continue;   // Reintentar en el siguiente ciclo
        }
        if (bytesRecv != static_cast<ssize_t>(sizeof(DataPacket))) {
            std::cerr << "[WARN] Tamaño de paquete inesperado ("
                      << bytesRecv << " bytes). Descartando.\n";
            continue;
        }

        // Obtener IP del cliente para logging
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));

        std::cout << "[SERVER] Paquete #" << packet.seq
                  << " recibido de " << clientIP
                  << " (" << packet.count << " muestras)\n";

        // ── 5b) Enviar ACK inmediato (antes de procesar) ──────────────────────
        // El ACK se envía siempre al recibir un paquete de 10s (REGLA 4).
        AckPacket ack{};
        ack.seq = packet.seq;
        std::strncpy(ack.msg, "ACK", sizeof(ack.msg) - 1);

        sendto(
            sockfd,
            &ack, sizeof(ack),
            0,
            reinterpret_cast<sockaddr*>(&clientAddr),
            clientLen
        );
        std::cout << "[SERVER] ACK enviado para seq=" << packet.seq << "\n";

        // ── 5c) Acumular muestras en los buffers de estadísticas ──────────────
        for (uint32_t i = 0; i < packet.count; ++i) {
            const SensorSample& s = packet.samples[i];
            bufAX.push(s.ax);
            bufAY.push(s.ay);
            bufAZ.push(s.az);
            bufR.push(static_cast<float>(s.r));
            bufG.push(static_cast<float>(s.g));
            bufB.push(static_cast<float>(s.b));
        }

        packetCount++;

        // ── 5d) Calcular estadísticas cada 6 paquetes (= 60 muestras = 1 min) ─
        if (packetCount >= PACKETS_PER_MINUTE)
        {
            std::cout << "\n"
                      << "╔══════════════════════════════════════════════════╗\n"
                      << "║         ESTADÍSTICAS DEL ÚLTIMO MINUTO           ║\n"
                      << "╠══════════════════════════════════════════════════╣\n";

            // Acelerómetro
            std::cout << "  ACELERÓMETRO (g):\n";
            printStats("ax", bufAX);
            printStats("ay", bufAY);
            printStats("az", bufAZ);

            // Color
            std::cout << "  COLOR (0-255):\n";
            printStats("R", bufR);
            printStats("G", bufG);
            printStats("B", bufB);

            // Color dominante inferido de las medias
            std::string color = dominantColor(bufR.mean(), bufG.mean(), bufB.mean());
            std::cout << "  Color dominante detectado: " << color << "\n";

            std::cout << "╚══════════════════════════════════════════════════╝\n\n";

            // Resetear buffers y contador para el próximo minuto
            bufAX.reset(); bufAY.reset(); bufAZ.reset();
            bufR.reset();  bufG.reset();  bufB.reset();
            packetCount = 0;
        }
    }

    // (Inalcanzable en operación normal; aquí por completitud)
    close(sockfd);
    return EXIT_SUCCESS;
}
