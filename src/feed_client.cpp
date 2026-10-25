#include "feed_client.h"
#include "app_config.h"
#include <cstring>
#include <esp_http_client.h>
#include <strings.h>

namespace {
struct Response {
  std::string body;
  std::string retryAfter;
  bool overflow = false;
};
esp_err_t onHttpEvent(esp_http_client_event_t *event) {
  auto &response = *static_cast<Response *>(event->user_data);
  if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
    if (response.body.size() + static_cast<size_t>(event->data_len) > transit::MaxBodyBytes) {
      response.overflow = true;
      return ESP_FAIL;
    }
    response.body.append(static_cast<const char *>(event->data), event->data_len);
  }
  if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key && event->header_value &&
      strcasecmp(event->header_key, "Retry-After") == 0 && std::strlen(event->header_value) <= 64)
    response.retryAfter = event->header_value;
  return ESP_OK;
}
} // namespace
bool FeedClient::configured() const {
  return transit::validOrigin(localcfg::FeedOrigin) &&
         transit::validBearer(localcfg::BearerToken) && localcfg::RootCa[0];
}
transit::FeedResult FeedClient::fetch(const std::vector<std::string> &selectors) const {
  transit::FeedResult result;
  if (!configured()) {
    result.error = transit::RequestError::NoConfig;
    return result;
  }
  const auto url = std::string(localcfg::FeedOrigin) + transit::arrivalsPath(selectors);
  Response response;
  response.body.reserve(8192);
  esp_http_client_config_t config{};
  config.url = url.c_str();
  config.cert_pem = localcfg::RootCa;
  config.timeout_ms = 10000;
  config.disable_auto_redirect = true; // Never forward Bearer credentials to another endpoint.
  config.event_handler = onHttpEvent;
  config.user_data = &response;
  const auto client = esp_http_client_init(&config);
  if (!client) {
    result.error = transit::RequestError::Network;
    return result;
  }
  const auto bearer = std::string("Bearer ") + localcfg::BearerToken;
  esp_http_client_set_header(client, "Authorization", bearer.c_str());
  esp_http_client_set_header(client, "Accept", "application/json");
  esp_http_client_set_header(client, "X-Firmware-Version", appcfg::FirmwareVersion);
  const auto performed = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  if (response.overflow) {
    result.error = transit::RequestError::InvalidFeed;
    result.retryMs = 60000;
  } else if (performed != ESP_OK) {
    result.error = transit::RequestError::Network;
    result.retryMs = 60000;
  } else
    result = transit::readResponse(status, response.body, response.retryAfter);
  if (result.error == transit::RequestError::None)
    result.feed = transit::selectStops(result.feed, selectors);
  return result;
}
