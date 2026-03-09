#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <csignal> // Para capturar Ctrl+C

#define MPU6050_ADDR 0x68
#define ACCEL_XOUT_H 0x3B
#define PWR_MGMT_1   0x6B

// Variable global para controlar el bucle
volatile sig_atomic_t running = 1;

// Función que se ejecuta al presionar Ctrl+C
void handle_sigint(int sig) {
    running = 0;
}

int main() {
    // Registrar el manejador de la señal SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    printf("Iniciando MPU-6050. Presiona Ctrl+C para finalizar y ver los resultados...\n");

    int dev = 1;
    int addr = MPU6050_ADDR;
    char i2cFile[15];
    sprintf(i2cFile, "/dev/i2c-%d", dev);
    int fd = open(i2cFile, O_RDWR);
    
    if (fd < 0) {
        perror("Error al abrir el bus I2C");
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        perror("Error en ioctl I2C_SLAVE");
        return 1;
    }

    // Despertar al sensor
    char wake_buf[2] = {PWR_MGMT_1, 0x00};
    if (write(fd, wake_buf, 2) != 2) {
        perror("Error al despertar el MPU-6050");
        close(fd);
        return 1;
    }
    usleep(100000); 

    const int w_len = 1;
    const int r_len = 6; 
    char write_bytes[w_len] = {ACCEL_XOUT_H}; 
    char read_bytes[r_len];

    struct i2c_rdwr_ioctl_data packets;
    struct i2c_msg messages[2];

    // Variables para el cálculo físico
    double dt = 1.0; // Intervalo de tiempo (1 segundo)
    double sum_ax = 0, sum_ay = 0, sum_az = 0; // Sumas de aceleración
    double vx = 0, vy = 0, vz = 0;             // Velocidades instantáneas
    double sum_vx = 0, sum_vy = 0, sum_vz = 0; // Sumas de velocidad
    long long count = 0;                       // Contador de muestras

    while (running) {
        messages[0].addr = addr;
        messages[0].flags = 0; 
        messages[0].len = w_len;
        messages[0].buf = (unsigned char*)write_bytes;

        messages[1].addr = addr;
        messages[1].flags = I2C_M_RD; 
        messages[1].len = r_len;
        messages[1].buf = (unsigned char*)read_bytes;

        packets.msgs = messages;
        packets.nmsgs = 2;

        if (ioctl(fd, I2C_RDWR, &packets) < 0) {
            perror("Error en I2C_RDWR");
            break;
        }

        short raw_ax = (read_bytes[0] << 8) | (unsigned char)read_bytes[1];
        short raw_ay = (read_bytes[2] << 8) | (unsigned char)read_bytes[3];
        short raw_az = (read_bytes[4] << 8) | (unsigned char)read_bytes[5];

        // 1. Convertir a m/s² (rango por defecto ±2g -> 16384 LSB/g)
        double ax = (raw_ax / 16384.0) * 9.81;
        double ay = (raw_ay / 16384.0) * 9.81;
        double az = (raw_az / 16384.0) * 9.81;

        // 2. Acumular aceleración
        sum_ax += ax;
        sum_ay += ay;
        sum_az += az;

        // 3. Integrar para obtener velocidad (v = v0 + a*t)
        vx += ax * dt;
        vy += ay * dt;
        vz += az * dt;

        // 4. Acumular velocidad
        sum_vx += vx;
        sum_vy += vy;
        sum_vz += vz;

        count++;

        printf("Actual -> aX: %.2f | aY: %.2f | aZ: %.2f (m/s²)\n", ax, ay, az);

        sleep(1); 
    }
    
    // --- CÁLCULO DE MEDIAS AL SALIR (Ctrl+C) ---
    printf("\n\n--- RESULTADOS FINALES ---\n");
    if (count > 0) {
        printf("Muestras totales: %lld\n", count);
        printf("Aceleración media (m/s²): X = %.3f | Y = %.3f | Z = %.3f\n", 
               sum_ax / count, sum_ay / count, sum_az / count);
        printf("Velocidad media (m/s):    X = %.3f | Y = %.3f | Z = %.3f\n", 
               sum_vx / count, sum_vy / count, sum_vz / count);
    } else {
        printf("No se tomaron suficientes muestras.\n");
    }

    close(fd);
    printf("Sistema cerrado correctamente.\n");
    return 0;
}
