#pragma once
#include "esp_log.h"
#include "esp_http_server.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"
#include "AppContext.h"
#include "JsonStream.h"

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

esp_err_t api_send_setting(JsonObjectWriter& writer, SettingHandle& setting)
{
    if (!setting)
        return ESP_FAIL;

    writer.field("name", setting.GetKey());

    switch (setting.GetType())
    {
    case SettingType::Integer: 
        writer.field("type", "integer");
        writer.field("value", setting.get<int32_t>());
        writer.field("default", setting.getDefault<int32_t>());
        break;
    case SettingType::Boolean:
        writer.field("type", "boolean");
        writer.field("value", setting.get<bool>());
        writer.field("default", setting.getDefault<bool>());
        break;
    case SettingType::Float:
        writer.field("type", "float");
        writer.field("value", setting.get<float>());
        writer.field("default", setting.getDefault<float>());
        break;
    case SettingType::String:{
        writer.field("type", "string");
        char* buf = setting.ptr<char>();
        writer.field("value", buf);
        writer.field("default", setting.getDefault<const char*>());
        break;
    }
    case SettingType::Enum:
        writer.field("type", "enum");
        writer.field("value", setting.get<int32_t>());
        writer.field("default", setting.getDefault<int32_t>());
        break;   
    
    default:
        writer.field("type", "unknown");
        break;
    }

    return ESP_OK;
}


esp_err_t api_settings_group(JsonObjectWriter& writer, const SettingGroupDescriptor* groupDesc) 
{
    if (!groupDesc) 
        return ESP_FAIL;

    writer.field("name", groupDesc->name);
    writer.withArray("settings", [groupDesc](JsonArrayWriter& arr) 
    {
        SettingsManager& settings = AppContext::GetAppContext().GetSettingsManager();
        auto accessor = settings.GetAccessor();

        for(int i=0; i < groupDesc->count; ++i) 
        {
            const SettingDescriptor* desc = &groupDesc->descriptors[i];
            auto setting = accessor.find(groupDesc, desc);
            if (!setting)
                continue;

            arr.withObject([&setting](JsonObjectWriter& obj) {
                api_send_setting(obj, setting);
            });
        }
    });
    return ESP_OK;
}


esp_err_t api_settings_handler_resp(httpd_req_t *req)
{
    ResponseStream respStream(req);
    JsonArrayWriter json(respStream);
    size_t groups = SettingsTraits<RootSettings>::DescriptorCount;

    for(int i=0; i < groups; ++i) 
    {
        const SettingGroupDescriptor* descriptor = &SettingsTraits<RootSettings>::Descriptors[i];
        json.withObject([descriptor](JsonObjectWriter& obj){
            api_settings_group(obj, descriptor);
        });
    }

    return ESP_OK;
}

esp_err_t api_settings_handler(httpd_req_t *req)
{
    SettingsManager& settings = AppContext::GetAppContext().GetSettingsManager();

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_type(req, "application/json");

    api_settings_handler_resp(req);

    httpd_resp_sendstr_chunk(req, NULL); // end response
    return ESP_OK;
}


esp_err_t api_settings_update_handler(httpd_req_t *req, SettingHandle& setting, cJSON* cValue)
{
    switch (setting.GetType()) {
        case SettingType::Integer:
        case SettingType::Enum: {
            if (!cJSON_IsNumber(cValue)) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Value must be a number");
                return ESP_FAIL;
            }
            setting.set<int32_t>(cValue->valueint);
            break;
        }

        case SettingType::Boolean: {
            if (!cJSON_IsBool(cValue)) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Value must be true/false");
                return ESP_FAIL;
            }
            setting.set<bool>(cJSON_IsTrue(cValue));
            break;
        }

        case SettingType::Float: {
            if (!cJSON_IsNumber(cValue)) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Value must be a number");
                return ESP_FAIL;
            }
            setting.set<float>(static_cast<float>(cValue->valuedouble));
            break;
        }

        default:
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unsupported type");
            return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t api_settings_update_handler(httpd_req_t *req) {
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *cContainer = cJSON_GetObjectItem(root, "container");
    cJSON *cName      = cJSON_GetObjectItem(root, "name");
    cJSON *cValue     = cJSON_GetObjectItem(root, "value");

    if (!cContainer || !cName || !cValue ||
        !cJSON_IsString(cContainer) || !cJSON_IsString(cName)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");
        return ESP_FAIL;
    }

    SettingsManager& settingsManager = AppContext::GetAppContext().GetSettingsManager();
    auto accessor = settingsManager.GetAccessor();
    auto handle   = accessor.find(cContainer->valuestring, cName->valuestring);

    if (!handle) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Container or setting not found");
        return ESP_FAIL;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t result = api_settings_update_handler(req, handle, cValue);

    cJSON_Delete(root);
    return result;
}

esp_err_t api_settings_command_handler(httpd_req_t *req) {
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Example payload: {"command":"xxx"}
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *cmdItem = cJSON_GetObjectItem(root, "command");
    if (!cJSON_IsString(cmdItem)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing command");
        return ESP_FAIL;
    }

    const char *command = cmdItem->valuestring;

    char response[256];

    if (strcmp(command, "CRES") == 0) {
        snprintf(response, sizeof(response), "{\"result\":\"Restarting\"}");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_sendstr(req, response);
        cJSON_Delete(root);
        esp_restart(); // will never return
        return ESP_OK;
    } 
    else if (strcmp(command, "DVER") == 0) {
        snprintf(response, sizeof(response), "{\"result\":\"%s\"}", "1.0.0");
    } 
    else if (strcmp(command, "CSAV") == 0) {
        SettingsManager& settings = AppContext::GetAppContext().GetSettingsManager();
        settings.SaveAll();
        snprintf(response, sizeof(response), "{\"result\":\"%s\"}", "OK");
    } 
    else {
        snprintf(response, sizeof(response), "{\"result\":\"%s\"}", "Command not found");
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, response);

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


        // Register API first
        httpd_uri_t api_settings = {
            .uri = "/api/settings",
            .method = HTTP_GET,
            .handler = api_settings_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &api_settings);


        httpd_uri_t api_settings_update = {
            .uri = "/api/settings",
            .method = HTTP_PUT,
            .handler = api_settings_update_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &api_settings_update);

        httpd_uri_t api_settings_command = {
            .uri = "/api/command",
            .method = HTTP_POST,
            .handler = api_settings_command_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &api_settings_command);

        
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
