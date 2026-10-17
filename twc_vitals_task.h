// =============================================================================
// JP fork 差分 #11（2026-10-15）：TWC3 /api/1/vitals の取得を専用タスクで行う
//
// 経緯：上流は interval + http_request.get で vitals を取っていた。ESPHome の
// http_request は応答待ちの間メインループを止める（timeout 2s でも接続と読み
// 取りで合計 3〜7.5 秒）。WC の応答が遅かった 2026-10-15 は、この停止で
//   (a) HA の API 接続が切れる（20〜60 分に 1 回）
//   (b) WC への RS485 返信を取りこぼす（Deferred server reply dropped、計 1950 回）
//   (c) vitals が古くなり Slow Meter Correlation が自動で無効になる
// が起きた。取得をこのタスク（コア 0）に移し、メインループは結果を写すだけにする。
//
// 使い方（twc-control.yaml）：
//   esphome.includes に本ファイル、on_boot で twc_vitals::start(url, ...)、
//   短い interval で twc_vitals::take(...) を呼んで globals に写す。
//
// スレッド安全性：共有データはスピンロック内でコピーするだけ。ログはこの
// タスクから出さない（メインループ側で出す）。
// =============================================================================
#pragma once

#include <string>
#include <cstring>

#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/components/json/json_util.h"

namespace twc_vitals {

struct Data {
  float a = 0.0f;
  float b = 0.0f;
  float c = 0.0f;
  float vehicle = 0.0f;
  bool contactor = false;
  uint32_t ok_seq = 0;     // 成功するたびに +1
  uint32_t fail_seq = 0;   // 失敗するたびに +1
  int last_err = 0;        // 最後の失敗の理由（esp_err_t か HTTP ステータス、-2 = JSON 不正）
  uint32_t last_ms = 0;    // 最後の 1 回にかかった時間 [ms]（診断用）
};

static Data g_data;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static std::string g_url;
static uint32_t g_period_ms = 2000;
static uint32_t g_timeout_ms = 1500;
static TaskHandle_t g_task = nullptr;

// 1 回取得して out に入れる。成功なら true、失敗なら err に理由。
static bool fetch_once_(Data &out, int &err) {
  static char buf[1536];
  esp_http_client_config_t cfg = {};
  cfg.url = g_url.c_str();
  cfg.timeout_ms = (int) g_timeout_ms;
  cfg.disable_auto_redirect = true;
  cfg.keep_alive_enable = false;

  esp_http_client_handle_t cl = esp_http_client_init(&cfg);
  if (cl == nullptr) {
    err = -1;
    return false;
  }
  bool ok = false;
  esp_err_t e = esp_http_client_open(cl, 0);
  if (e != ESP_OK) {
    err = (int) e;
  } else {
    esp_http_client_fetch_headers(cl);
    const int status = esp_http_client_get_status_code(cl);
    int n = 0;
    while (n < (int) sizeof(buf) - 1) {
      const int r = esp_http_client_read(cl, buf + n, sizeof(buf) - 1 - n);
      if (r <= 0)
        break;
      n += r;
    }
    buf[n] = '\0';
    if (status != 200 || n == 0) {
      err = status != 200 ? status : -3;
    } else {
      JsonDocument doc;
      if (deserializeJson(doc, buf, n)) {
        err = -2;
      } else {
        JsonObject root = doc.as<JsonObject>();
        if (root["currentA_a"].is<float>()) out.a = root["currentA_a"];
        if (root["currentB_a"].is<float>()) out.b = root["currentB_a"];
        if (root["currentC_a"].is<float>()) out.c = root["currentC_a"];
        if (root["vehicle_current_a"].is<float>()) out.vehicle = root["vehicle_current_a"];
        if (root["contactor_closed"].is<bool>()) out.contactor = root["contactor_closed"];
        ok = true;
      }
    }
  }
  esp_http_client_close(cl);
  esp_http_client_cleanup(cl);
  return ok;
}

static void task_(void *) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    Data got;
    portENTER_CRITICAL(&g_mux);
    got = g_data;  // 取れなかった項目は前回値のまま（上流の挙動と同じ）
    portEXIT_CRITICAL(&g_mux);

    const TickType_t t0 = xTaskGetTickCount();
    int err = 0;
    const bool ok = fetch_once_(got, err);
    const uint32_t took = (uint32_t) ((xTaskGetTickCount() - t0) * portTICK_PERIOD_MS);

    portENTER_CRITICAL(&g_mux);
    if (ok) {
      g_data.a = got.a;
      g_data.b = got.b;
      g_data.c = got.c;
      g_data.vehicle = got.vehicle;
      g_data.contactor = got.contactor;
      g_data.ok_seq++;
    } else {
      g_data.fail_seq++;
      g_data.last_err = err;
    }
    g_data.last_ms = took;
    portEXIT_CRITICAL(&g_mux);

    // 取得に周期より長くかかったら待たずに次へ（xTaskDelayUntil が即時に戻る）
    xTaskDelayUntil(&wake, pdMS_TO_TICKS(g_period_ms));
  }
}

// on_boot から 1 回だけ呼ぶ。
inline void start(const char *url, uint32_t period_ms, uint32_t timeout_ms) {
  if (g_task != nullptr)
    return;
  g_url = url;
  g_period_ms = period_ms;
  g_timeout_ms = timeout_ms;
  // コア 0（WiFi/lwIP と同じ側）、優先度 1。メインループ（ESPHome の loop）とは別。
  xTaskCreatePinnedToCore(task_, "twc_vitals", 8192, nullptr, 1, &g_task, 0);
}

// メインループから呼ぶ。最新の共有データを out にコピーする。
inline void take(Data &out) {
  portENTER_CRITICAL(&g_mux);
  out = g_data;
  portEXIT_CRITICAL(&g_mux);
}

}  // namespace twc_vitals
