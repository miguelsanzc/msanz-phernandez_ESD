// =============================================================================
// IOTClient_Final_FIXED.cpp — Cliente UDP Final CORREGIDO
// Plataforma: Raspberry Pi (AArch64)
//
// CORRECCIONES:
//   - mosquitto_pub se lanza en hilo separado (no bloquea el bucle 1Hz)
//   - Bucle principal estrictamente a 1 S/s con clock_nanosleep ABSTIME
//   - Impresión por pantalla cada segundo garantizada
//
// COMPILACIÓN CRUZADA:
//   source /home/ubuntu/Documents/rpi/build/host/environment-setup
//   $CXX -std=c++17 -Wall -Wextra -pthread \
//        -o IOTClient_Final_FIXED IOTClient_Final_FIXED.cpp -lm
//
// USO:
//   ./IOTClient_Final_FIXED <ip_servidor> <puerto_udp> <ip_mqtt> <token>
//   Ejemplo:
//   ./IOTClient_Final_FIXED 192.168.1.100 5000 192.168.1.100 TOKEN123
// =============================================================================

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <string>
#include <array>
#include <stdexcept>
#include <algorithm>
#include <thread>       // std::thread para lanzar mosquitto_pub sin bloquear

// Cabeceras POSIX / Linux
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

// Cabeceras I2C Linux
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <fcntl.h>

// =============================================================================
// ─── I2CBus ──────────────────────────────────────────────────────────────────
// =============================================================================
class I2CBus {
public:
    explicit I2CBus(const std::string& device = "/dev/i2c-1") {
        fd_ = open(device.c_str(), O_RDWR);
        if (fd_ < 0) throw std::runtime_error("No se pudo abrir " + device);
    }
    ~I2CBus() { if (fd_ >= 0) close(fd_); }
    I2CBus(const I2CBus&) = delete;
    I2CBus& operator=(const I2CBus&) = delete;

    void writeReg(uint8_t dev_addr, uint8_t reg, uint8_t value) const {
        if (ioctl(fd_, I2C_SLAVE, dev_addr) < 0)
            throw std::runtime_error("ioctl I2C_SLAVE falló");
        std::array<uint8_t, 2> buf = {reg, value};
        if (write(fd_, buf.data(), 2) != 2)
            throw std::runtime_error("write I2C falló");
    }

    void readReg(uint8_t dev_addr, uint8_t reg, uint8_t* out, int len) const {
        i2c_msg msgs[2];
        i2c_rdwr_ioctl_data pkt;
        msgs[0] = {dev_addr, 0, 1, &reg};
        msgs[1] = {dev_addr, I2C_M_RD, (uint16_t)len, out};
        pkt.msgs  = msgs;
        pkt.nmsgs = 2;
        if (ioctl(fd_, I2C_RDWR, &pkt) < 0)
            throw std::runtime_error("I2C_RDWR falló");
    }

private:
    int fd_ = -1;
};

// =============================================================================
// ─── MPU6000 ─────────────────────────────────────────────────────────────────
// =============================================================================
class MPU6000 {
public:
    static constexpr uint8_t ADDR          = 0x68;
    static constexpr uint8_t REG_PWR_MGMT  = 0x6B;
    static constexpr uint8_t REG_ACCEL_CFG = 0x1C;
    static constexpr uint8_t REG_ACCEL_OUT = 0x3B;
    static constexpr float   LSB_PER_G     = 16384.0f;

    struct Data {
        int16_t ax_raw, ay_raw, az_raw;
        float   ax_g, ay_g, az_g;
    };

    explicit MPU6000(const I2CBus& bus) : bus_(bus) {
        bus_.writeReg(ADDR, REG_PWR_MGMT,  0x00); usleep(10000);
        bus_.writeReg(ADDR, REG_ACCEL_CFG, 0x00); usleep(10000);
    }

    Data read() const {
        std::array<uint8_t, 6> raw{};
        bus_.readReg(ADDR, REG_ACCEL_OUT, raw.data(), 6);
        Data d;
        d.ax_raw = static_cast<int16_t>((raw[0] << 8) | raw[1]);
        d.ay_raw = static_cast<int16_t>((raw[2] << 8) | raw[3]);
        d.az_raw = static_cast<int16_t>((raw[4] << 8) | raw[5]);
        d.ax_g   = d.ax_raw / LSB_PER_G;
        d.ay_g   = d.ay_raw / LSB_PER_G;
        d.az_g   = d.az_raw / LSB_PER_G;
        return d;
    }

private:
    const I2CBus& bus_;
};

// =============================================================================
// ─── TCS34725 ────────────────────────────────────────────────────────────────
// =============================================================================
class TCS34725 {
public:
    static constexpr uint8_t ADDR       = 0x29;
    static constexpr uint8_t CMD        = 0x80;
    static constexpr uint8_t REG_ENABLE = CMD | 0x00;
    static constexpr uint8_t REG_CDATA  = CMD | 0x14;

