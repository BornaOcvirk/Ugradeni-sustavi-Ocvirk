#include "web_control.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "web_control";

static web_control_command_cb_t s_command_cb;
static web_control_status_cb_t s_status_cb;

static const char k_page[] =
    "<!doctype html><html lang=hr><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1,user-scalable=no'>"
    "<title>ESP32 Autic</title><style>"
    ":root{color-scheme:dark}"
    "body{margin:0;padding:16px;font:16px system-ui,sans-serif;background:#10151c;color:#e8eef6;"
    "text-align:center;-webkit-user-select:none;user-select:none}"
    "h1{font-size:20px;margin:4px 0 14px;letter-spacing:.08em}"
    "#pad{display:grid;grid-template-columns:repeat(3,88px);grid-gap:8px;justify-content:center;margin:10px 0}"
    "button{background:#1d2836;color:#e8eef6;border:1px solid #2f4055;border-radius:12px;"
    "height:74px;font-size:15px;font-weight:600;cursor:pointer;touch-action:manipulation}"
    "button:active{background:#2f7cf6;border-color:#2f7cf6}"
    "#stop{background:#7a1d2a;border-color:#a4283a}"
    ".sp{visibility:hidden}"
    "table{margin:14px auto 0;border-collapse:collapse;font-size:14px;min-width:280px}"
    "td{padding:5px 10px;border-bottom:1px solid #22303f;text-align:left}"
    "td:last-child{text-align:right;font-variant-numeric:tabular-nums;color:#9fd0ff}"
    "#warn{margin-top:12px;padding:9px;border-radius:9px;background:#7a1d2a;font-weight:600;display:none}"
    "#zero{height:42px;font-size:13px;margin-top:12px;width:184px}"
    "</style></head><body>"
    "<h1>ESP32 AUTIC</h1>"
    "<div id=pad>"
    "<button class=sp></button><button onclick=\"go('F')\">NAPRIJED</button><button class=sp></button>"
    "<button onclick=\"go('L')\">LIJEVO</button>"
    "<button id=stop onclick=\"go('S')\">STOP</button>"
    "<button onclick=\"go('R')\">DESNO</button>"
    "<button class=sp></button><button onclick=\"go('B')\">NATRAG</button><button class=sp></button>"
    "</div>"
    "<div id=warn>PREPREKA IZA - voznja natrag blokirana</div>"
    "<table>"
    "<tr><td>Stanje</td><td id=m>-</td></tr>"
    "<tr><td>Udaljenost straga</td><td id=d>-</td></tr>"
    "<tr><td>Prijedeni put</td><td id=o>-</td></tr>"
    "</table>"
    "<button id=zero onclick=\"go('Z')\">NULIRAJ PUT</button>"
    "<script>"
    "function go(c){fetch('/api/drive?cmd='+c)}"
    "document.addEventListener('keydown',e=>{var k={ArrowUp:'F',ArrowDown:'B',ArrowLeft:'L',"
    "ArrowRight:'R',' ':'S'}[e.key];if(k){e.preventDefault();go(k)}});"
    "function f(x,n){return x.toFixed(n)}"
    "async function tick(){try{var s=await (await fetch('/api/status')).json();"
    "m.textContent=s.motion;"
    "d.textContent=s.cm<0?'nema odjeka':f(s.cm,1)+' cm';"
    "warn.style.display=s.obstacle?'block':'none';"
    "o.textContent=f(s.odo,1)+' cm'}catch(e){m.textContent='veza prekinuta'}}"
    "setInterval(tick,400);tick()"
    "</script></body></html>";

static esp_err_t index_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, k_page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t drive_get(httpd_req_t *req)
{
    char query[32] = {0};
    char value[4] = {0};
    char command = 0;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "cmd", value, sizeof(value)) == ESP_OK) {
        command = value[0];
    }

    if (s_command_cb && command && strchr("FBLRSZ", command)) {
        s_command_cb(command);
        return httpd_resp_sendstr(req, "OK");
    }

    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "nepoznata naredba");
}

static esp_err_t status_get(httpd_req_t *req)
{
    web_control_status_t st = {.distance_cm = -1.0f, .motion = "?"};
    if (s_status_cb) {
        s_status_cb(&st);
    }

    char body[160];
    int n = snprintf(body, sizeof(body),
                     "{\"motion\":\"%s\",\"cm\":%.1f,\"obstacle\":%s,\"odo\":%.1f}",
                     st.motion ? st.motion : "?", (double)st.distance_cm,
                     st.obstacle ? "true" : "false", (double)st.odometry_cm);
    if (n < 0 || n >= (int)sizeof(body)) {
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, n);
}

esp_err_t web_control_start(const char *ssid, const char *password,
                            web_control_command_cb_t command_cb,
                            web_control_status_cb_t status_cb)
{
    ESP_RETURN_ON_FALSE(ssid && password, ESP_ERR_INVALID_ARG, TAG, "null ssid/pass");

    s_command_cb = command_cb;
    s_status_cb = status_cb;

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");

    esp_err_t loop = esp_event_loop_create_default();
    ESP_RETURN_ON_FALSE(loop == ESP_OK || loop == ESP_ERR_INVALID_STATE, loop, TAG, "event loop");
    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_ap(), ESP_FAIL, TAG, "AP netif");

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi init");

    wifi_config_t ap = {
        .ap = {
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s", ssid);
    snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", password);
    ap.ap.ssid_len = (uint8_t)strnlen((const char *)ap.ap.ssid, sizeof(ap.ap.ssid));
    if (strlen(password) < 8) {
        ESP_LOGW(TAG, "lozinka kraca od 8 znakova, mreza ce biti otvorena");
        ap.ap.authmode = WIFI_AUTH_OPEN;
        ap.ap.password[0] = '\0';
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap), TAG, "ap config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");

    esp_err_t txp = esp_wifi_set_max_tx_power(52);
    if (txp != ESP_OK) {
        ESP_LOGW(TAG, "snaga odasiljanja nije postavljena (%s)", esp_err_to_name(txp));
    }

    httpd_handle_t server = NULL;
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.lru_purge_enable = true;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &http_config), TAG, "httpd");

    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_get},
        {.uri = "/api/drive", .method = HTTP_GET, .handler = drive_get},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &routes[i]), TAG, "uri %s",
                            routes[i].uri);
    }

    ESP_LOGI(TAG, "AP '%s' aktivan, sucelje na http://192.168.4.1", ssid);
    return ESP_OK;
}
