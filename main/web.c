#include "include/web.h"
#include "include/stm32_uart.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "mdns.h"

#define TAG "WEB"
#define WEB_MAX_CLIENTS 7

static httpd_handle_t server = NULL;

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[] asm("_binary_index_html_end");

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    char *data;
};

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
}

static esp_err_t ws_handler(httpd_req_t *req) // source: Github esp-idf/examples/protocols/http_server/ws_echo_server
{
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;
    if (ws_pkt.len > 256) return ESP_FAIL;      // panel sends short lines only

    if (ws_pkt.len)
    {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) return ESP_ERR_NO_MEM;
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret == ESP_OK && ws_pkt.type == HTTPD_WS_TYPE_TEXT)
        {
            char *line = (char *)buf;
            if (strncmp(line, "SET ", 4) == 0 || strncmp(line, "GET ", 4) == 0)
            {
                if (stm32_send(line) < 0) ESP_LOGW(TAG, "Not sent: %s", line);
            }
            if (strcmp(line, "GET ALL") == 0)   // new client, tell it the link state
            {
                const char *link = stm32_online() ? "LINK up" : "LINK down";
                httpd_ws_frame_t resp = { .type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)link, .len = strlen(link) };
                httpd_ws_send_frame(req, &resp);
            }
        }
        free(buf);
    }
    return ret;
}

static void ws_async_send(void *arg) // source: Github esp-idf/examples/protocols/http_server/ws_echo_server
{
    struct async_resp_arg *resp_arg = arg;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)resp_arg->data;
    ws_pkt.len = strlen(resp_arg->data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    httpd_ws_send_frame_async(resp_arg->hd, resp_arg->fd, &ws_pkt);
    free(resp_arg->data);
    free(resp_arg);
}

void web_broadcast(const char *text) // source: Github esp-idf/examples/protocols/https_server/wss_server
{
    if (server == NULL) return;
    size_t clients = WEB_MAX_CLIENTS;
    int client_fds[WEB_MAX_CLIENTS];
    if (httpd_get_client_list(server, &clients, client_fds) != ESP_OK) return;
    for (size_t i = 0; i < clients; i++)
    {
        if (httpd_ws_get_fd_info(server, client_fds[i]) != HTTPD_WS_CLIENT_WEBSOCKET) continue;
        struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
        if (resp_arg == NULL) return;
        resp_arg->hd = server;
        resp_arg->fd = client_fds[i];
        resp_arg->data = strdup(text);
        if (resp_arg->data == NULL || httpd_queue_work(server, ws_async_send, resp_arg) != ESP_OK)
        {
            free(resp_arg->data);
            free(resp_arg);
        }
    }
}

int web_client_count(void) // number of open panels (WebSocket clients)
{
    if (server == NULL) return 0;
    size_t clients = WEB_MAX_CLIENTS;
    int client_fds[WEB_MAX_CLIENTS];
    if (httpd_get_client_list(server, &clients, client_fds) != ESP_OK) return 0;
    int n = 0;
    for (size_t i = 0; i < clients; i++)
        if (httpd_ws_get_fd_info(server, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) n++;
    return n;
}

static const httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
static const httpd_uri_t ws_uri = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };

static void mdns_start(void) // source: Github espressif/esp-protocols/components/mdns/examples
{
    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set("pedal");                          // http://pedal.local
    mdns_instance_name_set("GuitarFX");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
}

void web_init(void) // source: Github esp-idf/examples/protocols/http_server/ws_echo_server
{
    mdns_start();
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = WEB_MAX_CLIENTS;
    config.lru_purge_enable = true;             // phone asleep -> old socket gets reused
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &ws_uri);
    } else {
        ESP_LOGE(TAG, "Error starting server!");
    }
}