    struct Data { int clear, red, green, blue, r, g, b; };

    explicit TCS34725(const I2CBus& bus) : bus_(bus) {
        bus_.writeReg(ADDR, REG_ENABLE, 0x01); usleep(3000);
        bus_.writeReg(ADDR, REG_ENABLE, 0x03); usleep(700000);
    }

    Data read() const {
        std::array<uint8_t, 8> raw{};
        bus_.readReg(ADDR, REG_CDATA, raw.data(), 8);
        Data d;
        d.clear = (raw[1] << 8) | raw[0];
        d.red   = (raw[3] << 8) | raw[2];
        d.green = (raw[5] << 8) | raw[4];
        d.blue  = (raw[7] << 8) | raw[6];
        const int denom = (d.clear == 0) ? 1 : d.clear;
        d.r = std::min((d.red   * 255) / denom, 255);
        d.g = std::min((d.green * 255) / denom, 255);
        d.b = std::min((d.blue  * 255) / denom, 255);
        return d;
    }

private:
    const I2CBus& bus_;
};

// =============================================================================
// ─── Estructura del paquete UDP (idéntica al servidor) ───────────────────────
// =============================================================================
static constexpr int SAMPLES_PER_PACKET = 10;

#pragma pack(push, 1)
struct SensorSample { float ax, ay, az; int r, g, b; };
struct DataPacket   { uint32_t seq, count; SensorSample samples[SAMPLES_PER_PACKET]; };
struct AckPacket    { uint32_t seq; char msg[16]; };
#pragma pack(pop)

// =============================================================================
// ─── Publicar MQTT en hilo separado (NO bloquea el bucle principal) ──────────
// =============================================================================
// Se lanza std::thread con detach() para que corra en paralelo.
// El hilo ejecuta mosquitto_pub y termina solo, sin bloquear el ciclo 1Hz.
// =============================================================================
static void publishMQTT_async(const std::string& mqttHost,
                               const std::string& token,
                               float ax, float ay, float az,
                               int r, int g, int b)
{
    // Construir comando completo
    char cmd[512];
    std::snprintf(cmd, sizeof(cmd),
        "mosquitto_pub -q 1 -h %s -p 1883 "
        "-t \"v1/devices/me/telemetry\" "
        "-u %s "
        "-m '{\"ax\":%.4f,\"ay\":%.4f,\"az\":%.4f,"
        "\"r\":%d,\"g\":%d,\"b\":%d}' > /dev/null 2>&1",
        mqttHost.c_str(), token.c_str(),
        ax, ay, az, r, g, b);

    // Lanzar en hilo separado con detach
    // detach() = el hilo corre de forma independiente y libera recursos al acabar
    std::thread([cmd_str = std::string(cmd)]() {
        std::system(cmd_str.c_str());
    }).detach();
}

// =============================================================================
// ─── Utilidad: avanzar timespec exactamente 1 segundo ────────────────────────
// =============================================================================
static void timespecAdd(struct timespec& ts, long ns) {
    ts.tv_nsec += ns;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
}

