#include "wifi_setup.h"
#include "app_config.h"
#include "wifi_credentials.h"
#include "wifi_networks.h"
#include "wifi_setup_diagnostics.h"
#include "wifi_setup_screen.h"
#include "wifi_setup_web.h"
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
const char *WifiSetup::webState() const {
  return success_ ? "success" : testing_ ? "connecting" : failed_ ? "failed" : "configuration";
}
void WifiSetup::sendPage(int code, bool configuration) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader(
      "Content-Security-Policy",
      "default-src 'none'; style-src 'unsafe-inline'; connect-src 'self'; script-src 'nonce-" +
          token_ + "'; form-action 'self'; frame-ancestors 'none'");
  const String state = configuration && !testing_ && !success_ ? "configuration" : webState();
  String page = provisioning::WifiPageHead;
  if (state == "success")
    page += provisioning::WifiSuccessContent;
  else if (state == "connecting")
    page += provisioning::WifiConnectingContent;
  else if (state == "failed")
    page += provisioning::WifiFailureContent;
  else {
    page += "<h1>Connect your display</h1><p>Choose your Wi-Fi network.</p>"
            "<form method=post action=/connect><input type=hidden name=token value='" +
            token_ +
            "'><label>Wi-Fi network<select name=network><option value=''>Choose a network</option>";
    bool selectedInList = false;
    const int count = WiFi.scanComplete();
    std::vector<provisioning::WifiNetwork> networks;
    for (int i = 0; i < count; ++i) {
      const String name = WiFi.SSID(i);
      provisioning::addWifiNetwork(networks, std::string(name.c_str(), name.length()),
                                   WiFi.RSSI(i));
    }
    provisioning::sortWifiNetworks(networks);
    // Apply the existing display limit after deduplication and signal sorting.
    for (size_t i = 0; i < networks.size() && i < 40; ++i) {
      const String name(networks[i].ssid.c_str());
      const bool selected = name == selectedSsid_;
      selectedInList = selectedInList || selected;
      const String safe = escape(name);
      page +=
          "<option value='" + safe + "'" + (selected ? " selected" : "") + ">" + safe + "</option>";
    }
    page +=
        "</select></label>"
        "<button type=button class=link data-manual aria-controls=manual-network "
        "aria-expanded=false>Can’t find your network? Enter it manually</button>"
        "<div id=manual-network hidden><label>Network name (manual entry)<input name=ssid "
        "maxlength=32 "
        "autocomplete=off value='" +
        (selectedInList ? String("") : escape(selectedSsid_)) +
        "'></label><button type=button class=link data-scanned>Use scanned network list</button>"
        "</div><label for=wifi-password>Password</label><div class=password-field>"
        "<input id=wifi-password name=password type=password maxlength=64 autocomplete=off>"
        "<button type=button data-password aria-controls=wifi-password aria-label='Show password' "
        "aria-pressed=false><svg viewBox='0 0 24 24' width=24 height=24 aria-hidden=true "
        "fill=none stroke=currentColor stroke-width=2 stroke-linecap=round stroke-linejoin=round>"
        "<path d='M2.062 12a1 1 0 0 1 0-.694 10.75 10.75 0 0 1 19.876 0 "
        "1 1 0 0 1 0 .694 10.75 10.75 0 0 1-19.876 0'/><circle cx='12' cy='12' r='3'/>"
        "<path data-password-slash visibility=hidden d='m3 3 18 18'/>"
        "</svg></button></div><button>Connect</button></form>";
  }
  page += "</main><script nonce='" + token_ + "'>const token='" + token_ +
          "';const initialState='" + state + "';" + provisioning::WifiSetupScript +
          "</script></body></html>";
  server_.send(code, "text/html; charset=utf-8", page);
}
void WifiSetup::connect(const String &ssid, const String &password, uint64_t now) {
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    esp_wifi_scan_stop();
  WiFi.disconnect(false, false);
  disconnectReason_ = 0;
  reportedReason_ = 0;
  // begin() can return the previous asynchronous status; it is not the final result.
  const auto result = WiFi.begin(ssid.c_str(), password.c_str());
  attemptStarted_ = true;
  Serial.printf("Wi-Fi: connection attempt started; status=%d\n", static_cast<int>(result));
  attemptAt_ = now;
  retryAt_ = now + 15000;
}
bool WifiSetup::authorized() {
  const String origin = server_.header("Origin");
  const char *rejection =
      server_.client().localIP() != WiFi.softAPIP()            ? "wrong interface"
      : !provisioning::setupHost(server_.hostHeader().c_str()) ? "unexpected Host"
      : !provisioning::setupOrigin(origin.c_str())             ? "unexpected Origin"
      : (server_.arg("token").length() ? server_.arg("token") : server_.header("X-Setup-Token")) !=
              token_
          ? "missing or stale form token"
          : nullptr;
  if (rejection) {
    Serial.printf("Wi-Fi setup: HTTP request rejected (%s)\n", rejection);
    message("Setup session expired or request address was invalid. Reopen http://192.168.4.1 and "
            "retry.");
    server_.send(
        403, "text/html",
        "<p>Setup session expired or address was invalid.</p><a href=/>Reopen setup and retry</a>");
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
  if (!storageReady_)
    Serial.println("Wi-Fi setup: NVS unavailable; saved credentials will not be erased");
  WiFi.onEvent(
      [this](arduino_event_id_t, arduino_event_info_t info) {
        const uint16_t reason = info.wifi_sta_disconnected.reason;
        // Ignore the intentional disconnect used when changing station credentials.
        if (reason != WIFI_REASON_ASSOC_LEAVE)
          disconnectReason_ = reason;
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
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
  const char *headers[] = {"Origin", "X-Setup-Token"};
  server_.collectHeaders(headers, 2);
  server_.on("/", HTTP_GET, [this]() {
    if (server_.client().localIP() != WiFi.softAPIP() ||
        !provisioning::setupHost(server_.hostHeader().c_str())) {
      server_.send(400, "text/plain", "Open http://192.168.4.1");
      return;
    }
    sendPage(200, server_.hasArg("configure"));
  });
  server_.on("/status", HTTP_GET, [this]() {
    if (!authorized())
      return;
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", String("{\"state\":\"") + webState() + "\"}");
  });
  server_.on("/complete", HTTP_POST, [this]() {
    if (!authorized())
      return;
    if (!success_) {
      server_.send(409, "text/plain", "");
      return;
    }
    if (!successPageSent_) {
      successPageSent_ = true;
      finishAt_ = nowMs() + 5000;
      Serial.println("Wi-Fi setup: success rendered; closing AP in five seconds");
    }
    server_.send(204, "text/plain", "");
  });
  server_.on("/connect", HTTP_POST, [this]() {
    Serial.println("Wi-Fi setup: HTTP configuration request received");
    if (!authorized())
      return;
    if (testing_ || success_) {
      Serial.println("Wi-Fi setup: HTTP request deferred; connection already in progress");
      sendPage(409);
      return;
    }
    String ssid = server_.arg("ssid");
    if (!ssid.length())
      ssid = server_.arg("network");
    Serial.printf("Wi-Fi setup: SSID received \"%s\"\n",
                  provisioning::diagnosticSsid(std::string(ssid.c_str(), ssid.length())).c_str());
    if (ssid.length() <= 32)
      selectedSsid_ = ssid;
    const String password = server_.arg("password");
    const bool valid = provisioning::validCredentials(
        std::string(ssid.c_str(), ssid.length()), std::string(password.c_str(), password.length()));
    if (!valid) {
      failed_ = true;
      Serial.println("Wi-Fi setup: invalid SSID/password format; connection not started");
      message("Invalid network name or password format. Use a 1-32 byte SSID and an empty, 8-63 "
              "byte or 64 hex digit password.");
      sendPage(400);
      return;
    }
    failed_ = false;
    browserSubmission_ = true;
    candidateSsid_ = ssid;
    candidatePassword_ = password;
    testing_ = true;
    attemptStarted_ = false;
    connectAt_ = nowMs() + 1000;
    message("Connecting. This page refreshes automatically; allow up to 20 seconds after the "
            "attempt starts.");
    // Return the progress page before changing the radio channel or station state.
    sendPage();
    Serial.println("Wi-Fi setup: progress response sent; connection queued");
  });
  server_.onNotFound([this]() {
    Serial.println("Wi-Fi setup: unrecognized HTTP route or method");
    server_.send(404, "text/plain", "Open http://192.168.4.1");
  });
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
  browserSubmission_ = false;
  failed_ = false;
  selectedSsid_ = "";
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
    Serial.println("Wi-Fi setup: AP and configuration page ready");
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  } else {
    Serial.println("Wi-Fi setup: AP start failed");
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
  if (testing_ && !attemptStarted_ && now >= connectAt_)
    connect(candidateSsid_, candidatePassword_, nowMs());
  if (testing_ && attemptStarted_) {
    const uint16_t reason = disconnectReason_.load();
    if (reason && reason != reportedReason_) {
      Serial.printf("Wi-Fi setup: station disconnected; reason=%u (%s)\n", reason,
                    provisioning::connectionFailure(reason));
      reportedReason_ = reason;
    }
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == candidateSsid_ &&
        WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      Serial.println("Wi-Fi setup: connection successful; DHCP address acquired");
      Credentials record{};
      record.version = 1;
      candidateSsid_.toCharArray(record.ssid, sizeof(record.ssid));
      candidatePassword_.toCharArray(record.password, sizeof(record.password));
      // Commit the complete inactive record before changing the selected slot.
      // Neither association failure nor a partial candidate write removes the old record.
      const uint8_t nextSlot = activeSlot_ == 0 ? 1 : 0;
      const char *key = nextSlot == 1 ? "slot1" : "slot0";
      Credentials verified{};
      const char *saveFailure = nullptr;
      if (!storageReady_)
        saveFailure = "NVS unavailable";
      else if (storage_.putBytes(key, &record, sizeof(record)) != sizeof(record))
        saveFailure = "record write failed";
      else if (storage_.getBytes(key, &verified, sizeof(verified)) != sizeof(verified))
        saveFailure = "readback failed";
      else if (memcmp(&record, &verified, sizeof(record)) != 0)
        saveFailure = "readback mismatch";
      else if (storage_.putUChar("active", nextSlot) != 1)
        saveFailure = "activation write failed";
      const bool saved = saveFailure == nullptr;
      if (saved) {
        Serial.println("Wi-Fi setup: credentials saved successfully");
        activeSlot_ = nextSlot;
        savedSsid_ = candidateSsid_;
        savedPassword_ = candidatePassword_;
        stored_ = true;
        success_ = true;
        successPageSent_ = false;
        // Browser setup waits for its success response before starting the AP shutdown grace.
        // Compiled-credential migration has no browser response to wait for.
        finishAt_ = browserSubmission_ ? 0 : nowMs() + 5000;
        provisioning_ = false;
        xSemaphoreTake(mutex_, portMAX_DELAY);
        visible_ = false; // Arrivals resume even if the phone needs to rejoin for the success page.
        xSemaphoreGive(mutex_);
        message("Connected and saved. Setup will close; reconnect your phone to home Wi-Fi.");
      } else {
        failed_ = true;
        Serial.printf("Wi-Fi setup: credentials not saved (%s)\n", saveFailure);
        message("Could not save. Previous credentials retained; retry setup.");
        WiFi.disconnect(false, false);
      }
      testing_ = false;
      candidateSsid_ = "";
      candidatePassword_ = "";
    } else if (nowMs() - attemptAt_ >= 20000) {
      const String failure = provisioning::connectionFailure(disconnectReason_.load());
      Serial.printf("Wi-Fi setup: connection failed; status=%d; reason=%u (%s)\n",
                    static_cast<int>(WiFi.status()), disconnectReason_.load(), failure.c_str());
      failed_ = true;
      testing_ = false;
      candidateSsid_ = "";
      candidatePassword_ = "";
      WiFi.disconnect(false, false);
      message(failure + " Previous saved credentials are unchanged. Retry below.");
      WiFi.scanDelete();
      WiFi.scanNetworks(true);
    }
  }
  if (setup_ && success_ && finishAt_ != 0 && now >= finishAt_) {
    testing_ = false;
    candidateSsid_ = "";
    candidatePassword_ = "";
    server_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println("Wi-Fi setup: setup mode exited; returning to arrivals");
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
  if ((!setup_ || success_) && stored_ && WiFi.status() != WL_CONNECTED && now >= retryAt_)
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
