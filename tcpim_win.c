#ifdef _WIN32
#include "tcpim_common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h> // Include Windows Threads API library
#pragma comment(lib, "ws2_32.lib")

typedef SOCKET socket_t;
typedef DWORD WINAPI thread_func_t(LPVOID lpParameter);
typedef HANDLE thread_t;


socket_t connect_to_partner(char* partner_ip, int port) {
    socket_t client_socket;
    struct sockaddr_in server_addr;
    struct addrinfo* host;

    // Resolve partner IP address
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int result = getaddrinfo(partner_ip, NULL, &hints, &host);
    if (result != 0) {
        fprintf(stderr, "%ls", (wchar_t*)gai_strerror(result));
        return INVALID_SOCKET;
    }

    // Create client socket
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        perror("socket");
        freeaddrinfo(host);
        return INVALID_SOCKET;
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, &((struct sockaddr_in*)host->ai_addr)->sin_addr, sizeof(struct in_addr));

    freeaddrinfo(host);

    // Connect to partner
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        perror("connect");
        closesocket(client_socket);
        return INVALID_SOCKET;
    }

    return client_socket;
}

DWORD WINAPI receive_messages(LPVOID arg) {
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
    return 0;
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }
    char hostname[256];
    char ip[16];
    char buffer[MAX_BUFFER_LEN];

    SOCKET server_socket, client_socket = INVALID_SOCKET;
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
        fprintf(stderr, "%ls", (wchar_t*)gai_strerror(result));
        exit(1);
    }

    // Display IP address of the local host
    inet_ntop(AF_INET, &((struct sockaddr_in*)host->ai_addr)->sin_addr, ip, sizeof(ip));
    printf("IP address: %s\n", ip);

    freeaddrinfo(host);

    // Get port number from user
    printf("Enter a port number to connect on (default 1836): ");
    if (scanf_s("%d", &port) != 1) {
        fprintf(stderr, "Invalid input\n");
        exit(1);
    }
    getchar(); // Consume newline character after scanf_s

    // Prompt user for partner IP address
    printf("Partner IP (enter '1' for server mode): ");
    if (scanf_s("%15s", partner_ip, (unsigned)sizeof(partner_ip)) != 1) {
        fprintf(stderr, "Invalid input\n");
        exit(1);
    }
    getchar(); // Consume newline character after scanf_s

    // Check if the user entered '1' for server mode
    if (strcmp(partner_ip, "1") != 0) {
       
        // Try to connect to partner
        client_socket = connect_to_partner(partner_ip, port);
        if (client_socket != INVALID_SOCKET) {
            printf("Connected to partner. Starting in client mode...\n");
        }
    }

    if (client_socket == INVALID_SOCKET) {
        printf("Failed to connect to partner. Starting in server mode...\n");

        // Create server socket
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == INVALID_SOCKET) {
            perror("socket");
            exit(1);
        }

        // Set server socket to blocking mode
        u_long iMode = 0; // 0 = blocking mode, 1 = non-blocking mode
        if (ioctlsocket(server_socket, FIONBIO, &iMode) == SOCKET_ERROR) {
            perror("ioctlsocket");
            closesocket(server_socket);
            exit(1);
        }

        // Bind server socket to the specified port
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            perror("bind");
            closesocket(server_socket);
            exit(1);
        }

        // Listen for incoming connections
        if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
            perror("listen");
            closesocket(server_socket);
            exit(1);
        }

        printf("Listening on port %d\n", port);

        while (1) {
            // Accept incoming connection
            client_addr_len = sizeof(client_addr);
            client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
            if (client_socket == INVALID_SOCKET) {
                perror("accept");
                closesocket(server_socket);
                exit(1);
            }

            printf("Connected to partner. Starting in server mode...\n");
            break; // Break the loop after accepting the connection
        }
    }

    // Create thread to receive messages from partner
    thread_t thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)receive_messages, &client_socket, 0, NULL);
    if (thread == NULL) {
        fprintf(stderr, "CreateThread failed\n");
        closesocket(client_socket);
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
        if (send(client_socket, buffer, (int)strlen(buffer), 0) == SOCKET_ERROR) {
            perror("send");
            closesocket(client_socket);
            exit(1);
        }


    }

    // Wait for thread to finish and close socket
    WaitForSingleObject(thread, INFINITE);
    closesocket(client_socket);

    // Clean up Winsock
    WSACleanup();

    return 0;
}

#endif 