// =============================================================================
// ─── main ────────────────────────────────────────────────────────────────────
// =============================================================================
int main(int argc, char* argv[])
{
    // ── 1) Validar argumentos ─────────────────────────────────────────────────
    if (argc != 5) {
        std::cerr << "Uso: " << argv[0]
                  << " <ip_servidor> <puerto_udp> <ip_mqtt> <token>\n";
        std::cerr << "Ej:  " << argv[0]
                  << " 192.168.1.100 5000 192.168.1.100 TOKEN123\n";
        return EXIT_FAILURE;
    }

    const std::string SERVER_IP  (argv[1]);
    const int         SERVER_PORT = std::atoi(argv[2]);
    const std::string MQTT_HOST  (argv[3]);
    const std::string TB_TOKEN   (argv[4]);

    // ── 2) Inicializar sensores I2C ───────────────────────────────────────────
    std::cout << "[CLIENT] Inicializando sensores I2C...\n";
    I2CBus   bus("/dev/i2c-1");
    MPU6000  imu(bus);
    TCS34725 colorSensor(bus);
    std::cout << "[CLIENT] Sensores OK.\n";

    // ── 3) Crear socket UDP ───────────────────────────────────────────────────
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("[ERROR] socket()"); return EXIT_FAILURE; }

    // ── 4) Timeout ACK = 200ms (muy corto para no perder tiempo en el bucle) ──
    // Se reduce a 200ms porque el ACK es local en la red y debe llegar rápido.
    // Si no llega en 200ms, continuamos sin bloquear el siguiente segundo.
    timeval tv{};
    tv.tv_sec  = 0;
    tv.tv_usec = 200000;   // 200 ms
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // ── 5) Dirección del servidor UDP ─────────────────────────────────────────
    sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port   = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP.c_str(), &servAddr.sin_addr);

    std::cout << "[CLIENT] Servidor UDP : " << SERVER_IP << ":" << SERVER_PORT << "\n";
    std::cout << "[CLIENT] MQTT broker  : " << MQTT_HOST << ":1883\n";
    std::cout << "[CLIENT] Token        : " << TB_TOKEN  << "\n";
    std::cout << "[CLIENT] Iniciando bucle 1 S/s...\n\n";

    // ── 6) Variables del bucle ────────────────────────────────────────────────
    DataPacket packet{};
    packet.seq   = 0;
    packet.count = 0;
    uint32_t globalSeq = 0;
    int      sampleIdx = 0;

    // ── 7) Tomar el tiempo inicial absoluto para TIMER_ABSTIME ───────────────
    // clock_gettime obtiene el tiempo actual del reloj monotónico.
    // nextTick es el instante absoluto en el que debe ocurrir el siguiente ciclo.
    struct timespec nextTick{};
    clock_gettime(CLOCK_MONOTONIC, &nextTick);

    // ── 8) Bucle principal estricto a 1 Hz ────────────────────────────────────
    while (true)
    {
        // Calcular el instante del próximo tick ANTES de hacer nada
        // Así aunque el procesamiento tarde, el siguiente tick es siempre
        // exactamente 1 segundo después del anterior (no se acumula error)
        timespecAdd(nextTick, 1000000000L);

        // ── 8a) Leer sensores I2C ─────────────────────────────────────────────
        MPU6000::Data  imuData;
        TCS34725::Data colorData;

        try {
            imuData   = imu.read();
            colorData = colorSensor.read();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[ERROR] Sensor: %s\n", e.what());
            // Dormir hasta el siguiente tick y continuar
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &nextTick, nullptr);
            continue;
        }

        // ── 8b) Imprimir muestra en consola (cada segundo) ────────────────────
        std::printf("[S%02d/%02d] ax=%7.4fg  ay=%7.4fg  az=%7.4fg  "
                    "| R=%3d  G=%3d  B=%3d\n",
                    sampleIdx + 1, SAMPLES_PER_PACKET,
                    imuData.ax_g, imuData.ay_g, imuData.az_g,
                    colorData.r, colorData.g, colorData.b);
        std::fflush(stdout);   // Forzar flush inmediato para ver en tiempo real

        // ── 8c) Publicar MQTT en hilo separado (no bloquea el bucle) ──────────
        // La función retorna INMEDIATAMENTE lanzando un hilo en background.
        // mosquitto_pub se ejecuta en paralelo mientras el bucle continúa.
        publishMQTT_async(MQTT_HOST, TB_TOKEN,
                          imuData.ax_g, imuData.ay_g, imuData.az_g,
                          colorData.r, colorData.g, colorData.b);

        // ── 8d) Guardar muestra en el paquete UDP ─────────────────────────────
        SensorSample& s = packet.samples[sampleIdx];
        s.ax = imuData.ax_g;
        s.ay = imuData.ay_g;
        s.az = imuData.az_g;
        s.r  = colorData.r;
        s.g  = colorData.g;
        s.b  = colorData.b;
        sampleIdx++;
        packet.count = static_cast<uint32_t>(sampleIdx);

        // ── 8e) Cada 10 muestras: enviar paquete UDP y esperar ACK ────────────
        if (sampleIdx >= SAMPLES_PER_PACKET)
        {
            packet.seq = globalSeq++;

            std::printf("\n[UDP TX] seq=%u  %u muestras → %s:%d\n",
                        packet.seq, packet.count,
                        SERVER_IP.c_str(), SERVER_PORT);

            // Enviar struct binaria al servidor
            ssize_t sent = sendto(sockfd,
                                  &packet, sizeof(packet),
                                  0,
                                  reinterpret_cast<sockaddr*>(&servAddr),
                                  sizeof(servAddr));
            if (sent < 0) perror("[WARN] sendto()");

            // Esperar ACK con timeout de 200ms
            AckPacket   ack{};
            sockaddr_in fromAddr{};
            socklen_t   fromLen = sizeof(fromAddr);

            ssize_t ackBytes = recvfrom(sockfd,
                                        &ack, sizeof(ack),
                                        0,
                                        reinterpret_cast<sockaddr*>(&fromAddr),
                                        &fromLen);
            if (ackBytes < 0)
                std::printf("[WARN] ACK no recibido para seq=%u\n\n", packet.seq);
            else
                std::printf("[ACK] seq=%u  \"%s\"\n\n", ack.seq, ack.msg);

            // Reset buffer para el siguiente bloque de 10 muestras
            sampleIdx    = 0;
            packet.count = 0;
        }

        // ── 8f) Dormir exactamente hasta el siguiente tick absoluto ───────────
        // TIMER_ABSTIME: duerme hasta nextTick exacto (no relativo).
        // Compensa automáticamente el tiempo gastado en leer sensores,
        // imprimir, lanzar el hilo MQTT y enviar UDP.
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &nextTick, nullptr);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
