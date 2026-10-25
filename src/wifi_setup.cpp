#include "wifi_setup.h"
#include "app_config.h"
#include "wifi_credentials.h"
#include "wifi_setup_screen.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>

namespace {
uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
String randomCode() {
  char code[9];
  snprintf(code, sizeof(code), "%08lX", static_cast<unsigned long>(esp_random()));
  return code;
}
String escape(const String &value) {
  String out;
  for (unsigned i = 0; i < value.length(); ++i) {
    switch (value[i]) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&#39;";
      break;
    default:
      out += value[i];
    }
  }
  return out;
}
using provisioning::Credentials;
} // namespace
void WifiSetup::message(const String &status) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  status_ = status;
  xSemaphoreGive(mutex_);
}
void WifiSetup::connect(const String &ssid, const String &password, uint64_t now) {
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    esp_wifi_scan_stop();
  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(), password.c_str());
  attemptAt_ = now;
  retryAt_ = now + 15000;
}
bool WifiSetup::authorized() {
  const String origin = server_.header("Origin");
  if (server_.client().localIP() != WiFi.softAPIP() || server_.hostHeader() != "192.168.4.1" ||
      (origin.length() && origin != "http://192.168.4.1") || server_.arg("token") != token_) {
    server_.send(403, "text/plain", "Reopen the setup page.");
    return false;
  }
  return true;
}
bool WifiSetup::initialize() {
  mutex_ = xSemaphoreCreateMutex();
  return mutex_ != nullptr;
}
void WifiSetup::begin() {
  if (!mutex_)
    return;
  storageReady_ = storage_.begin("transit-wifi", false);
  Credentials record{};
  activeSlot_ = storageReady_ ? storage_.getUChar("active", 0) : 0;
  const char *key = activeSlot_ == 1 ? "slot1" : "slot0";
  if (storageReady_ && activeSlot_ <= 1 && storage_.getBytesLength(key) == sizeof(record) &&
      storage_.getBytes(key, &record, sizeof(record)) == sizeof(record) &&
      provisioning::validRecord(record)) {
    savedSsid_ = record.ssid;
    savedPassword_ = record.password;
    stored_ = true;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // This task owns bounded retries and candidate testing.
  const char *headers[] = {"Origin"};
  server_.collectHeaders(headers, 1);
  server_.on("/", HTTP_GET, [this]() {
    if (server_.client().localIP() != WiFi.softAPIP() || server_.hostHeader() != "192.168.4.1") {
      server_.send(400, "text/plain", "Open http://192.168.4.1");
      return;
    }
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; "
                                                  "form-action 'self'; frame-ancestors 'none'");
    String page = "<!doctype html><meta name=viewport "
                  "content='width=device-width,initial-scale=1'><title>Display "
                  "Wi-Fi</title><style>body{font:18px system-ui;max-width:32rem;margin:2rem "
                  "auto;padding:1rem}input,select,button{font:inherit;box-sizing:border-box;width:"
                  "100%;padding:.8rem;margin:.4rem 0 1rem}label{display:block}</style><h1>Connect "
                  "your display</h1><p>";
    if (testing_)
      page += "<meta http-equiv=refresh content=2>";
    page += escape(status_) +
            "</p><p>Use a 2.4 GHz network. This page works without internet.</p><form method=post "
            "action=/connect><input type=hidden name=token value='" +
            token_ +
            "'><label>Available networks<select name=network><option value=''>Enter a name "
            "below</option>";
    const int count = WiFi.scanComplete();
    if (count >= 0) {
      for (int i = 0; i < count && i < 40; ++i) {
        const String ssid = escape(WiFi.SSID(i));
        page += "<option value='" + ssid + "'>" + ssid + "</option>";
      }
    }
    page +=
        "</select></label><label>Network name (overrides selection)<input name=ssid maxlength=32 "
        "autocomplete=off></label><label>Password<input name=password type=password maxlength=64 "
        "autocomplete=off></label><button>Connect</button></form><p><a href=/>Refresh networks and "
        "status</a></p><p>After success, reconnect your phone to your usual Wi-Fi.</p>";
    server_.send(200, "text/html; charset=utf-8", page);
  });
  server_.on("/connect", HTTP_POST, [this]() {
    if (!authorized())
      return;
    if (testing_ || success_) {
      server_.send(409, "text/plain", "Connection in progress. Return to the setup page.");
      return;
    }
    String ssid = server_.arg("ssid");
    if (!ssid.length())
      ssid = server_.arg("network");
    const String password = server_.arg("password");
    const bool valid = provisioning::validCredentials(
        std::string(ssid.c_str(), ssid.length()), std::string(password.c_str(), password.length()));
    if (!valid) {
      server_.send(400, "text/plain",
                   "Use a 1-32 byte SSID and an empty, 8-63 byte, or 64 hex digit password. Go "
                   "back to retry.");
      return;
    }
    candidateSsid_ = ssid;
    candidatePassword_ = password;
    testing_ = true;
    message("Connecting. This page refreshes automatically; allow up to 20 seconds.");
    connect(candidateSsid_, candidatePassword_, nowMs());
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("Location", "/");
    server_.send(303);
  });
  server_.onNotFound([this]() { server_.send(404, "text/plain", "Open http://192.168.4.1"); });
  if (stored_)
    connect(savedSsid_, savedPassword_, nowMs());
  else {
    start();
    // Transitional development credentials are a candidate, never a permanent prerequisite.
    if (storageReady_ && !storage_.isKey("active") && !storage_.isKey("slot0") &&
        !storage_.isKey("slot1") && localcfg::WifiSsid[0]) {
      candidateSsid_ = localcfg::WifiSsid;
      candidatePassword_ = localcfg::WifiPassword;
      testing_ = true;
      connect(candidateSsid_, candidatePassword_, nowMs());
    }
  }
}
void WifiSetup::start() {
  if (setup_)
    return;
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);
  xSemaphoreTake(mutex_, portMAX_DELAY);
  apName_ = provisioning::SetupSsid;
  apPassword_ = provisioning::SetupPassword;
  token_ = randomCode() + randomCode() + randomCode() + randomCode();
  status_ = "Choose your network, then connect.";
  visible_ = true;
  xSemaphoreGive(mutex_);
  provisioning_ = true;
  setup_ = WiFi.softAP(apName_.c_str(), apPassword_.c_str(), 1, false, 1);
  if (setup_) {
    server_.begin();
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  } else {
    provisioning_ = false;
    message("Setup failed. Hold BOOT to retry.");
  }
}
void WifiSetup::tick(uint64_t now) {
  if (!mutex_)
    return;
  if (requested_.exchange(false))
    start();
  if (setup_)
    server_.handleClient();
  if (testing_) {
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == candidateSsid_ &&
        WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      Credentials record{};
      record.version = 1;
      candidateSsid_.toCharArray(record.ssid, sizeof(record.ssid));
      candidatePassword_.toCharArray(record.password, sizeof(record.password));
      // Commit the complete inactive record before changing the selected slot.
      // Neither association failure nor a partial candidate write removes the old record.
      const uint8_t nextSlot = activeSlot_ == 0 ? 1 : 0;
      const char *key = nextSlot == 1 ? "slot1" : "slot0";
      Credentials verified{};
      const bool saved = storageReady_ &&
                         storage_.putBytes(key, &record, sizeof(record)) == sizeof(record) &&
                         storage_.getBytes(key, &verified, sizeof(verified)) == sizeof(verified) &&
                         memcmp(&record, &verified, sizeof(record)) == 0 &&
                         storage_.putUChar("active", nextSlot) == 1;
      if (saved) {
        activeSlot_ = nextSlot;
        savedSsid_ = candidateSsid_;
        savedPassword_ = candidatePassword_;
        stored_ = true;
        success_ = true;
        finishAt_ = now + 5000;
        message("Connected and saved. Setup will close; reconnect your phone to home Wi-Fi.");
      } else {
        message("Could not save. Previous credentials retained; retry setup.");
        WiFi.disconnect(false, false);
      }
      testing_ = false;
      candidateSsid_ = "";
      candidatePassword_ = "";
    } else if (now - attemptAt_ >= 20000) {
      testing_ = false;
      candidateSsid_ = "";
      candidatePassword_ = "";
      WiFi.disconnect(false, false);
      message("Connection failed. Check the password and retry.");
      WiFi.scanDelete();
      WiFi.scanNetworks(true);
    }
  }
  if (setup_ && success_ && now >= finishAt_) {
    testing_ = false;
    candidateSsid_ = "";
    candidatePassword_ = "";
    server_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    setup_ = false;
    provisioning_ = false;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    visible_ = false;
    apPassword_ = "";
    token_ = "";
    xSemaphoreGive(mutex_);
    success_ = false;
    if (stored_ && WiFi.status() != WL_CONNECTED)
      connect(savedSsid_, savedPassword_, now);
  }
  if (!setup_ && stored_ && WiFi.status() != WL_CONNECTED && now >= retryAt_)
    connect(savedSsid_, savedPassword_, now);
}
bool WifiSetup::render(transit::Framebuffer &frame) {
  if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
    return false;
  const bool visible = visible_;
  if (visible)
    provisioning::renderSetupScreen(frame, apName_.c_str(), apPassword_.c_str());
  xSemaphoreGive(mutex_);
  return visible;
}
