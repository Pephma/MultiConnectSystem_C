#include "common.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024
#define MAX_USERS 15

void usage(int argc, char **argv){
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("exemplo: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data{
    int client_socket;
    struct sockaddr_storage storage;
};

struct client_data *clients[MAX_USERS];

int total_users = 0;

// Função que será executada pela thread
// permite varios clientes comunicarem com o servidor ao mesmo tempo
void *client_thread(void *data){
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *client_addr = (struct sockaddr *)(&cdata->storage);
    char client_addrstr[BUFSIZ];
    addrtostr(client_addr, client_addrstr, BUFSIZ);
    printf("Connection from %s\n", client_addrstr);

    char comando_user[BUFFER_SIZE];
    char resposta_servidor[BUFFER_SIZE];
    int num_user;
    for(int i = 0; i < MAX_USERS; i++){
        if(clients[i] != NULL && clients[i]->client_socket == cdata->client_socket){
            num_user = i;
        }
    }

    while(1){
        memset(comando_user, 0, BUFFER_SIZE);
        //Recebe o comando do user
        if(0 > recv(cdata->client_socket, comando_user, BUFFER_SIZE, 0)){
            logexit("recv");
        }
        if(strcmp(comando_user, "close connection") == 0){
            //envia mensagem de sucesso para o user
            memset(resposta_servidor, 0, BUFFER_SIZE);
            sprintf(resposta_servidor, "Removed Successfully\n");
            if(0 > send(cdata->client_socket, resposta_servidor, BUFFER_SIZE, 0)){
                logexit("send");
            }
            clients[num_user] = NULL;
            //"User i removed\n"
            if(num_user < 10)
                printf("User 0%d removed\n", num_user);
            else
                printf("User %d removed\n", num_user);
            close(cdata->client_socket);
            // Libera a memória alocada para a estrutura
            free(cdata);
            //envia para todos os clientes que o usuario saiu
            memset(resposta_servidor, 0, BUFFER_SIZE);
            if(num_user < 10)
                sprintf(resposta_servidor, "User 0%d left the group!\n", num_user);
            else
                sprintf(resposta_servidor, "User %d left the group!\n", num_user);
            for(int i = 0; i < MAX_USERS; i++){
                if(clients[i] != NULL){
                    if(0 > send(clients[i]->client_socket, resposta_servidor, BUFFER_SIZE, 0)){
                        logexit("send");
                    }
                }
            }
            total_users--;
            break;
        }
        else if (strcmp(comando_user, "list users") == 0){
            //envia a lista de usuários para o cliente
            memset(resposta_servidor, 0, BUFFER_SIZE);
            int offset = 0;
            for (int i = 0; i < MAX_USERS; i++) {
                if (clients[i] != NULL && i<10) {
                    offset += snprintf(resposta_servidor + offset, BUFFER_SIZE - offset, "0%d ", i);
                }
                else if(clients[i] != NULL){
                    offset += snprintf(resposta_servidor + offset, BUFFER_SIZE - offset, "%d ", i);
                }
            }
            offset += snprintf(resposta_servidor + offset, BUFFER_SIZE - offset, "\n");
            if (offset >= BUFFER_SIZE) {
                // O tamanho do buffer foi excedido
                logexit("Buffer overflow");
            }
            if (0 > send(cdata->client_socket, resposta_servidor, BUFFER_SIZE, 0)) {
                logexit("send");
            }
        }
        //send to user "mensagem"
        else if(strncmp(comando_user,"send to",7) == 0){
            //separar por espaco e pegar o 3 argumento
            char *token = strtok(comando_user, " ");
            token = strtok(NULL, " ");
            token = strtok(NULL, " ");
            int user = atoi(token);
            //mensagem
            token = strtok(NULL, "\"");
            //verifica se o usuario existe
            if(clients[user] == NULL){
                printf("User %d not found\n", user);
                //envia mensagem de erro para o cliente
                memset(resposta_servidor, 0, BUFFER_SIZE);
                sprintf(resposta_servidor, "Receiver not found\n");
                if(0 > send(cdata->client_socket, resposta_servidor, BUFFER_SIZE, 0)){
                    logexit("send");
                }
            }
            else{
                //envia mensagem para o usuario
                memset(resposta_servidor, 0, BUFFER_SIZE);
                //hora e minutos
                time_t rawtime;
                struct tm * timeinfo;
                time(&rawtime);
                timeinfo = localtime(&rawtime);
                //confirmando envio para o usuario que enviou
                if(user < 10)
                    sprintf(resposta_servidor, "P [%d:%d] -> 0%d: ", timeinfo->tm_hour, timeinfo->tm_min, user);
                else
                    sprintf(resposta_servidor, "P [%d:%d] -> %d: ", timeinfo->tm_hour, timeinfo->tm_min, user);
                //mensagem
                strcat(resposta_servidor, token);
                strcat(resposta_servidor, "\n");
                if(0 > send(cdata->client_socket, resposta_servidor, BUFFER_SIZE, 0)){
                    logexit("send");
                }
                //envia mensagem para o usuario
                if(num_user < 10)
                    sprintf(resposta_servidor, "P [%d:%d] 0%d: ", timeinfo->tm_hour, timeinfo->tm_min, num_user);
                else
                    sprintf(resposta_servidor, "P [%d:%d] %d: ", timeinfo->tm_hour, timeinfo->tm_min, num_user);
                //mensagem
                strcat(resposta_servidor, token);
                strcat(resposta_servidor, "\n");
                if(0 > send(clients[user]->client_socket, resposta_servidor, BUFFER_SIZE, 0)){
                    logexit("send");
                }
            }
        }
        //send all "mensagem"
        else if(strncmp(comando_user, "send all",8) == 0){
            //separar por aspas e pegar o 2 argumento
            char *token = strtok(comando_user, "\"");
            token = strtok(NULL, "\"");
            //hora e minutos
            time_t rawtime;
            struct tm * timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            //envia mensagem para todos os usuarios
            memset(resposta_servidor, 0, BUFFER_SIZE);
            //confirmando envio para o usuario que enviou
            sprintf(resposta_servidor, "[%d:%d] -> all: ", timeinfo->tm_hour, timeinfo->tm_min);
            //mensagem
            strcat(resposta_servidor, token);
            strcat(resposta_servidor, "\n");
            if(0 > send(cdata->client_socket, resposta_servidor, BUFFER_SIZE, 0)){
                logexit("send");
            }
            //envia mensagem para todos os usuarios
            memset(resposta_servidor, 0, BUFFER_SIZE);
            if(num_user < 10)
                sprintf(resposta_servidor, "[%d:%d] 0%d: ", timeinfo->tm_hour, timeinfo->tm_min, num_user);
            else
                sprintf(resposta_servidor, "[%d:%d] %d: ", timeinfo->tm_hour, timeinfo->tm_min, num_user);
            strcat(resposta_servidor, token);
            strcat(resposta_servidor, "\n");
            for(int i = 0; i < MAX_USERS; i++){
                //todos menos o proprio cliente
                if(clients[i] != NULL && clients[i]->client_socket != cdata->client_socket){
                    if(0 > send(clients[i]->client_socket, resposta_servidor, BUFFER_SIZE, 0)){
                        logexit("send");
                    }
                }
            }
        }
        else{
            printf("Comando invalido\n");
            break;
        }
    }
    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv){
    if(argc < 3){
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if(0 != server_sockaddr_init(argv[1], argv[2], &storage)){
        usage(argc, argv);
    }

    //Definindo o socket de acordo com o protocolo
    int socket_internet;
    socket_internet = socket(storage.ss_family, SOCK_STREAM, 0);

    if(socket_internet == -1){
        logexit("socket");
    }

    int enable = 1;
    if(0 != setsockopt(socket_internet, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))){
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if( 0 != bind(socket_internet, addr, sizeof(storage))){
        logexit("bind");
    }

    if( 0 != listen(socket_internet, 10)){
        logexit("listen");
    }

    char mensagem_servidor[BUFFER_SIZE];
    char addrstr[BUFFER_SIZE];
    addrtostr(addr, addrstr, BUFFER_SIZE);
    printf("Bound to %s, waiting connections\n", addrstr);
    while(1){
        struct sockaddr_storage client_storage;
        struct sockaddr *client_addr = (struct sockaddr *)(&client_storage);
        socklen_t client_addrlen = sizeof(client_storage);
        int client_socket = accept(socket_internet, client_addr, &client_addrlen);
        if(client_socket == -1){
            logexit("accept");
        }
        //Verifica se o numero de clientes conectados é maior que o permitido
        if(total_users >= MAX_USERS){
            //Mensagem de erro para o cliente
            strcpy(mensagem_servidor, "User limit exceeded\n");
            if(0 > send(client_socket, mensagem_servidor, BUFFER_SIZE, 0)){
                logexit("send");
            }
            close(client_socket);
            continue;
        }
        else{
            total_users++;
            //Adiciona o cliente na lista de clientes
            //percorre a lista de clientes e adiciona o cliente na primeira posição vazia
            for(int i = 0; i < MAX_USERS; i++){
                if(clients[i] == NULL){
                    clients[i] = malloc(sizeof(struct client_data));
                    clients[i]->client_socket = client_socket;
                    memcpy(&(clients[i]->storage), &client_storage, sizeof(client_storage));
                    //Mensagem de sucesso para o cliente "User i added\n"
                    memset(mensagem_servidor, 0, BUFFER_SIZE);
                    if(i < 10)
                        sprintf(mensagem_servidor, "User 0%d joined the group!\n", i);
                    else
                        sprintf(mensagem_servidor, "User %d joined the group!\n", i);
                    //print usuario que entrou
                    if(i < 10)
                        printf("User 0%d added\n", i);
                    else
                        printf("User %d added\n", i);
                    //Envia mensagem para todos os clientes
                    for(int j = 0; j < MAX_USERS; j++){
                        if(clients[j] != NULL){
                            if(0 > send(clients[j]->client_socket, mensagem_servidor, BUFFER_SIZE, 0)){
                                logexit("send");
                            }
                        }
                    }
                    //Cria uma thread para o cliente
                    pthread_t thread_id;
                    pthread_create(&thread_id, NULL, client_thread, clients[i]);
                    break;
                }
            }
        }
    }
    exit(EXIT_SUCCESS);
}