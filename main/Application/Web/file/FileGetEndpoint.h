#pragma once
#include "HttpEndpoint.h"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

class FileGetEndpoint : public HttpEndpoint {
public:
    static constexpr size_t MAX_PATH_LEN  = 256;
    static constexpr size_t COPY_BUF_SIZE = 512;

    explicit FileGetEndpoint(const char* basePath)
        : basePath(basePath) {}

    esp_err_t handle(httpd_req_t* req) override {
        char filepath[MAX_PATH_LEN];
        if (!resolvePath(req->uri, filepath, sizeof(filepath))) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
            return ESP_FAIL;
        }

        setHeaders(req, filepath);
        return streamFile(req, filepath);
    }

    /// Utility: check if path ends with `.gz`
    static bool isGz(const char* filepath) {
        size_t len = strlen(filepath);
        return (len > 3 && strcmp(filepath + len - 3, ".gz") == 0);
    }

private:
    const char* basePath;

    bool fileExists(const char* path) const {
        struct stat st;
        return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
    }

    bool chooseFileOrGz(const char* candidate, char* outPath, size_t outSize) {
        char gzPath[MAX_PATH_LEN];
        snprintf(gzPath, sizeof(gzPath), "%s.gz", candidate);
        if (fileExists(gzPath)) {
            strncpy(outPath, gzPath, outSize);
            outPath[outSize - 1] = '\0';
            return true;
        }
        if (fileExists(candidate)) {
            strncpy(outPath, candidate, outSize);
            outPath[outSize - 1] = '\0';
            return true;
        }
        return false;
    }

    bool resolvePath(const char* uri, char* outPath, size_t outSize) {
        char candidate[MAX_PATH_LEN];

        // Try requested path
        snprintf(candidate, sizeof(candidate), "%s%s", basePath, uri);
        if (chooseFileOrGz(candidate, outPath, outSize)) {
            return true;
        }

        // Fallback to SPA index.html
        snprintf(candidate, sizeof(candidate), "%s/index.html", basePath);
        return chooseFileOrGz(candidate, outPath, outSize);
    }

    void setHeaders(httpd_req_t* req, const char* filepath) {
        const char* typePath = filepath;

        if (isGz(filepath)) {
            // remove ".gz" for MIME detection
            static char tmp[MAX_PATH_LEN];
            size_t len = strlen(filepath);
            strncpy(tmp, filepath, len - 3);
            tmp[len - 3] = '\0';
            typePath = tmp;

            httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        }

        if (strstr(typePath, ".html"))
            httpd_resp_set_type(req, "text/html");
        else if (strstr(typePath, ".js"))
            httpd_resp_set_type(req, "application/javascript");
        else if (strstr(typePath, ".css"))
            httpd_resp_set_type(req, "text/css");
        else if (strstr(typePath, ".png"))
            httpd_resp_set_type(req, "image/png");
        else if (strstr(typePath, ".jpg") || strstr(typePath, ".jpeg"))
            httpd_resp_set_type(req, "image/jpeg");
        else if (strstr(typePath, ".ico"))
            httpd_resp_set_type(req, "image/x-icon");
        else if (strstr(typePath, ".svg"))
            httpd_resp_set_type(req, "image/svg+xml");
        else if (strstr(typePath, ".map"))
            httpd_resp_set_type(req, "application/json");
        else
            httpd_resp_set_type(req, "text/plain");
    }

    esp_err_t streamFile(httpd_req_t* req, const char* filepath) {
        ESP_LOGI("FileGetEndpoint", "Serving file: %s", filepath);
        int fd = open(filepath, O_RDONLY);
        if (fd < 0) {
            ESP_LOGE("FileGetEndpoint", "Failed to open: %s", filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Open failed");
            return ESP_FAIL;
        }

        char buf[COPY_BUF_SIZE];
        ssize_t readBytes;
        while ((readBytes = read(fd, buf, sizeof(buf))) > 0) {
            if (httpd_resp_send_chunk(req, buf, readBytes) != ESP_OK) {
                close(fd);
                httpd_resp_sendstr_chunk(req, nullptr);
                return ESP_FAIL;
            }
        }
        close(fd);

        httpd_resp_sendstr_chunk(req, nullptr); // end response
        return ESP_OK;
    }
};
