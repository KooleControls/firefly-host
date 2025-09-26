#include "FtpServer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"

static const char *TAG_FTP = "ftp_server";

/* ==== Data connection helpers ==== */
int FtpServer::openDataConnection(Client &c) {
    if (c.pasv_listen_sock <= 0) return -1;

    struct sockaddr_in6 source_addr;
    socklen_t addr_len = sizeof(source_addr);

    int sock = accept(c.pasv_listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // try again later
        }
        ESP_LOGE(TAG_FTP, "Failed to accept PASV data connection (errno=%d)", errno);
        return -1;
    }

    c.pasv_data_sock = sock;
    return sock;
}

void FtpServer::closeDataConnection(Client &c) {
    if (c.pasv_data_sock > 0) {
        close(c.pasv_data_sock);
        c.pasv_data_sock = -1;
    }
    if (c.pasv_listen_sock > 0) {
        close(c.pasv_listen_sock);
        c.pasv_listen_sock = -1;
    }
}


/* ==== FTP Command Handlers ==== */

void FtpServer::ftp_cmd_user(Client &c, const char *args) {
    (void)args;
    const char *resp = "331 OK. Password required\r\n";
    send(c.client_sock, resp, strlen(resp), 0);
}

void FtpServer::ftp_cmd_pass(Client &c, const char *args) {
    (void)args;
    const char *resp = "230 Login successful\r\n";
    send(c.client_sock, resp, strlen(resp), 0);
}

void FtpServer::ftp_cmd_syst(Client &c, const char *args) {
    (void)args;
    const char *resp = "215 UNIX Type: L8\r\n";
    send(c.client_sock, resp, strlen(resp), 0);
}

void FtpServer::ftp_cmd_quit(Client &c, const char *args) {
    (void)args;
    const char *resp = "221 Goodbye.\r\n";
    send(c.client_sock, resp, strlen(resp), 0);
    close(c.client_sock);
    c.client_sock = -1;
    c.active = false;
}

void FtpServer::ftp_cmd_pwd(Client &c, const char *args) {
    (void)args;
    char resp[256];
    snprintf(resp, sizeof(resp), "257 \"%s\" is current directory\r\n",
             c.cwd[0] ? c.cwd : "/");
    send(c.client_sock, resp, strlen(resp), 0);
}

void FtpServer::ftp_cmd_list(Client &c, const char *args) {
    (void)args;
    const char *start = "150 Here comes the directory listing\r\n";
    send(c.client_sock, start, strlen(start), 0);

    if (openDataConnection(c) < 0) {
        const char *resp = "425 Can't open data connection\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullbase[256];
    snprintf(fullbase, sizeof(fullbase), "%s%s", root_path, c.cwd);

    DIR *dir = opendir(fullbase);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            char line[256];
            char fullpath[256];
            struct stat st;

            snprintf(fullpath, sizeof(fullpath), "%s/%s", fullbase, entry->d_name);
            if (stat(fullpath, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    snprintf(line, sizeof(line),
                             "drwxr-xr-x 1 user group 0 Jan 1 00:00 %s\r\n",
                             entry->d_name);
                } else {
                    snprintf(line, sizeof(line),
                             "-rw-r--r-- 1 user group %ld Jan 1 00:00 %s\r\n",
                             (long)st.st_size, entry->d_name);
                }
                send(c.pasv_data_sock, line, strlen(line), 0);
            }
        }
        closedir(dir);
    }

    closeDataConnection(c);

    const char *done = "226 Directory send OK.\r\n";
    send(c.client_sock, done, strlen(done), 0);
}

void FtpServer::ftp_cmd_type(Client &c, const char *args) {
    (void)args;
    const char *resp = "200 Type set to I\r\n";
    send(c.client_sock, resp, strlen(resp), 0);
}

void FtpServer::ftp_cmd_noop(Client &c, const char *args) {
    (void)args;
    const char *resp = "200 NOOP ok\r\n";
    send(c.client_sock, resp, strlen(resp), 0);
}

void FtpServer::ftp_cmd_auth(Client &c, const char *args) {
    if (!args) {
        const char *resp = "501 Syntax error in parameters\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    if (strcasecmp(args, "TLS") == 0 || strcasecmp(args, "SSL") == 0) {
        //ESP_LOGW(TAG_FTP, "AUTH %s requested but not supported", args);
        const char *resp = "534 Local policy on server does not allow TLS/SSL\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
    } else {
        ESP_LOGW(TAG_FTP, "AUTH not recognized (args=%s)", args);
        const char *resp = "502 AUTH type not supported\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
    }
}



