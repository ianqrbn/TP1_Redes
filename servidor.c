#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 4096

const char *get_file_extension(const char *file_name)
{
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name)
    {
        return "";
    }
    return dot + 1;
}

const char *get_mime_type(const char *file_ext)
{
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0)
    {
        return "text/html";
    }
    else if (strcasecmp(file_ext, "txt") == 0)
    {
        return "text/plain";
    }
    else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0)
    {
        return "image/jpeg";
    }
    else if (strcasecmp(file_ext, "png") == 0)
    {
        return "image/png";
    }
    else
    {
        return "application/octet-stream";
    }
}

char *url_decode(const char *src)
{
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    for (size_t i = 0; i < src_len; i++)
    {
        if (src[i] == '%' && i + 2 < src_len)
        {
            int hex_val;
            sscanf(src + i + 1, "%2x", &hex_val);
            decoded[decoded_len++] = hex_val;
            i += 2;
        }
        else
        {
            decoded[decoded_len++] = src[i];
        }
    }

    decoded[decoded_len] = '\0';
    return decoded;
}

void *handle_client(void *arg)
{
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(8192 * sizeof(char));
    if (buffer == NULL)
    {
        perror("malloc buffer");
        close(client_fd);
        free(arg);
        return NULL;
    }

    ssize_t bytes_received = recv(client_fd, buffer, 8192 - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        regex_t regex;
        regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
        regmatch_t matches[2];

        if (regexec(&regex, buffer, 2, matches, 0) == 0)
        {
            buffer[matches[1].rm_eo] = '\0';
            const char *url_encoded_uri = buffer + matches[1].rm_so;
            char *decoded_uri = url_decode(url_encoded_uri);

            if (strstr(decoded_uri, "..") != NULL)
            {
                char response[128];
                snprintf(response, sizeof(response), "HTTP/1.1 403 Forbidden\r\n\r\nForbidden");
                send(client_fd, response, strlen(response), 0);
            }
            else
            {
                char file_path[1024];

                if (strlen(decoded_uri) == 0)
                {
                    strcpy(file_path, ".");
                }
                else
                {
                    strcpy(file_path, decoded_uri);
                }

                struct stat path_stat;
                if (stat(file_path, &path_stat) != 0)
                {
                    send_404_response(client_fd);
                }
                else if (S_ISREG(path_stat.st_mode))
                {
                    char file_ext[32];
                    strcpy(file_ext, get_file_extension(file_path));
                    send_file_response(client_fd, file_path, file_ext);
                }
                else if (S_ISDIR(path_stat.st_mode))
                {
                    char index_path[1024];
                    snprintf(index_path, sizeof(index_path), "%s/index.html", file_path);

                    struct stat index_stat;
                    if (stat(index_path, &index_stat) == 0 && S_ISREG(index_stat.st_mode))
                    {
                        send_file_response(client_fd, index_path, "html");
                    }
                    else
                    {
                        char *uri_for_links = decoded_uri;
                        if (strlen(uri_for_links) == 0)
                            uri_for_links = "/";
                        send_directory_listing(client_fd, file_path, uri_for_links);
                    }
                }
                else
                {
                    send_404_response(client_fd);
                }
            }

            free(decoded_uri);
        }
        regfree(&regex);
    }
    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}

void send_404_response(int client_fd)
{
    char response[512];
    snprintf(response, sizeof(response),
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/plain\r\n"
             "Connection: close\r\n"
             "\r\n"
             "404 Not Found");
    send(client_fd, response, strlen(response), 0);
}

void send_directory_listing(int client_fd, const char *dir_path, const char *uri_path)
{
    char header[1024];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Connection: close\r\n"
             "\r\n");
    send(client_fd, header, strlen(header), 0);

    char body_line[2048];
    snprintf(body_line, sizeof(body_line),
             "<html><head><title>Arquivos</title></head>"
             "<body><h1>Arquivos</h1><ul>\r\n");
    send(client_fd, body_line, strlen(body_line), 0);

    DIR *d = opendir(dir_path);
    if (d == NULL)
    {
        send(client_fd, "<li>Erro ao ler o diretório.</li>\r\n", 35, 0);
    }
    else
    {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_name[0] == '.')
            {
                continue;
            }

            char link_path[1024];
            if (strcmp(uri_path, "/") == 0)
            {
                snprintf(link_path, sizeof(link_path), "%s", dir->d_name);
            }
            else
            {
                snprintf(link_path, sizeof(link_path), "%s/%s", uri_path, dir->d_name);
            }

            snprintf(body_line, sizeof(body_line),
                     "<li><a href=\"%s\">%s</a></li>\r\n",
                     link_path, dir->d_name);
            send(client_fd, body_line, strlen(body_line), 0);
        }
        closedir(d);
    }

    snprintf(body_line, sizeof(body_line), "</ul></body></html>\r\n");
    send(client_fd, body_line, strlen(body_line), 0);
}

void send_file_response(int client_fd, const char *file_name, const char *file_ext)
{
    struct stat file_stat;
    if (stat(file_name, &file_stat) != 0)
    {
        send_404_response(client_fd);
        return;
    }
    off_t file_size = file_stat.st_size;
    const char *mime_type = get_mime_type(file_ext);

    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1)
    {
        send_404_response(client_fd);
        return;
    }

    char header[1024];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n",
             mime_type, file_size);
    send(client_fd, header, strlen(header), 0);

    char file_buffer[8192];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0)
    {
        if (send(client_fd, file_buffer, bytes_read, 0) < 0)
        {
            break;
        }
    }

    close(file_fd);
}

int main(int argc, char *argv[])
{
    int server_fd;
    struct sockaddr_in server_addr;

    if (argc < 2)
    {
        fprintf(stderr, "Uso: %s <diretorio_base>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *base_dir = argv[1];

    if (chdir(base_dir) != 0)
    {
        perror("chdir: Falha ao mudar para o diretório base");
        exit(EXIT_FAILURE);
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Servidor escutando a porta %d\n", PORT);
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        if ((*client_fd = accept(server_fd,
                                 (struct sockaddr *)&client_addr,
                                 &client_addr_len)) < 0)
        {
            perror("accept failed");
            continue;
        }

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}