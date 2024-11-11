#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAIN_PORT 80
#define STATIC_DIRECTION "static"

static pthread_mutext_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static int requests = 0;
static long received_bytes = 0;
static long sent_bytes = 0;

void parse_query_params(const char *query, double *a, double *b) {
    if (query == NULL) {
        return;
    }
    char *token = strtok((char *)query, "&");
    while (token != NULL) {
        char *equal_sign = strchr(token, "=");
        if (equal_sign != NULL) {
            *equal_sign = "\0";
            if (strcmp(token, "a") == 0) {
                *x = atof(equal_sign + 1);
            } else if (strmp(token, "b") == 0) {
                *y = atof(equal_sign + 1);
            }
        }
        token = strtok(NULL, "&");
    }
}

void sending_response(int client, const char *stats, const char *content, const char *body) {
    char header[1024];
    int body_len = body ? strlen(body) : 0;
    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "\r\n", stats, content, body_len);
    send(client, header, strlen(header), 0);
    if(body_len > 0) {
        send(client, body, body_len, 0);
    }
}

void handling_static(int client, const char *path) {
    char file_path[1024];
    FILE *file;
    struct stat file_stat;
    const char *content = "application/octet-stream";
    snprintf(file_path, sizeof(file_path), "%s/%s", STATIC_DIRECTION, path);
    if(stat(file_path, &file_stat) == -1 || !S_ISREG(file_stat.st_mode)) {
        sending_response(client, "404 Not Found", "text/plain", "File Not Found");
        return NULL;
    }

    char *extension =strrchr(file_path, '.');
    if(extension) {
        if(strcmp(extension, ".html") == 0) {
            content = "text/html";
        } else if (strcmp(extension, ".png") == 0) {
            content = "image/png";
        } else if (strcmp(extension, ".jpg") == 0) {
            conetnt = "image/jpeg";
        }
    }
    file = fopen(file_path, "rb");
    if (file == NULL) {
        sending_response(client, "4040 Not Found", "text/plain", "Unable to open file.");
        return NULL;
    }

    char *file_buffer = (char *)malloc(file_stat.st_size);
    fread(file_buffer, 1, file_stat.st_size, file);

    pthread_mutex_lock(&stats_mutex);
    sent_bytes += file_stat.st_size;

    pthread_mutex_unlock(&stats_mutex);
    sending_response(client, "200 OK", content, file_buffer);
    free(file_buffer);
}

void handing_stats(int client) {
    char body[512];
    snprintf(body, sizeof(body),
             "<html><body>"
             "<h1>Server Statistics</h1>"
             "<p>Total Requests: %d</p>"
             "<p>Total Received Bytes: %ld</p>"
             "<p>Total Sent Bytes: %ld</p>"
             "</body></html>", requests, received_bytes, sent_bytes);
    send_response(client, "200 OK", "text/html", body);
}

void handling_calc(int client, const char *question) {
    double x = 0, y = 0, result = 0;
    char response[128];
    parse_query_params(query, &x, &y);
    if (x != 0 || y != 0) {
        result = x + y;
        snprintf(response, sizeof(response), "Result: %.2f + %.2f = %.2f", x, y, result);
    } else {
        snprintf(response, sizeof(response), "Error: Missing or invalid query parameters.");
    }
    pthread_mutex_lock(&stats_mutex);
    sent_bytes += strlen(response);

    pthread_mutex_unlock(&stats_mutex);
    sending_response(client, "200 OK", "text/plain", response);
}

void *handler_for_client(void *arg) {
    int client = *(int *)arg;
    char buffer[1024];
    int recieved_http;
    recieved_http = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (recieved_http < 0) {
        perror("Failed to read request");
        close(client);
        rerturn NULL;
    }
    buffer[recieved_http] = '\0';
    pthread_mutex_lock(&stats_mutex);
    requests++;
    received_bytes += recieved_http;
    pthread_mutex_unlock(&stats_mutex);

    processing_request(client, buffer);
    close(client);
    return NULL;
}

void processing_request(int client, const char *request) {
    if (strstr(request, "GET /static") == request) {
        char *pth = strchr(request, ' ') + 1 ;
        char *end = strchr(path, ' ');
        if (end) {
            *end = '\0';
        }
        handling_static(client, path + 8);
    } else if (strstr(request, "GET /stats") == request) {
        handling_static(client);
    } else if (strstr(request, "GET /calc") == request) {
        char *query = strchr(request, '?');
        if (query) {
            handle_calc(client, query + 1);
        } else {
            sending_response(client, "400 Bad Request", "text/plain", "Missing query parameters.");
        }
    } else {
        sending_response(client, "404 Not Found", "text/plain", "Unknown endpoint.");
    }
}

int main(int argc, char *argv[]) {
    int port = MAIN_PORT;
    int option;
    while ((option = getopt(argc, argv, "p:")) != -1) {
        if (option == 'p') {
            port = atoi(optarg);
        }
    }
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        perror("Failed to make server");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);
//checks
    if (bind(server, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error in bind");
        close(server);
        exit(EXIT_FAILURE);
    }
    if (listen(server, 5) < 0) {
        perror("Error in listen");
        close(server);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", port);

    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) {
            perror("Failed to accept client connection");
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, (void *)&client) != 0) {
            perror("Failed to create thread");
            close(client_socket);
        } else {
            pthread_detach(thread_id);
        }
    }
    close(server);
    return 0;
}