#pragma once


#define FTP_CTRL_PORT 21
#define FTP_BUFFER_SIZE 512
#define FTP_MAX_CLIENTS 4

class FtpServer {
public:
    struct Client {
        int client_sock;
        int pasv_listen_sock;
        int pasv_data_sock;
        char cwd[128];
        char buffer[FTP_BUFFER_SIZE];
        int active;
    };

    FtpServer(const char* root_path);
    ~FtpServer();

    bool init();
    void tick();

private:
    // === Helpers ===
    int  openDataConnection(Client& c);
    void closeDataConnection(Client& c);

    // === FTP Command Handlers ===
    void ftp_cmd_user(Client &c, const char *args);
    void ftp_cmd_pass(Client &c, const char *args);
    void ftp_cmd_syst(Client &c, const char *args);
    void ftp_cmd_quit(Client &c, const char *args);
    void ftp_cmd_pwd(Client &c, const char *args);
    void ftp_cmd_list(Client &c, const char *args);
    void ftp_cmd_type(Client &c, const char *args);
    void ftp_cmd_noop(Client &c, const char *args);
    void ftp_cmd_auth(Client &c, const char *args);
    void ftp_cmd_pasv(Client &c, const char *args);
    void ftp_cmd_port(Client &c, const char *args);
    void ftp_cmd_cwd(Client &c, const char *args);
    void ftp_cmd_cdup(Client &c, const char *args);
    void ftp_cmd_retr(Client &c, const char *args);
    void ftp_cmd_stor(Client &c, const char *args);
    void ftp_cmd_dele(Client &c, const char *args);
    void ftp_cmd_rmd(Client &c, const char *args);
    void ftp_cmd_mkd(Client &c, const char *args);
    void ftp_cmd_size(Client &c, const char *args);
    void ftp_cmd_mdtm(Client &c, const char *args);

    struct CommandEntry {
        const char *cmd;
        void (FtpServer::*handler)(Client &c, const char *args);
    };

    static const CommandEntry cmdTable[];


private:
    int listen_sock;
    char root_path[128];
    Client clients[FTP_MAX_CLIENTS];
};