void FtpServer::ftp_cmd_pasv(Client &c, const char *args) {
    (void)args;
    closeDataConnection(c);

    c.pasv_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (c.pasv_listen_sock < 0) {
        const char *resp = "421 Can't open passive socket\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;

    if (bind(c.pasv_listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(c.pasv_listen_sock, 1) < 0) {
        ESP_LOGE(TAG_FTP, "PASV setup failed");
        close(c.pasv_listen_sock);
        c.pasv_listen_sock = -1;
        return;
    }

    struct sockaddr_in bound_addr;
    socklen_t len = sizeof(bound_addr);
    getsockname(c.pasv_listen_sock, (struct sockaddr *)&bound_addr, &len);
    int port = ntohs(bound_addr.sin_port);

    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(c.client_sock, (struct sockaddr *)&local_addr, &local_len) < 0) {
        ESP_LOGE(TAG_FTP, "Failed to get local address for PASV");
        close(c.pasv_listen_sock);
        c.pasv_listen_sock = -1;
        return;
    }

    uint32_t ip = ntohl(local_addr.sin_addr.s_addr);
    int ip1 = (ip >> 24) & 0xFF;
    int ip2 = (ip >> 16) & 0xFF;
    int ip3 = (ip >> 8) & 0xFF;
    int ip4 = ip & 0xFF;

    char resp[128];
    snprintf(resp, sizeof(resp),
             "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
             ip1, ip2, ip3, ip4,
             (port >> 8) & 0xFF, port & 0xFF);
    send(c.client_sock, resp, strlen(resp), 0);
}


void FtpServer::ftp_cmd_port(Client &c, const char *args) {
    if (!args) {
        const char *resp = "501 Syntax error in parameters\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(args, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        const char *resp = "501 Bad PORT format\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char ip[32];
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", h1, h2, h3, h4);
    int port = (p1 << 8) | p2;

    closeDataConnection(c);

    c.pasv_data_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (c.pasv_data_sock < 0) {
        const char *resp = "425 Can't open data connection\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(c.pasv_data_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG_FTP, "Active connect failed to %s:%d", ip, port);
        close(c.pasv_data_sock);
        c.pasv_data_sock = -1;
        const char *resp = "425 Can't connect to client\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }
    const char *resp = "200 PORT command successful\r\n";
    send(c.client_sock, resp, strlen(resp), 0);
}

void FtpServer::ftp_cmd_cwd(Client &c, const char *args) {
    if (!args || strlen(args) == 0) {
        const char *resp = "550 Failed to change directory.\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char newpath[256];
    if (args[0] == '/') {
        snprintf(newpath, sizeof(newpath), "%s%s", root_path, args);
    } else {
        snprintf(newpath, sizeof(newpath), "%s%s/%s", root_path,
                 (c.cwd[0] ? c.cwd : ""), args);
    }

    struct stat st;
    if (stat(newpath, &st) == 0 && S_ISDIR(st.st_mode)) {
        char newcwd[sizeof(c.cwd)];

        if (args[0] == '/') {
            snprintf(newcwd, sizeof(newcwd), "%s", args);
        } else {
            if (strcmp(c.cwd, "/") == 0) {
                snprintf(newcwd, sizeof(newcwd), "/%s", args);
            } else {
                snprintf(newcwd, sizeof(newcwd), "%s/%s", c.cwd, args);
            }
        }

        // ✅ Safe copy back into c.cwd
        strncpy(c.cwd, newcwd, sizeof(c.cwd));
        c.cwd[sizeof(c.cwd) - 1] = '\0';

        char resp[256];
        snprintf(resp, sizeof(resp), "250 Directory successfully changed to %s\r\n", c.cwd);
        send(c.client_sock, resp, strlen(resp), 0);
    } else {
        const char *resp = "550 Failed to change directory.\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
    }
}


void FtpServer::ftp_cmd_cdup(Client &c, const char *args) {
    (void)args;
    if (strcmp(c.cwd, "/") == 0) {
        const char *resp = "250 Already at root\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }
    char *slash = strrchr(c.cwd, '/');
    if (!slash || slash == c.cwd) {
        strcpy(c.cwd, "/");
    } else {
        *slash = '\0';
    }
    char resp[256];
    snprintf(resp, sizeof(resp), "250 Directory changed to %s\r\n", c.cwd);
    send(c.client_sock, resp, strlen(resp), 0);
}

void FtpServer::ftp_cmd_retr(Client &c, const char *args) {
    if (!args || strlen(args) == 0) {
        const char *resp = "550 File name required\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s", root_path,
             c.cwd[0] ? c.cwd : "", args);
    int fd = open(fullpath, O_RDONLY);
    if (fd < 0) {
        const char *resp = "550 Failed to open file\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    const char *start = "150 Opening data connection\r\n";
    send(c.client_sock, start, strlen(start), 0);
    if (openDataConnection(c) < 0) {
        const char *resp = "425 Can't open data connection\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        close(fd);
        return;
    }

    char buf[512];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        send(c.pasv_data_sock, buf, n, 0);
    }
    close(fd);
    closeDataConnection(c);

    const char *done = "226 Transfer complete\r\n";
    send(c.client_sock, done, strlen(done), 0);
}

void FtpServer::ftp_cmd_stor(Client &c, const char *args) {
    if (!args || strlen(args) == 0) {
        const char *resp = "550 File name required\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s", root_path,
             c.cwd[0] ? c.cwd : "", args);
    int fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        const char *resp = "550 Failed to create file\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    const char *start = "150 Opening data connection for upload\r\n";
    send(c.client_sock, start, strlen(start), 0);
    if (openDataConnection(c) < 0) {
        const char *resp = "425 Can't open data connection\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        close(fd);
        return;
    }

    char buf[512];
    ssize_t n;
    while ((n = recv(c.pasv_data_sock, buf, sizeof(buf), 0)) > 0) {
        write(fd, buf, n);
    }
    close(fd);
    closeDataConnection(c);

    const char *done = "226 Transfer complete\r\n";
    send(c.client_sock, done, strlen(done), 0);
}

void FtpServer::ftp_cmd_dele(Client &c, const char *args) {
    if (!args || strlen(args) == 0) {
        const char *resp = "501 No filename given\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s", root_path,
             c.cwd[0] ? c.cwd : "", args);

    if (unlink(fullpath) == 0) {
        char resp[256];
        snprintf(resp, sizeof(resp), "250 File deleted: %s\r\n", args);
        send(c.client_sock, resp, strlen(resp), 0);
    } else {
        const char *resp = "550 Failed to delete file\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
    }
}

void FtpServer::ftp_cmd_rmd(Client &c, const char *args) {
    if (!args || strlen(args) == 0) {
        const char *resp = "501 No directory name given\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s", root_path,
             c.cwd[0] ? c.cwd : "", args);

    if (rmdir(fullpath) == 0) {
        char resp[256];
        snprintf(resp, sizeof(resp), "250 Directory removed: %s\r\n", args);
        send(c.client_sock, resp, strlen(resp), 0);
    } else {
        const char *resp = "550 Failed to remove directory\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
    }
}

void FtpServer::ftp_cmd_mkd(Client &c, const char *args) {
    if (!args || strlen(args) == 0) {
        const char *resp = "501 No directory name given\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s", root_path,
             c.cwd[0] ? c.cwd : "", args);

    if (mkdir(fullpath, 0755) == 0) {
        char resp[256];
        snprintf(resp, sizeof(resp), "257 \"%s\" directory created\r\n", args);
        send(c.client_sock, resp, strlen(resp), 0);
    } else {
        const char *resp = "550 Failed to create directory\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
    }
}

void FtpServer::ftp_cmd_size(Client &c, const char *args) {
    if (!args || strlen(args) == 0) {
        const char *resp = "501 No filename given\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s", root_path,
             c.cwd[0] ? c.cwd : "", args);

    struct stat st;
    if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
        char resp[64];
        snprintf(resp, sizeof(resp), "213 %ld\r\n", (long)st.st_size);
        send(c.client_sock, resp, strlen(resp), 0);
    } else {
        const char *resp = "550 Could not get file size\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
    }
}

void FtpServer::ftp_cmd_mdtm(Client &c, const char *args) {
    if (!args || strlen(args) == 0) {
        const char *resp = "501 No filename given\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s",
             root_path,
             c.cwd[0] ? c.cwd : "",
             args);

    struct stat st;
    if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
        struct tm *tm_info = gmtime(&st.st_mtime);
        char resp[64];
        strftime(resp, sizeof(resp), "213 %Y%m%d%H%M%S\r\n", tm_info);
        send(c.client_sock, resp, strlen(resp), 0);
    } else {
        const char *resp = "550 Could not get file time\r\n";
        send(c.client_sock, resp, strlen(resp), 0);
    }
}

const FtpServer::CommandEntry FtpServer::cmdTable[] = {
    {"USER", &FtpServer::ftp_cmd_user},
    {"PASS", &FtpServer::ftp_cmd_pass},
    {"SYST", &FtpServer::ftp_cmd_syst},
    {"QUIT", &FtpServer::ftp_cmd_quit},
    {"PWD",  &FtpServer::ftp_cmd_pwd},
    {"XPWD", &FtpServer::ftp_cmd_pwd},
    {"TYPE", &FtpServer::ftp_cmd_type},
    {"NOOP", &FtpServer::ftp_cmd_noop},
    {"AUTH", &FtpServer::ftp_cmd_auth},
    {"PASV", &FtpServer::ftp_cmd_pasv},
    {"LIST", &FtpServer::ftp_cmd_list},
    {"CWD",  &FtpServer::ftp_cmd_cwd},
    {"CDUP", &FtpServer::ftp_cmd_cdup},
    {"RETR", &FtpServer::ftp_cmd_retr},
    {"STOR", &FtpServer::ftp_cmd_stor},
    {"DELE", &FtpServer::ftp_cmd_dele},
    {"RMD",  &FtpServer::ftp_cmd_rmd},
    {"MKD",  &FtpServer::ftp_cmd_mkd},
    {"SIZE", &FtpServer::ftp_cmd_size},
    {"MDTM", &FtpServer::ftp_cmd_mdtm},
    {"PORT", &FtpServer::ftp_cmd_port},
    {NULL,   NULL}
};


/* ==== Public methods ==== */
FtpServer::FtpServer(const char *root) : listen_sock(-1) {
    memset(root_path, 0, sizeof(root_path));
    snprintf(root_path, sizeof(root_path), "%s", root);

    for (int i = 0; i < FTP_MAX_CLIENTS; i++) {
        clients[i].client_sock = -1;
        clients[i].pasv_listen_sock = -1;
        clients[i].pasv_data_sock = -1;
        snprintf(clients[i].cwd, sizeof(clients[i].cwd), "/");
        clients[i].active = false;
    }
}

FtpServer::~FtpServer() {
    // Close listening socket
    if (listen_sock >= 0) {
        close(listen_sock);
        listen_sock = -1;
    }

    // Close all client sockets and data connections
    for (int i = 0; i < FTP_MAX_CLIENTS; i++) {
        Client &c = clients[i];
        if (c.client_sock >= 0) {
            close(c.client_sock);
            c.client_sock = -1;
        }
        if (c.pasv_listen_sock >= 0) {
            close(c.pasv_listen_sock);
            c.pasv_listen_sock = -1;
        }
        if (c.pasv_data_sock >= 0) {
            close(c.pasv_data_sock);
            c.pasv_data_sock = -1;
        }
    }

    ESP_LOGI(TAG_FTP, "FTP server shut down");
}


bool FtpServer::init() {
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG_FTP, "Unable to create socket");
        return false;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(FTP_CTRL_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG_FTP, "Socket bind failed");
        close(listen_sock);
        return false;
    }

    if (listen(listen_sock, FTP_MAX_CLIENTS) < 0) {
        ESP_LOGE(TAG_FTP, "Socket listen failed");
        close(listen_sock);
        return false;
    }

    int flags = fcntl(listen_sock, F_GETFL, 0);
    fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK);

    ESP_LOGI(TAG_FTP, "FTP server started on port %d, root=%s",
             FTP_CTRL_PORT, root_path);
    return true;
}

