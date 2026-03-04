#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>


#define MPU6050_ADDR 0x68
#define ACCEL_XOUT_H 0x3B

int main() {
    printf("Iniciando MPU-6050...\n");

    int dev = 1; // Bus I2C 1
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

    const int w_len = 1;
    const int r_len = 6; 
    
    char write_bytes[w_len] = {ACCEL_XOUT_H}; 
    char read_bytes[r_len];

    struct i2c_rdwr_ioctl_data packets;
    struct i2c_msg messages[2];

    while (true) {
        // Configurar mensaje de escritura (indicar registro)
        messages[0].addr = addr;
        messages[0].flags = 0; // write
        messages[0].len = w_len;
        messages[0].buf = (unsigned char*)write_bytes;

        // Configurar mensaje de lectura (obtener datos)
        messages[1].addr = addr;
        messages[1].flags = I2C_M_RD; // read
        messages[1].len = r_len;
        messages[1].buf = (unsigned char*)read_bytes;

        // Construir lista de paquetes para la transacción
        packets.msgs = messages;
        packets.nmsgs = 2;

        // Enviar vía ioctl I2C_RDWR
        if (ioctl(fd, I2C_RDWR, &packets) < 0) {
            perror("Error en I2C_RDWR");
            break;
        }

        short ax = (read_bytes[0] << 8) | (unsigned char)read_bytes[1];
        short ay = (read_bytes[2] << 8) | (unsigned char)read_bytes[3];
        short az = (read_bytes[4] << 8) | (unsigned char)read_bytes[5];

        printf("Lectura -> AccelX: %d | AccelY: %d | AccelZ: %d\n m/s2", ax, ay, az);

        // Frecuencia de muestreo (100ms)
        usleep(100000); 
    }
    close(fd);
    printf("Sistema cerrado correctamente.\n");
    return 0;
}
