#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define COLOR_SENSOR_ADDR 0x29 

#define ENABLE_REG 0x80
#define ENABLE_VALUE 0x03
#define CDATA_REG 0x94

int main() {
    printf("Iniciando Sensor de Color...\n");

    int dev = 1;
    char i2cFile[15];
    sprintf(i2cFile, "/dev/i2c-%d", dev);
    int fd = open(i2cFile, O_RDWR);

    if (fd < 0) {
        perror("Error al abrir el bus I2C");
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, COLOR_SENSOR_ADDR) < 0) {
        perror("Error en ioctl I2C_SLAVE");
        close(fd);
        return 1;
    }

    char enable_cmd[2];
    enable_cmd[0] = ENABLE_REG;
    enable_cmd[1] = ENABLE_VALUE;

    if (write(fd, enable_cmd, 2) != 2) {
        perror("Error al habilitar el sensor de color");
        close(fd);
        return 1;
    }
    printf("Sensor de color habilitado con éxito.\n");
    
    usleep(3000); 

    char reg[1] = {CDATA_REG};
    char data[8];

    while (1) {
        if (write(fd, reg, 1) != 1) {
            perror("Error al escribir el registro de lectura");
            break;
        }

        if (read(fd, data, 8) != 8) {
            perror("Error al leer los datos");
            break;
        }

        int clear = (data[1] << 8) | data[0];
        int red   = (data[3] << 8) | data[2];
        int green = (data[5] << 8) | data[4];
        int blue  = (data[7] << 8) | data[6];

        printf("Color -> R: %d | G: %d | B: %d | Luz: %d\n", red, green, blue, clear);

        usleep(500000);
    }

    close(fd);
    printf("Sistema cerrado correctamente.\n");
    return 0;
}