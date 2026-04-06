#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define MPU6000_ADDR 0x68
#define ACCEL_XOUT_H 0x3B
#define PWR_MGMT_1   0x6B

#define COLOR_SENSOR_ADDR 0x29 
#define ENABLE_REG   0x80
#define ENABLE_VALUE 0x03
#define CDATA_REG    0x94

int main() {
    printf("Iniciando Sensores I2C en Raspberry Pi (Linux Embedded)...\n");

    int fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) {
        perror("Error al abrir el bus I2C");
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, MPU6000_ADDR) < 0) {
        perror("Error en ioctl MPU6000");
        return 1;
    }
    char wake[2] = {PWR_MGMT_1, 0x00};
    if(write(fd, wake, 2) != 2) {
        perror("Error al despertar el MPU6000");
    } else {
        printf("MPU6000 (Acelerómetro) despertado con éxito.\n");
    }
    usleep(100000);

    if (ioctl(fd, I2C_SLAVE, COLOR_SENSOR_ADDR) < 0) {
        perror("Error en ioctl Sensor de Color");
        return 1;
    }
    char enable_cmd[2] = {ENABLE_REG, ENABLE_VALUE};
    if (write(fd, enable_cmd, 2) != 2) {
        perror("Error al habilitar el sensor de color");
    } else {
        printf("Sensor de color habilitado con éxito.\n");
    }
    usleep(3000); 

    unsigned char write_bytes_mpu[1] = {ACCEL_XOUT_H};
    unsigned char read_bytes_mpu[6];
    
    unsigned char reg_color[1] = {CDATA_REG};
    unsigned char data_color[8];

    struct i2c_rdwr_ioctl_data packets;
    struct i2c_msg messages[2];

    printf("\n--- Comenzando lectura continua de datos ---\n");

    while (1) {
        messages[0].addr = MPU6000_ADDR;
        messages[0].flags = 0;
        messages[0].len = 1;
        messages[0].buf = write_bytes_mpu;

        messages[1].addr = MPU6000_ADDR;
        messages[1].flags = I2C_M_RD;
        messages[1].len = 6;
        messages[1].buf = read_bytes_mpu;

        packets.msgs = messages;
        packets.nmsgs = 2;

        if (ioctl(fd, I2C_RDWR, &packets) < 0) {
            perror("Error leyendo MPU6000");
        } else {
            short ax = (short)((read_bytes_mpu[0] << 8) | read_bytes_mpu[1]);
            short ay = (short)((read_bytes_mpu[2] << 8) | read_bytes_mpu[3]);
            short az = (short)((read_bytes_mpu[4] << 8) | read_bytes_mpu[5]);
            printf("Accel -> X: %6d | Y: %6d | Z: %6d\n", ax, ay, az);
        }

        if (write(fd, reg_color, 1) == 1) {
            if (read(fd, data_color, 8) == 8) {
                int clear = (data_color[1] << 8) | data_color[0];
                int red   = (data_color[3] << 8) | data_color[2];
                int green = (data_color[5] << 8) | data_color[4];
                int blue  = (data_color[7] << 8) | data_color[6];
                printf("Color -> R: %5d | G: %5d | B: %5d | Luz: %5d\n", red, green, blue, clear);
            }
        }

        printf("----------------------------------------------------\n");
        usleep(500000);
    }

    close(fd);
    return 0;
}
