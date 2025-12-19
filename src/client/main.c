#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <direct.h>
#include "common/protocol.h"
#include "input_inject.h"
#include "keymap.h"

#pragma comment(lib, "ws2_32.lib")

static int running = 1;
static FILE *log_file = NULL;

// Get current timestamp string
void get_timestamp(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Log message to both console and file
void log_message(const char *format, ...) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));

    // Format the message
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Print to console
    printf("[%s] %s\n", timestamp, message);

    // Write to log file
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fflush(log_file);  // Ensure it's written immediately
    }
}

// Create Logs directory and open log file
int init_logging() {
    // Create Logs directory
    if (_mkdir("Logs") == 0) {
        printf("Created Logs directory\n");
    } else if (errno != EEXIST) {
        printf("Warning: Could not create Logs directory (error %d)\n", errno);
        // Continue anyway, will just log to console
        return 0;
    }

    // Generate log filename with timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char filename[256];
    strftime(filename, sizeof(filename), "Logs\\lankm_client_%Y%m%d_%H%M%S.log", tm_info);

    log_file = fopen(filename, "w");
    if (!log_file) {
        printf("Warning: Could not open log file %s (error %d)\n", filename, errno);
        return -1;
    }

    log_message("Log file created: %s", filename);
    return 0;
}

void cleanup_logging() {
    if (log_file) {
        log_message("Client shutting down");
        fclose(log_file);
        log_file = NULL;
    }
}

BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        log_message("Received shutdown signal (code: %d)", signal);
        running = 0;
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET sock_fd;
    struct sockaddr_in server_addr;
    Message msg;
    char *server_ip;
    int server_port = 24800;

    // Initialize logging first
    init_logging();
    log_message("LanKM Client v1.0.0 starting...");

    // Setup console handler
    SetConsoleCtrlHandler(console_handler, TRUE);

    // Check arguments
    if (argc < 2) {
        log_message("Usage: %s <server-ip> [port]", argv[0]);
        log_message("Example: %s 192.168.1.100", argv[0]);
        cleanup_logging();
        return 1;
    }

    server_ip = argv[1];
    if (argc > 2) {
        server_port = atoi(argv[2]);
        if (server_port <= 0 || server_port > 65535) {
            log_message("Invalid port number. Using default port 24800.");
            server_port = 24800;
        }
    }

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        log_message("Failed to initialize Winsock: %d", WSAGetLastError());
        cleanup_logging();
        return 1;
    }

    // Create socket
    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd == INVALID_SOCKET) {
        log_message("Failed to create socket: %d", WSAGetLastError());
        WSACleanup();
        cleanup_logging();
        return 1;
    }

    // Disable Nagle's algorithm for low latency
    BOOL nagle_disabled = TRUE;
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&nagle_disabled, sizeof(nagle_disabled)) == SOCKET_ERROR) {
        log_message("Warning: Could not disable Nagle's algorithm");
    }

    // Disable socket send buffer to reduce buffering
    int sendbuf_size = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, (char*)&sendbuf_size, sizeof(sendbuf_size)) == SOCKET_ERROR) {
        log_message("Warning: Could not set send buffer size");
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Convert IP address
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        log_message("Invalid IP address: %s", server_ip);
        closesocket(sock_fd);
        WSACleanup();
        cleanup_logging();
        return 1;
    }

    // Connect to server
    log_message("Connecting to %s:%d...", server_ip, server_port);
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        log_message("Failed to connect to server: %d", WSAGetLastError());
        closesocket(sock_fd);
        WSACleanup();
        cleanup_logging();
        return 1;
    }

    log_message("Connected to server!");

    // Initialize input injection
    if (init_input_inject() != 0) {
        log_message("Failed to initialize input injection");
        closesocket(sock_fd);
        WSACleanup();
        cleanup_logging();
        return 1;
    }

    // Initialize keymap
    init_keymap();

    log_message("Receiving input events... Press Ctrl+C to disconnect");

    // Set socket to non-blocking mode
    u_long mode = 1;
    ioctlsocket(sock_fd, FIONBIO, &mode);

    // Main loop
    while (running) {
        // Use select() to check if data is available
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);

        // Set timeout to 1ms for low latency
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        int result = select(0, &read_fds, NULL, NULL, &timeout);

        if (result > 0 && FD_ISSET(sock_fd, &read_fds)) {
            // Data is available, receive it
            int batch_count = 0;

            while (batch_count < 25) {
                int bytes_received = recv(sock_fd, (char*)&msg, sizeof(Message), 0);

                if (bytes_received <= 0) {
                    int error = WSAGetLastError();
                    if (bytes_received == 0) {
                        log_message("Server disconnected");
                        running = 0;
                        break;
                    } else if (error == WSAEWOULDBLOCK) {
                        break;
                    } else if (error == WSAECONNRESET) {
                        log_message("Connection reset by server");
                        running = 0;
                        break;
                    } else {
                        log_message("Error receiving data: %d", error);
                        running = 0;
                        break;
                    }
                }

                if (bytes_received != sizeof(Message)) {
                    log_message("Received incomplete message (%d bytes)", bytes_received);
                    break;
                }

                // Process message based on type
                switch (msg.type) {
                    case MSG_MOUSE_MOVE:
                        inject_mouse_move(msg.a, msg.b);
                        break;

                    case MSG_MOUSE_BUTTON:
                        {
                            const char* btn_name = "UNKNOWN";
                            const char* state_name = msg.b ? "DOWN" : "UP";
                            if (msg.a == 1) btn_name = "LEFT";
                            else if (msg.a == 2) btn_name = "RIGHT";
                            else if (msg.a == 3) btn_name = "MIDDLE";
                            log_message("Mouse %s %s (type=%d, a=%d, b=%d)", btn_name, state_name, msg.type, msg.a, msg.b);
                        }
                        inject_mouse_button(msg.a, msg.b);
                        batch_count = 9999;
                        break;

                    case MSG_KEY_EVENT:
                        {
                            WORD vk_code = map_scancode_to_vk(msg.a);
                            if (vk_code != 0) {
                                inject_key_event(vk_code, msg.b);
                                log_message("Key event: scancode=%d, vk=%d, state=%d", msg.a, vk_code, msg.b);
                            }
                        }
                        batch_count = 9999;
                        break;

                    case MSG_SWITCH:
                        if (msg.a == 1) {
                            log_message("Control switched to REMOTE");
                        } else {
                            log_message("Control switched to LOCAL");
                        }
                        break;

                    default:
                        log_message("Unknown message type: %d", msg.type);
                        break;
                }

                batch_count++;

                // Quick check if more data is available without waiting
                fd_set read_fds_quick;
                struct timeval timeout_quick = {0, 0};
                FD_ZERO(&read_fds_quick);
                FD_SET(sock_fd, &read_fds_quick);

                if (select(0, &read_fds_quick, NULL, NULL, &timeout_quick) <= 0) {
                    break;
                }
            }
        } else if (result == SOCKET_ERROR) {
            log_message("select() error: %d", WSAGetLastError());
            break;
        }
    }

    // Cleanup
    log_message("Cleaning up...");

    // Graceful shutdown
    mode = 0;
    ioctlsocket(sock_fd, FIONBIO, &mode);

    cleanup_input_inject();

    // Close socket gracefully
    shutdown(sock_fd, SD_BOTH);
    closesocket(sock_fd);
    WSACleanup();

    log_message("Client shutdown complete");
    cleanup_logging();
    return 0;
}
