#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096

void die(const char *msg)
{
    perror(msg);
    exit(1);
}

const char *get_file(const char *path)
{
    const char *file = strrchr(path, '/');

    if (file)
    {
        return file + 1;
    }

    return path;
}

int main(int argc, char *argv[])
{
    int sockfd, bytes_recebidos;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[BUFFER_SIZE];

    if (argc < 2)
    {
        fprintf(stderr, "Uso: %s http://hostname[:port]/caminho/arquivo\n", argv[0]);
        exit(0);
    }

    char *url = argv[1];

    char authority[256];
    char host[256];
    char path[512];
    char filename[256];
    int portno;

    if (sscanf(url, "http://%255[^/]%511[^\n]", authority, path) == 2)
    {
        strcpy(path, path + 1);
    }
    else if (sscanf(url, "http://%255[^\n]", authority) == 1)
    {

        strcpy(path, "");
    }
    else
    {
        fprintf(stderr, "URL mal formatada. Use http://host[:port]/path\n");
        exit(1);
    }

    char *port_str = strrchr(authority, ':');
    if (port_str != NULL)
    {
        int host_len = port_str - authority;
        strncpy(host, authority, host_len);
        host[host_len] = '\0';

        portno = atoi(port_str + 1);
    }
    else
    {
        strcpy(host, authority);
        portno = 80;
    }

    const char *filename_ptr = get_file(path);
    if (strlen(filename_ptr) == 0)
    {
        strcpy(filename, "index.html");
    }
    else
    {
        strcpy(filename, filename_ptr);
    }

    printf("Host: %s, Porta: %d, Path: /%s, Salvando como: %s\n",
           host, portno, path, filename);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        die("ERRO ao abrir socket");

    server = gethostbyname(host);
    if (server == NULL)
    {
        fprintf(stderr, "ERRO, host não encontrado: %s\n", host);
        exit(0);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0],
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        die("ERRO ao conectar");

    printf("Conectado ao servidor. Enviando requisição...\n");

    bzero(buffer, BUFFER_SIZE);
    snprintf(buffer, BUFFER_SIZE,
             "GET /%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: meu_navegador/1.0\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);

    if (send(sockfd, buffer, strlen(buffer), 0) < 0)
        die("ERRO ao enviar requisição");

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
        die("ERRO ao criar arquivo local");

    bool headers_found = false;

    printf("Recebendo resposta e salvando em %s...\n", filename);

    while ((bytes_recebidos = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[bytes_recebidos] = '\0';

        if (!headers_found)
        {
            char *header_end = strstr(buffer, "\r\n\r\n");

            if (header_end != NULL)
            {
                headers_found = true;

                char *body_start = header_end + 4;

                size_t body_chunk_len = bytes_recebidos - (body_start - buffer);

                if (body_chunk_len > 0)
                {
                    fwrite(body_start, 1, body_chunk_len, fp);
                }
            }
        }

        else
        {
            fwrite(buffer, 1, bytes_recebidos, fp);
        }
    }

    if (bytes_recebidos < 0)
    {
        perror("ERRO ao receber dados");
    }

    printf("Download concluído.\n");
    fclose(fp);
    close(sockfd);

    return 0;
}