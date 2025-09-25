#pragma once
#include "esp_log.h"
#include "esp_http_server.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"
#include "AppContext.h"
#include "json.h"
#include <dirent.h>


constexpr char *TAG_HTTP = "Http";

class ResponseStream : public Stream {
    httpd_req_t* req;
public:
    explicit ResponseStream(httpd_req_t* r) : req(r) {}

    size_t write(const void* data, size_t len) override {
        int ret = httpd_resp_send_chunk(req, (const char*)data, len);
        if (ret == ESP_OK) {
            return len;
        }
        return 0;
    }

    size_t read(void* buffer, size_t len) override {
        assert(false && "ResponseStream does not support read()");
        return 0; // not supported
    }

    void flush() override {
        // no-op
	}
};


esp_err_t file_get_handler(httpd_req_t *req)
{
    char filepath[256] = "/fat";

    if (strcmp(req->uri, "/") == 0)
    {
        strcat(filepath, "/index.html");
    }
    else
    {
        strcat(filepath, req->uri);
    }

    // First try to serve a .gz version (if it exists)
    char gz_filepath[260];
    snprintf(gz_filepath, sizeof(gz_filepath), "%s.gz", filepath);
    FILE *f = fopen(gz_filepath, "r");
    bool is_gz = false;

    if (f)
    {
        is_gz = true;
        strcpy(filepath, gz_filepath);
    }
    else
    {
        f = fopen(filepath, "r");
    }

    if (!f)
    {
        // SPA fallback: always return index.html for unknown paths
        strcpy(filepath, "/fat/index.html");
        f = fopen(filepath, "r");
        if (!f)
        {
            ESP_LOGE(TAG_HTTP, "File not found: %s", filepath);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
            return ESP_FAIL;
        }
        httpd_resp_set_type(req, "text/html");
    }
    else
    {
        // Set content type based on extension
        if (strstr(filepath, ".html"))
            httpd_resp_set_type(req, "text/html");
        else if (strstr(filepath, ".js"))
            httpd_resp_set_type(req, "application/javascript");
        else if (strstr(filepath, ".css"))
            httpd_resp_set_type(req, "text/css");
        else if (strstr(filepath, ".png"))
            httpd_resp_set_type(req, "image/png");
        else if (strstr(filepath, ".jpg"))
            httpd_resp_set_type(req, "image/jpeg");
        else if (strstr(filepath, ".ico"))
            httpd_resp_set_type(req, "image/x-icon");
        else if (strstr(filepath, ".svg"))
            httpd_resp_set_type(req, "image/svg+xml");
        else if (strstr(filepath, ".map"))
            httpd_resp_set_type(req, "application/json");
        else
            httpd_resp_set_type(req, "text/plain");

        // If serving gzipped file, tell the browser
        if (is_gz)
        {
            httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        }
    }

    // Stream file in chunks
    char chunk[1024];
    size_t read_bytes;
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), f)) > 0)
    {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK)
        {
            fclose(f);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }
    fclose(f);

    httpd_resp_sendstr_chunk(req, NULL); // End response
    return ESP_OK;
}

static void write_directory(JsonArrayWriter& arr, const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        arr.withObject([&](JsonObjectWriter& obj) {
            obj.field("name", entry->d_name);

            if (entry->d_type == DT_DIR) {
                obj.withArray("children", [&](JsonArrayWriter& subarr) {
                    write_directory(subarr, fullpath);
                });
            } else {
                FILE* f = fopen(fullpath, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long size = ftell(f);
                    fclose(f);
                    obj.field("size", (int64_t)size);
                } else {
                    obj.field("size", (int64_t)-1);
                }
            }
        });
    }
    closedir(dir);
}

esp_err_t api_list_fat_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");

    ResponseStream rs(req);
    
    JsonObjectWriter::create(rs, [&](JsonObjectWriter& root) {
        root.field("filesystem", "FAT");
        root.field("path", "/fat");

        // List files
        root.withArray("files", [&](JsonArrayWriter& arr) {
            write_directory(arr, "/fat");
        });
    });

    // Finish chunked response
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t api_settings_options_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    return httpd_resp_send(req, NULL, 0);
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 16;
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {       
        httpd_uri_t list_uri = {
            .uri = "/api/files",
            .method = HTTP_GET,
            .handler = api_list_fat_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &list_uri);


        // Handle OPTIONS for CORS preflight
        httpd_uri_t api_settings_options = {
            .uri = "/*",
            .method = HTTP_OPTIONS,
            .handler = api_settings_options_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_settings_options);

        // Catch-all file handler
        httpd_uri_t file_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = file_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &file_uri);

        ESP_LOGI(TAG_HTTP, "HTTP server started");
    }
}
