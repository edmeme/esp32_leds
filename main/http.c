#include "http.h"
#include "webfiles.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"

#include <esp_http_server.h>

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "http";

typedef struct {
  const rgb_t * color;
  QueueHandle_t queue;
} server_state_t;

/* Serve a file from context */
static esp_err_t root_get_handler(httpd_req_t *req)
{
  const char* resp_str = (const char*) req->user_ctx;
  httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = web_file_index
};

static const httpd_uri_t js_picker= {
    .uri       = "/kellycolorpicker.js",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = web_file_kellycolorpicker
};

struct async_resp_arg {
  httpd_handle_t hd;
  int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
  static const char * data = "Async data";
  struct async_resp_arg *resp_arg = arg;
  httpd_handle_t hd = resp_arg->hd;
  int fd = resp_arg->fd;
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = (uint8_t*)data;
  ws_pkt.len = strlen(data);
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  
  httpd_ws_send_frame_async(hd, fd, &ws_pkt);
  free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
  struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
  resp_arg->hd = req->handle;
  resp_arg->fd = httpd_req_to_sockfd(req);
  return httpd_queue_work(handle, ws_async_send, resp_arg);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
  server_state_t * state = (server_state_t*)
    httpd_get_global_user_ctx(req->handle);
  uint8_t buf[128] = { 0 };
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = buf;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
    return ret;
  }
  ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
  ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
  if (ws_pkt.type == HTTPD_WS_TYPE_TEXT){
    // On new connection, send the current rgb value
    if(strcmp((char*)ws_pkt.payload,"get") == 0) {
      uint8_t out[128];
      if(!state){
	ESP_LOGE(TAG, "No context found");
      }
      int r=state->color->r;
      int g=state->color->g;
      int b=state->color->b;
      snprintf((char*)out,sizeof(out), "{\"r\":%d,\"g\":%d,\"b\":%d}", r,g,b);
      ws_pkt.payload = out;
      ws_pkt.len = strlen((char*)out);
      ESP_LOGI(TAG, "New connection on ws, sending color.");
      return httpd_ws_send_frame(req, &ws_pkt);
    }

    if(ws_pkt.payload[0] == '[') {
      ESP_LOGI(TAG, "Got color %s.", ws_pkt.payload);
      char * data = (char*) ws_pkt.payload;
      char * parts[3] = {NULL,NULL,NULL};
      int ipart = 0;
      for(char *p = data; *p;++p){
	if(*p == ']' || *p == ',' || *p == '['){
	  *p = 0;
	  if(ipart < 3){
	    parts[ipart++] = p+1;
	  }
	}
      }      
      if(ipart == 3){
	ESP_LOGI(TAG, "red: %s, green: %s, blue %s.",
		 parts[0], parts[1], parts[2]);
	web_color_event_t msg;
	msg.id0 = WEB_COLOR_EVID;
	msg.color.r = atoi(parts[0]);
	msg.color.g = atoi(parts[1]);
	msg.color.b = atoi(parts[2]);
	BaseType_t task_woken = pdFALSE;
	xQueueOverwrite(state->queue, &msg);
      }
      return ESP_OK;
    }

    if(ws_pkt.payload[0] == '<') {
      ESP_LOGI(TAG, "Got corr %s.", ws_pkt.payload);
      char * data = (char*) ws_pkt.payload;
      char * parts[3] = {NULL,NULL,NULL};
      int ipart = 0;
      for(char *p = data; *p;++p){
	if(*p == '>' || *p == ',' || *p == '<'){
	  *p = 0;
	  if(ipart < 3){
	    parts[ipart++] = p+1;
	  }
	}
      }      
      if(ipart == 3){
	ESP_LOGI(TAG, "red: %s, green: %s, blue %s.",
		 parts[0], parts[1], parts[2]);
	web_calibration_event_t msg;
	msg.id0 = WEB_CALIBRATION_EVID;
	msg.color.r_scale = atoi(parts[0]);
	msg.color.g_scale = atoi(parts[1]);
	msg.color.b_scale = atoi(parts[2]);
	BaseType_t task_woken = pdFALSE;
	xQueueOverwrite(state->queue, &msg);
      }
      return ESP_OK;
    }
  }
  return ret;
}

static const httpd_uri_t ws = {
  .uri        = "/ws",
  .method     = HTTP_GET,
  .handler    = ws_handler,
  .user_ctx   = NULL,
  .is_websocket = true
};

static httpd_handle_t start_webserver(const rgb_t * color, QueueHandle_t queue)
{
  server_state_t * state = (server_state_t *) calloc(1, sizeof(server_state_t));
  state->color = color;
  state->queue = queue;

  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.global_user_ctx = state;
  config.global_user_ctx_free_fn = free;
  
  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &js_picker);
    httpd_register_uri_handler(server, &ws);
    return server;
  }

  ESP_LOGI(TAG, "Error starting server!");
  return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
  // Stop the httpd server
  httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
  httpd_handle_t* server = (httpd_handle_t*) arg;
  if (*server) {
    ESP_LOGI(TAG, "Stopping webserver");
    stop_webserver(*server);
    *server = NULL;
  }
}

// Filthy
static const rgb_t * g_color;
static QueueHandle_t g_queue;

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
  httpd_handle_t* server = (httpd_handle_t*) arg;
  if (*server == NULL) {
    ESP_LOGI(TAG, "Starting webserver");
    *server = start_webserver(g_color, g_queue);
  }
}

void init_httpd(const rgb_t * color, QueueHandle_t queue)
{
  g_color = color;
  g_queue = queue;
  static httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
  server = start_webserver(color, queue);
}
