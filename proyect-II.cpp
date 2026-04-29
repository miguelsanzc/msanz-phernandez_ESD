#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <array>
#include <algorithm>

class I2CBus {
public:
    explicit I2CBus(const std::string& device = "/dev/i2c-1") {
        fd_ = open(device.c_str(), O_RDWR);
        if (fd_ < 0) throw std::runtime_error("No se pudo abrir " + device);
    }

    ~I2CBus() {
        if (fd_ >= 0) close(fd_);
    }

    I2CBus(const I2CBus&) = delete;
    I2CBus& operator=(const I2CBus&) = delete;

    void writeReg(uint8_t dev_addr, uint8_t reg, uint8_t value) const {
        if (ioctl(fd_, I2C_SLAVE, dev_addr) < 0) throw std::runtime_error("ioctl falló");
        std::array<uint8_t, 2> buf = {reg, value};
        if (write(fd_, buf.data(), 2) != 2) throw std::runtime_error("write falló");
    }

    void readReg(uint8_t dev_addr, uint8_t reg, uint8_t* out, int len) const {
        i2c_msg msgs[2];
        i2c_rdwr_ioctl_data pkt;

        msgs[0] = {dev_addr, 0, 1, &reg};
        msgs[1] = {dev_addr, I2C_M_RD, (uint16_t)len, out};

        pkt.msgs  = msgs;
        pkt.nmsgs = 2;

        if (ioctl(fd_, I2C_RDWR, &pkt) < 0) throw std::runtime_error("I2C_RDWR falló");
    }

private:
    int fd_ = -1;
};

class MPU6000 {
public:
    static constexpr uint8_t ADDR = 0x68;
    static constexpr uint8_t REG_PWR_MGMT = 0x6B;
    static constexpr uint8_t REG_ACCEL_CFG = 0x1C;
    static constexpr uint8_t REG_ACCEL_OUT = 0x3B;
    static constexpr float LSB_PER_G = 16384.0f;

    struct Data {
        int16_t ax_raw, ay_raw, az_raw;
        float ax_g, ay_g, az_g;
    };

    explicit MPU6000(const I2CBus& bus) : bus_(bus) {
        bus_.writeReg(ADDR, REG_PWR_MGMT, 0x00);
        usleep(10000);
        bus_.writeReg(ADDR, REG_ACCEL_CFG, 0x00);
        usleep(10000);
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

    static void print(const Data& d) {
        printf("MPU-6000:\n");
        printf("  Raw: X: %d, Y: %d, Z: %d\n", d.ax_raw, d.ay_raw, d.az_raw);
        printf("  G:   X: %.3f, Y: %.3f, Z: %.3f\n", d.ax_g, d.ay_g, d.az_g);
    }

private:
    const I2CBus& bus_;
};

class TCS34725 {
public:
    static constexpr uint8_t ADDR = 0x29;
    static constexpr uint8_t CMD = 0x80;
    static constexpr uint8_t REG_ENABLE = CMD | 0x00;
    static constexpr uint8_t REG_CDATA = CMD | 0x14;

    struct Data {
        int clear, red, green, blue;
        int r, g, b;
    };

    explicit TCS34725(const I2CBus& bus) : bus_(bus) {
        bus_.writeReg(ADDR, REG_ENABLE, 0x01);
        usleep(3000);
        bus_.writeReg(ADDR, REG_ENABLE, 0x03);
        usleep(700000);
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

    static void print(const Data& d) {
        printf("TCS34725:\n");
        printf("  Clear: %d\n", d.clear);
        printf("  Raw:  R: %d, G: %d, B: %d\n", d.red, d.green, d.blue);
        printf("  Norm: R: %d, G: %d, B: %d\n", d.r, d.g, d.b);
    }

private:
    const I2CBus& bus_;
};

int main() {
    try {
        I2CBus bus;
        MPU6000 mpu(bus);
        TCS34725 tcs(bus);

        int loop = 0;
        while (true) {
            printf("--- Lectura %d ---\n", ++loop);

            try {
                MPU6000::print(mpu.read());
            } catch (const std::exception& e) {
                fprintf(stderr, "Error MPU: %s\n", e.what());
            }

            try {
                TCS34725::print(tcs.read());
            } catch (const std::exception& e) {
                fprintf(stderr, "Error TCS: %s\n", e.what());
            }

            printf("\n");
            sleep(1);
        }

    } catch (const std::exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    return 0;
}
