#ifndef _WIN32
#include "tcpim_common.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

typedef int socket_t;
typedef void* thread_func_t(void* arg);
typedef pthread_t thread_t;

void* receive_messages(void* arg) {
    socket_t client_socket = *(socket_t*)arg;
    char buffer[MAX_BUFFER_LEN];
    int bytes_received;

    while (1) {
        // Receive message from the partner
        bytes_received = recv(client_socket, buffer, MAX_MSG_LEN, 0);
        if (bytes_received <= 0) {
            break;
        }
        buffer[bytes_received] = '\0';

        // Display received message to user
        printf("\nPartner: %sYou: ", buffer);
        fflush(stdout);
    }

    printf("\nPartner disconnected\n");

    // Exit thread
    pthread_exit(NULL);
}

socket_t connect_to_partner(char* partner_ip, int port) {
    socket_t client_socket;
    struct sockaddr_in partner_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == -1) {
        perror("socket");
        return -1;
    }

    partner_addr.sin_family = AF_INET;
    partner_addr.sin_port = htons(port);
    inet_pton(AF_INET, partner_ip, &partner_addr.sin_addr);

    if (connect(client_socket, (struct sockaddr*)&partner_addr, sizeof(partner_addr)) == -1) {
        perror("connect");
        close(client_socket);
        return -1;
    }

    return client_socket;
}

int main(int argc, char* argv[]) {
    char hostname[256];
    char ip[16];
    char buffer[MAX_BUFFER_LEN];

    socket_t server_socket, client_socket = -1;
    struct sockaddr_in server_addr, client_addr;

    socklen_t client_addr_len;
    int port;
    char partner_ip[16];

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        exit(1);
    }
    printf("Welcome to tcpim!\n");
    printf("Hostname: %s\n", hostname);

    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* host = NULL;
    int result = getaddrinfo(hostname, NULL, &hints, &host);
    if (result != 0) {
        fprintf(stderr, "%s", gai_strerror(result));
        exit(1);
    }

    // Display IP address of the local host
    inet_ntop(AF_INET, &((struct sockaddr_in*)host->ai_addr)->sin_addr, ip, sizeof(ip));
    printf("IP address: %s\n", ip);

    freeaddrinfo(host);

    // Get port number from user
    printf("Enter a port number to connect on (default 1836): ");
    if (scanf("%d", &port) != 1) {
        fprintf(stderr, "Invalid input\n");
        exit(1);
    }
    getchar(); // Consume newline character after scanf

      // Prompt user for partner IP address
    printf("Partner IP (enter '1' for server mode): ");
    if (scanf("%15s", partner_ip) != 1) {
        fprintf(stderr, "Invalid input\n");
        exit(1);
    }
    getchar(); // Consume newline character after scanf

    // Check if the user entered '1' for server mode
    if (strcmp(partner_ip, "1") != 0) {

        // Try to connect to partner
        client_socket = connect_to_partner(partner_ip, port);
        if (client_socket != -1) {
            printf("Connected to partner. Starting in client mode...\n");
        }
    }

    if (client_socket == -1) {
        printf("Failed to connect to partner. Starting in server mode...\n");

        // Create server socket
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == -1) {
            perror("socket");
            exit(1);
        }

        // Bind server socket to the specified port
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            perror("bind");
            close(server_socket);
            exit(1);
        }

        // Listen for incoming connections
        if (listen(server_socket, SOMAXCONN) == -1) {
            perror("listen");
            close(server_socket);
            exit(1);
        }

        printf("Listening on port %d\n", port);

        while (1) {
            // Accept incoming connection
            client_addr_len = sizeof(client_addr);
            client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
            if (client_socket == -1) {
                perror("accept");
                close(server_socket);
                exit(1);
            }

            printf("Connected to partner. Starting in server mode...\n");
            break; // Break the loop after accepting the connection
        }
    }

    // Create thread to receive messages from partner
    thread_t thread;
    if (pthread_create(&thread, NULL, receive_messages, &client_socket) != 0) {
        fprintf(stderr, "pthread_create failed\n");
        close(client_socket);
        exit(1);
    }

    // Print "You:" prompt before sending the first message
    printf("You: ");
    fflush(stdout);

    // Send messages to partner
    while (1) {
        if (fgets(buffer, MAX_BUFFER_LEN, stdin) == NULL) {
            break;
        }

        // Send message to partner
        if (send(client_socket, buffer, (int)strlen(buffer), 0) == -1) {
            perror("send");
            close(client_socket);
            exit(1);
        }
    }

    // Wait for thread to finish and close socket
    pthread_join(thread, NULL);
    close(client_socket);

    return 0;
}

#endif 