#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char const *argv[])
{
    int sockfd;
    char *message = "Hello World";
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //Crea socket
    if (sockfd < 0){
        perror("Error al crear el socket");
        return 1;
    }

    //Configurar servidor
    memset(&servaddr, 0, sizeof(servaddr)); //Limpia la estructura
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);//Aquí se especifica el puerto
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //Aquí debe ir la IP del servidor

    //Enviar mensaje
    int n = sendto(sockfd, message, strlen(message), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
    if(n < 0){
        perror("Error al enviar");
    } else {
        printf("Mensaje enviado con éxito.\n");
    }

    close(sockfd);
    return 0;
}
