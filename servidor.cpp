#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MAXLINE 1024

int main() {
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;

    //Crear el socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear el socket");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    //Configurar la dirección del servidor
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; // Escucha en cualquier interfaz de red
    servaddr.sin_port = htons(PORT);

    //Vincular el socket al puerto (BIND)
    //Le dice al S.O. que todo lo que llegue al puerto 8080 es para este programa
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Error en el bind");
        close(sockfd);
        return -1;
    }

    std::cout << "Servidor UDP listo y esperando en el puerto " << PORT << "..." << std::endl;

    socklen_t len = sizeof(cliaddr);
    int n;

    //Recibir el mensaje
    
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len); // recvfrom bloquea el programa hasta que llega algo
    
    if (n >= 0) {
        buffer[n] = '\0'; // Finalizar la cadena
        std::cout << "Cliente dice: " << buffer << std::endl;
        
        // Opcional: Responder al cliente
        char *response = "Mensaje recibido";
        sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *) &cliaddr, len);
        std::cout << "Respuesta enviada con éxito." << std::endl;
    }

    close(sockfd);
    return 0;
}