void FtpServer::tick() {
    // === Accept new clients ===
    struct sockaddr_in6 source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int new_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (new_sock >= 0) {
        int assigned = 0;
        for (int i = 0; i < FTP_MAX_CLIENTS; i++) {
            if (clients[i].client_sock < 0) {
                Client &c = clients[i];
                c.client_sock = new_sock;
                c.active = true;
                c.pasv_listen_sock = -1;
                c.pasv_data_sock = -1;
                snprintf(c.cwd, sizeof(c.cwd), "/");

                const char *welcome = "220 ESP32 FTP Server Ready\r\n";
                send(new_sock, welcome, strlen(welcome), 0);
                assigned = 1;
                break;
            }
        }
        if (!assigned) {
            ESP_LOGW(TAG_FTP, "Too many clients, rejecting");
            close(new_sock);
        }
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGE(TAG_FTP, "accept() error: %d", errno);
    }

    // === Process existing clients ===
    for (int i = 0; i < FTP_MAX_CLIENTS; i++) {
        Client &c = clients[i];
        if (c.client_sock < 0) continue;

        int len = recv(c.client_sock, c.buffer, sizeof(c.buffer) - 1, MSG_DONTWAIT);
        if (len > 0) {
            c.buffer[len] = 0;
            char *cmd = strtok(c.buffer, " \r\n");
            char *args = strtok(NULL, "\r\n");
            if (!cmd) continue;

            const CommandEntry *entry = cmdTable;
            while (entry->cmd) {
                if (strcasecmp(entry->cmd, cmd) == 0) {
                    (this->*entry->handler)(c, args ? args : "");
                    break;
                }
                entry++;
            }
            if (!entry->cmd) {
                const char *resp = "502 Command not implemented\r\n";
                send(c.client_sock, resp, strlen(resp), 0);
            }
        } else if (len == 0) {
            close(c.client_sock);
            c.client_sock = -1;
            c.active = false;
            closeDataConnection(c);
        } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGW(TAG_FTP, "Client %d socket error (errno=%d)", i, errno);
            close(c.client_sock);
            c.client_sock = -1;
            c.active = false;
            closeDataConnection(c);
        }
    }
}
