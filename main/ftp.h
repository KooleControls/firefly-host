#pragma once

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
#include "esp_log.h"
#include "esp_netif.h"

#define FTP_CTRL_PORT 21
#define FTP_BUFFER_SIZE 512
#define FTP_MAX_CLIENTS 4

static const char *TAG_FTP = "ftp_server";

/* ==== Client/session ==== */
typedef struct
{
    int client_sock;
    int pasv_listen_sock;
    int pasv_data_sock;
    char cwd[128];
    char buffer[FTP_BUFFER_SIZE];
    int active;
} ftp_client_t;

/* ==== Server instance ==== */
typedef struct
{
    int listen_sock;
    char root_path[128];
    ftp_client_t clients[FTP_MAX_CLIENTS];
} ftp_server_t;

/* ===== Command handler typedef ===== */
typedef void (*ftp_cmd_handler_t)(ftp_server_t *srv, ftp_client_t *c, const char *args);

/* ==== Data connection helpers ==== */
static int ftp_open_data_connection(ftp_client_t *c) {
    if (c->pasv_listen_sock <= 0) return -1;

    struct sockaddr_in6 source_addr;
    socklen_t addr_len = sizeof(source_addr);

    int sock = accept(c->pasv_listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no connection yet, try again later
            return 0;
        }
        ESP_LOGE(TAG_FTP, "Failed to accept PASV data connection (errno=%d)", errno);
        return -1;
    }

    c->pasv_data_sock = sock;
    return sock;
}

static void ftp_close_data_connection(ftp_client_t *c)
{
    if (c->pasv_data_sock > 0)
    {
        close(c->pasv_data_sock);
        c->pasv_data_sock = -1;
    }
    if (c->pasv_listen_sock > 0)
    {
        close(c->pasv_listen_sock);
        c->pasv_listen_sock = -1;
    }
}

/* ==== FTP Command Handlers ==== */

static void ftp_cmd_user(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    (void)args;
    const char *resp = "331 OK. Password required\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_pass(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    (void)args;
    const char *resp = "230 Login successful\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_syst(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    (void)args;
    const char *resp = "215 UNIX Type: L8\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_quit(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    (void)args;
    const char *resp = "221 Goodbye.\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
    close(c->client_sock);
    c->client_sock = -1;
    c->active = 0;
}

static void ftp_cmd_pwd(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    (void)args;
    char resp[256];
    snprintf(resp, sizeof(resp), "257 \"%s\" is current directory\r\n",
             c->cwd[0] ? c->cwd : "/");
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_list(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)args;
    const char *start = "150 Here comes the directory listing\r\n";
    send(c->client_sock, start, strlen(start), 0);

    if (ftp_open_data_connection(c) < 0)
    {
        const char *resp = "425 Can't open data connection\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullbase[256];
    snprintf(fullbase, sizeof(fullbase), "%s%s", srv->root_path, c->cwd);

    DIR *dir = opendir(fullbase);
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            char line[256];
            char fullpath[256];
            struct stat st;

            snprintf(fullpath, sizeof(fullpath), "%s/%s", fullbase, entry->d_name);
            if (stat(fullpath, &st) == 0)
            {
                if (S_ISDIR(st.st_mode))
                {
                    snprintf(line, sizeof(line),
                             "drwxr-xr-x 1 user group 0 Jan 1 00:00 %s\r\n",
                             entry->d_name);
                }
                else
                {
                    snprintf(line, sizeof(line),
                             "-rw-r--r-- 1 user group %ld Jan 1 00:00 %s\r\n",
                             (long)st.st_size, entry->d_name);
                }
                send(c->pasv_data_sock, line, strlen(line), 0);
            }
        }
        closedir(dir);
    }

    ftp_close_data_connection(c);

    const char *done = "226 Directory send OK.\r\n";
    send(c->client_sock, done, strlen(done), 0);
}

static void ftp_cmd_type(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    (void)args;
    const char *resp = "200 Type set to I\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_noop(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    (void)args;
    const char *resp = "200 NOOP ok\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_auth(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    ESP_LOGW(TAG_FTP, "AUTH not supported (args=%s)", args ? args : "");
    const char *resp = "502 AUTH not supported\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_pasv(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)args;
    ftp_close_data_connection(c);

    c->pasv_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (c->pasv_listen_sock < 0)
    {
        const char *resp = "421 Can't open passive socket\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;

    if (bind(c->pasv_listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(c->pasv_listen_sock, 1) < 0)
    {
        ESP_LOGE(TAG_FTP, "PASV setup failed");
        close(c->pasv_listen_sock);
        c->pasv_listen_sock = -1;
        return;
    }

    struct sockaddr_in bound_addr;
    socklen_t len = sizeof(bound_addr);
    getsockname(c->pasv_listen_sock, (struct sockaddr *)&bound_addr, &len);
    int port = ntohs(bound_addr.sin_port);

    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK)
    {
        ESP_LOGE(TAG_FTP, "Failed to get station IP");
        return;
    }
    uint32_t ip = ip_info.ip.addr; // already in network byte order
    int ip1 = ip & 0xFF;
    int ip2 = (ip >> 8) & 0xFF;
    int ip3 = (ip >> 16) & 0xFF;
    int ip4 = (ip >> 24) & 0xFF;

    char resp[128];
    snprintf(resp, sizeof(resp),
             "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
             ip1, ip2, ip3, ip4,
             (port >> 8) & 0xFF, port & 0xFF);
    send(c->client_sock, resp, strlen(resp), 0);
    ESP_LOGI(TAG_FTP, "PASV on %d.%d.%d.%d:%d", ip1, ip2, ip3, ip4, port);
}

static void ftp_cmd_port(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args)
    {
        const char *resp = "501 Syntax error in parameters\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(args, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6)
    {
        const char *resp = "501 Bad PORT format\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    char ip[32];
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", h1, h2, h3, h4);
    int port = (p1 << 8) | p2;

    // close old data sockets
    ftp_close_data_connection(c);

    c->pasv_data_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (c->pasv_data_sock < 0)
    {
        const char *resp = "425 Can't open data connection\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(c->pasv_data_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        ESP_LOGE(TAG_FTP, "Active connect failed to %s:%d", ip, port);
        close(c->pasv_data_sock);
        c->pasv_data_sock = -1;
        const char *resp = "425 Can't connect to client\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    ESP_LOGI(TAG_FTP, "Active data connection to %s:%d", ip, port);
    const char *resp = "200 PORT command successful\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_cwd(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args || strlen(args) == 0)
    {
        const char *resp = "550 Failed to change directory.\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    // Build candidate absolute path (root_path + cwd + args)
    char newpath[256];
    if (args[0] == '/')
    {
        snprintf(newpath, sizeof(newpath), "%s%s", srv->root_path, args);
    }
    else
    {
        snprintf(newpath, sizeof(newpath), "%s%s/%s",
                 srv->root_path,
                 (c->cwd[0] ? c->cwd : ""),
                 args);
    }

    struct stat st;
    if (stat(newpath, &st) == 0 && S_ISDIR(st.st_mode))
    {
        // Now update c->cwd safely
        char newcwd[sizeof(c->cwd)];
        if (args[0] == '/')
        {
            // absolute path
            snprintf(newcwd, sizeof(newcwd), "%s", args);
        }
        else
        {
            if (strcmp(c->cwd, "/") == 0)
            {
                snprintf(newcwd, sizeof(newcwd), "/%s", args);
            }
            else
            {
                snprintf(newcwd, sizeof(newcwd), "%s/%s", c->cwd, args);
            }
        }

        // Copy into client cwd safely
        strncpy(c->cwd, newcwd, sizeof(c->cwd));
        c->cwd[sizeof(c->cwd) - 1] = '\0';

        char resp[256];
        snprintf(resp, sizeof(resp),
                 "250 Directory successfully changed to %s\r\n",
                 c->cwd);
        send(c->client_sock, resp, strlen(resp), 0);
    }
    else
    {
        const char *resp = "550 Failed to change directory.\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
    }
}

static void ftp_cmd_cdup(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    (void)srv;
    (void)args;
    if (strcmp(c->cwd, "/") == 0)
    {
        const char *resp = "250 Already at root\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }
    char *slash = strrchr(c->cwd, '/');
    if (!slash || slash == c->cwd)
        strcpy(c->cwd, "/");
    else
        *slash = '\0';
    char resp[256];
    snprintf(resp, sizeof(resp), "250 Directory changed to %s\r\n", c->cwd);
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_retr(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args || strlen(args) == 0)
    {
        const char *resp = "550 File name required\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s", srv->root_path,
             c->cwd[0] ? c->cwd : "", args);
    int fd = open(fullpath, O_RDONLY);
    if (fd < 0)
    {
        const char *resp = "550 Failed to open file\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }
    const char *start = "150 Opening data connection\r\n";
    send(c->client_sock, start, strlen(start), 0);
    if (ftp_open_data_connection(c) < 0)
    {
        const char *resp = "425 Can't open data connection\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        close(fd);
        return;
    }
    char buf[512];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        send(c->pasv_data_sock, buf, n, 0);
    }
    close(fd);
    ftp_close_data_connection(c);
    const char *done = "226 Transfer complete\r\n";
    send(c->client_sock, done, strlen(done), 0);
}

static void ftp_cmd_stor(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args || strlen(args) == 0)
    {
        const char *resp = "550 File name required\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s", srv->root_path,
             c->cwd[0] ? c->cwd : "", args);
    int fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        const char *resp = "550 Failed to create file\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }
    const char *start = "150 Opening data connection for upload\r\n";
    send(c->client_sock, start, strlen(start), 0);
    if (ftp_open_data_connection(c) < 0)
    {
        const char *resp = "425 Can't open data connection\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        close(fd);
        return;
    }
    char buf[512];
    ssize_t n;
    while ((n = recv(c->pasv_data_sock, buf, sizeof(buf), 0)) > 0)
    {
        write(fd, buf, n);
    }
    close(fd);
    ftp_close_data_connection(c);
    const char *done = "226 Transfer complete\r\n";
    send(c->client_sock, done, strlen(done), 0);
}

static void ftp_cmd_unknown(ftp_server_t *srv, ftp_client_t *c, const char *cmd)
{
    (void)srv;
    ESP_LOGE(TAG_FTP, "Unknown command: %s", cmd ? cmd : "(null)");
    const char *resp = "502 Command not implemented\r\n";
    send(c->client_sock, resp, strlen(resp), 0);
}

static void ftp_cmd_dele(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args || strlen(args) == 0)
    {
        const char *resp = "501 No filename given\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s",
             srv->root_path,
             c->cwd[0] ? c->cwd : "",
             args);

    if (unlink(fullpath) == 0)
    {
        char resp[256];
        snprintf(resp, sizeof(resp), "250 File deleted: %s\r\n", args);
        send(c->client_sock, resp, strlen(resp), 0);
        ESP_LOGI(TAG_FTP, "Deleted file: %s", fullpath);
    }
    else
    {
        ESP_LOGE(TAG_FTP, "Failed to delete file: %s", fullpath);
        const char *resp = "550 Failed to delete file\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
    }
}

static void ftp_cmd_rmd(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args || strlen(args) == 0)
    {
        const char *resp = "501 No directory name given\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s",
             srv->root_path,
             c->cwd[0] ? c->cwd : "",
             args);

    if (rmdir(fullpath) == 0)
    {
        char resp[256];
        snprintf(resp, sizeof(resp), "250 Directory removed: %s\r\n", args);
        send(c->client_sock, resp, strlen(resp), 0);
        ESP_LOGI(TAG_FTP, "Removed directory: %s", fullpath);
    }
    else
    {
        ESP_LOGE(TAG_FTP, "Failed to remove directory: %s", fullpath);
        const char *resp = "550 Failed to remove directory\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
    }
}

static void ftp_cmd_mkd(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args || strlen(args) == 0)
    {
        const char *resp = "501 No directory name given\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s",
             srv->root_path,
             c->cwd[0] ? c->cwd : "",
             args);

    if (mkdir(fullpath, 0755) == 0)
    {
        char resp[256];
        snprintf(resp, sizeof(resp), "257 \"%s\" directory created\r\n", args);
        send(c->client_sock, resp, strlen(resp), 0);
        ESP_LOGI(TAG_FTP, "Created directory: %s", fullpath);
    }
    else
    {
        ESP_LOGE(TAG_FTP, "Failed to create directory: %s", fullpath);
        const char *resp = "550 Failed to create directory\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
    }
}

static void ftp_cmd_size(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args || strlen(args) == 0)
    {
        const char *resp = "501 No filename given\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s",
             srv->root_path,
             c->cwd[0] ? c->cwd : "",
             args);

    struct stat st;
    if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode))
    {
        char resp[64];
        snprintf(resp, sizeof(resp), "213 %ld\r\n", (long)st.st_size);
        send(c->client_sock, resp, strlen(resp), 0);
    }
    else
    {
        const char *resp = "550 Could not get file size\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
    }
}

static void ftp_cmd_mdtm(ftp_server_t *srv, ftp_client_t *c, const char *args)
{
    if (!args || strlen(args) == 0)
    {
        const char *resp = "501 No filename given\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
        return;
    }

    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s%s/%s",
             srv->root_path,
             c->cwd[0] ? c->cwd : "",
             args);

    struct stat st;
    if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode))
    {
        struct tm *tm_info = gmtime(&st.st_mtime);
        char resp[64];
        strftime(resp, sizeof(resp), "213 %Y%m%d%H%M%S\r\n", tm_info);
        send(c->client_sock, resp, strlen(resp), 0);
    }
    else
    {
        const char *resp = "550 Could not get file time\r\n";
        send(c->client_sock, resp, strlen(resp), 0);
    }
}

/* ==== Command table ==== */
typedef struct
{
    const char *cmd;
    ftp_cmd_handler_t handler;
} ftp_cmd_entry_t;

static const ftp_cmd_entry_t ftp_cmd_table[] = {
    {"USER", ftp_cmd_user},
    {"PASS", ftp_cmd_pass},
    {"SYST", ftp_cmd_syst},
    {"QUIT", ftp_cmd_quit},
    {"PWD", ftp_cmd_pwd},
    {"XPWD", ftp_cmd_pwd},
    {"TYPE", ftp_cmd_type},
    {"NOOP", ftp_cmd_noop},
    {"AUTH", ftp_cmd_auth},
    {"PASV", ftp_cmd_pasv},
    {"LIST", ftp_cmd_list},
    {"CWD", ftp_cmd_cwd},
    {"CDUP", ftp_cmd_cdup},
    {"RETR", ftp_cmd_retr},
    {"STOR", ftp_cmd_stor},
    {"DELE", ftp_cmd_dele},
    {"RMD", ftp_cmd_rmd},
    {"MKD", ftp_cmd_mkd},
    {"SIZE", ftp_cmd_size},
    {"MDTM", ftp_cmd_mdtm},
    {"PORT", ftp_cmd_port},
    {NULL, NULL}};

/* ==== Server public functions ==== */
int ftp_server_init(ftp_server_t *srv, const char *root_path)
{
    memset(srv, 0, sizeof(*srv));
    snprintf(srv->root_path, sizeof(srv->root_path), "%s", root_path);

    for (int i = 0; i < FTP_MAX_CLIENTS; i++)
    {
        srv->clients[i].client_sock = -1;
        srv->clients[i].pasv_listen_sock = -1;
        srv->clients[i].pasv_data_sock = -1;
        snprintf(srv->clients[i].cwd, sizeof(srv->clients[i].cwd), "/");
    }

    srv->listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (srv->listen_sock < 0)
    {
        ESP_LOGE(TAG_FTP, "Unable to create socket");
        return -1;
    }

    int opt = 1;
    setsockopt(srv->listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(FTP_CTRL_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(srv->listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        ESP_LOGE(TAG_FTP, "Socket bind failed");
        close(srv->listen_sock);
        return -1;
    }

    if (listen(srv->listen_sock, FTP_MAX_CLIENTS) < 0)
    {
        ESP_LOGE(TAG_FTP, "Socket listen failed");
        close(srv->listen_sock);
        return -1;
    }

    // set non-blocking
    int flags = fcntl(srv->listen_sock, F_GETFL, 0);
    fcntl(srv->listen_sock, F_SETFL, flags | O_NONBLOCK);

    ESP_LOGI(TAG_FTP, "FTP server started on port %d, root=%s",
             FTP_CTRL_PORT, srv->root_path);
    return 0;
}
void ftp_server_tick(ftp_server_t *srv)
{
    // === Accept new clients (non-blocking) ===
    struct sockaddr_in6 source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int new_sock = accept(srv->listen_sock,
                          (struct sockaddr *)&source_addr,
                          &addr_len);
    if (new_sock >= 0)
    {
        int assigned = 0;
        for (int i = 0; i < FTP_MAX_CLIENTS; i++)
        {
            if (srv->clients[i].client_sock < 0)
            {
                ftp_client_t *c = &srv->clients[i];
                c->client_sock = new_sock;
                c->active = 1;
                c->pasv_listen_sock = -1;
                c->pasv_data_sock = -1;
                snprintf(c->cwd, sizeof(c->cwd), "/");

                const char *welcome = "220 ESP32 FTP Server Ready\r\n";
                send(new_sock, welcome, strlen(welcome), 0);

                ESP_LOGI(TAG_FTP, "Client %d connected", i);
                assigned = 1;
                break;
            }
        }
        if (!assigned)
        {
            ESP_LOGW(TAG_FTP, "Too many clients, rejecting");
            close(new_sock);
        }
    }
    else if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        ESP_LOGE(TAG_FTP, "accept() error: %d", errno);
    }

    // === Process existing clients ===
    for (int i = 0; i < FTP_MAX_CLIENTS; i++)
    {
        ftp_client_t *c = &srv->clients[i];
        if (c->client_sock < 0)
            continue;

        int len = recv(c->client_sock, c->buffer,
                       sizeof(c->buffer) - 1, MSG_DONTWAIT);

        if (len > 0)
        {
            c->buffer[len] = 0;
            char *cmd = strtok(c->buffer, " \r\n");
            char *args = strtok(NULL, "\r\n");
            if (!cmd)
                continue;

            const ftp_cmd_entry_t *entry = ftp_cmd_table;
            while (entry->cmd)
            {
                if (strcasecmp(entry->cmd, cmd) == 0)
                {
                    entry->handler(srv, c, args ? args : "");
                    break;
                }
                entry++;
            }
            if (!entry->cmd)
            {
                ftp_cmd_unknown(srv, c, cmd);
            }
        }
        else if (len == 0)
        {
            ESP_LOGI(TAG_FTP, "Client %d disconnected", i);
            close(c->client_sock);
            c->client_sock = -1;
            c->active = 0;
            ftp_close_data_connection(c);
        }
        else
        { // len < 0
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                ESP_LOGW(TAG_FTP, "Client %d socket error (errno=%d)", i, errno);
                close(c->client_sock);
                c->client_sock = -1;
                c->active = 0;
                ftp_close_data_connection(c);
            }
        }
    }
}
