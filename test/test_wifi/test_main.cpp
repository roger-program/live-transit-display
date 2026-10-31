#include "setup_button.h"
#include "wifi_credentials.h"
#include "wifi_networks.h"
#include "wifi_setup_diagnostics.h"
#include "wifi_setup_screen.h"
#include "wifi_setup_web.h"
#include <unity.h>
using namespace provisioning;
void setUp() {}
void tearDown() {}
void credential_boundaries() {
  TEST_ASSERT_TRUE(validCredentials("home", ""));
  TEST_ASSERT_TRUE(validCredentials(std::string(32, 'x'), std::string(63, 'p')));
  TEST_ASSERT_FALSE(validCredentials("", "password"));
  TEST_ASSERT_FALSE(validCredentials(std::string(33, 'x'), "password"));
  TEST_ASSERT_FALSE(validCredentials("home", "short"));
  TEST_ASSERT_FALSE(validCredentials("home", std::string(65, 'a')));
}
void raw_psk_validation() {
  TEST_ASSERT_TRUE(validCredentials("home", std::string(64, 'a')));
  TEST_ASSERT_FALSE(validCredentials("home", std::string(64, 'g')));
}
void reject_embedded_null() {
  TEST_ASSERT_FALSE(validCredentials(std::string("ab\0cd", 5), "password"));
  TEST_ASSERT_FALSE(validCredentials("home", std::string("password\0x", 10)));
}
void stored_record_validation() {
  Credentials record;
  TEST_ASSERT_FALSE(validRecord(record));
  std::strcpy(record.ssid, "home");
  std::strcpy(record.password, "password");
  TEST_ASSERT_TRUE(validRecord(record));
  record.version = 2;
  TEST_ASSERT_FALSE(validRecord(record));
  record.version = 1;
  record.ssid[32] = 'x';
  TEST_ASSERT_FALSE(validRecord(record));
  record.ssid[32] = 0;
  record.password[64] = 'x';
  TEST_ASSERT_FALSE(validRecord(record));
}
void setup_credentials_fit_static_panel() {
  TEST_ASSERT_EQUAL_STRING("METRO-DISPLAY", SetupSsid);
  TEST_ASSERT_EQUAL_STRING("BCNMETRO", SetupPassword);
  TEST_ASSERT_TRUE(validCredentials(SetupSsid, SetupPassword));
  for (const std::string &line :
       {std::string("1. WIFI"), std::string(SetupSsid), std::string(SetupPassword),
        std::string("2. ABRE WEB"), std::string("METRO.CONF")}) {
    TEST_ASSERT_TRUE(typography::c2Supports(line));
    TEST_ASSERT_LESS_OR_EQUAL(64, typography::c2Width(line));
  }
}
void scan_names_are_exact_unique_and_strongest_first() {
  std::vector<WifiNetwork> networks;
  addWifiNetwork(networks, "Mesh", -80);
  addWifiNetwork(networks, "Mesh", -35);
  addWifiNetwork(networks, "Mesh", -70);
  addWifiNetwork(networks, "mesh", -50);
  addWifiNetwork(networks, "Mesh ", -60);
  addWifiNetwork(networks, "Mesh-Guest", -90);
  addWifiNetwork(networks, "", -10);
  sortWifiNetworks(networks);
  TEST_ASSERT_EQUAL(4, networks.size());
  TEST_ASSERT_EQUAL_STRING("Mesh", networks[0].ssid.c_str());
  TEST_ASSERT_EQUAL(-35, networks[0].rssi);
  TEST_ASSERT_EQUAL_STRING("mesh", networks[1].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("Mesh ", networks[2].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("Mesh-Guest", networks[3].ssid.c_str());
  std::vector<WifiNetwork> empty;
  sortWifiNetworks(empty);
  TEST_ASSERT_TRUE(empty.empty());
}
void setup_credentials_match_uppercase_screen() {
  TEST_ASSERT_TRUE(typography::c2Supports(SetupSsid));
  TEST_ASSERT_TRUE(typography::c2Supports(SetupPassword));
  TEST_ASSERT_EQUAL_STRING(SetupSsid, typography::c2Text(SetupSsid).c_str());
  TEST_ASSERT_EQUAL_STRING(SetupPassword, typography::c2Text(SetupPassword).c_str());
  TEST_ASSERT_LESS_OR_EQUAL(64, typography::c2Width(SetupSsid));
  TEST_ASSERT_LESS_OR_EQUAL(64, typography::c2Width(SetupPassword));
  transit::Framebuffer frame(64, 32), expected(64, 32);
  renderSetupScreen(frame, SetupSsid, SetupPassword);
  typography::drawC2(expected, "1. WIFI", 0, 0, 0xffffff, 0, 64);
  typography::drawC2(expected, SetupSsid, 0, 6, 0xffffff, 0, 64);
  typography::drawC2(expected, SetupPassword, 0, 12, 0xffffff, 0, 64);
  typography::drawC2(expected, "2. ABRE WEB", 0, 21, 0xffffff, 0, 64);
  typography::drawC2(expected, "METRO.CONF", 0, 27, 0xffffff, 0, 64);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      TEST_ASSERT_EQUAL(expected.pixel(x, y), frame.pixel(x, y));
}
void setup_request_origin_validation() {
  TEST_ASSERT_EQUAL_STRING("metro.conf", SetupHostname);
  for (const std::string &host :
       {"192.168.4.1", "192.168.4.1:80", "metro.conf", "METRO.CONF", "MeTrO.CoNf:80"}) {
    TEST_ASSERT_TRUE(setupHost(host));
    TEST_ASSERT_TRUE(setupOrigin("http://" + host));
  }
  TEST_ASSERT_TRUE(setupOrigin(""));
  for (const std::string &host : {"192.168.4.1.attacker.example", "192.168.4.1:81", "localhost",
                                  "metro.conf.attacker.example", "metro.conf:443", "www.metro.conf",
                                  "metro.conf/", "user@metro.conf"}) {
    TEST_ASSERT_FALSE(setupHost(host));
    TEST_ASSERT_FALSE(setupOrigin("http://" + host));
  }
  TEST_ASSERT_FALSE(setupOrigin("null"));
  TEST_ASSERT_FALSE(setupOrigin("https://192.168.4.1"));
  TEST_ASSERT_FALSE(setupOrigin("https://metro.conf"));
}
void ssid_diagnostics_are_bounded_single_line() {
  TEST_ASSERT_EQUAL_STRING("Home Wi-Fi", diagnosticSsid("Home Wi-Fi").c_str());
  const std::string input = std::string("x\r\n\t\x1b\"\\", 7) + char(0xff);
  const auto safe = diagnosticSsid(input);
  TEST_ASSERT_EQUAL_STRING("x\\x0D\\x0A\\x09\\x1B\\x22\\x5C\\xFF", safe.c_str());
  TEST_ASSERT_EQUAL(32, diagnosticSsid(std::string(100, 'x')).size());
}
void connection_failures_distinguish_network_auth_and_timeout() {
  TEST_ASSERT_NOT_NULL(std::strstr(connectionFailure(201), "Network not found"));
  TEST_ASSERT_NOT_NULL(std::strstr(connectionFailure(202), "Authentication"));
  TEST_ASSERT_NOT_NULL(std::strstr(connectionFailure(204), "handshake"));
  TEST_ASSERT_NOT_NULL(std::strstr(connectionFailure(0), "DHCP"));
  TEST_ASSERT_NOT_NULL(std::strstr(connectionFailure(65535), "retry"));
}
void boot_hold_debounces_and_triggers_once() {
  SetupButton button;
  TEST_ASSERT_FALSE(button.update(false, 0));
  TEST_ASSERT_FALSE(button.update(true, 100));
  TEST_ASSERT_FALSE(button.update(false, 150));
  TEST_ASSERT_FALSE(button.update(true, 180));
  TEST_ASSERT_FALSE(button.update(true, 280));
  TEST_ASSERT_TRUE(button.pressed());
  TEST_ASSERT_FALSE(button.update(false, 1000)); // Short release noise does not reset the hold.
  TEST_ASSERT_FALSE(button.update(true, 1050));
  TEST_ASSERT_FALSE(button.update(true, 3279));
  TEST_ASSERT_TRUE(button.update(true, 3280));
  TEST_ASSERT_FALSE(button.update(true, 9000));
  TEST_ASSERT_FALSE(button.update(false, 9050));
  TEST_ASSERT_FALSE(button.update(false, 9150));
  TEST_ASSERT_FALSE(button.pressed());
  TEST_ASSERT_FALSE(button.update(true, 9200));
  TEST_ASSERT_FALSE(button.update(true, 9300));
  TEST_ASSERT_TRUE(button.update(true, 12300));
}
void boot_hold_at_first_sample_and_short_press() {
  SetupButton held;
  TEST_ASSERT_FALSE(held.update(true, 0));
  TEST_ASSERT_FALSE(held.update(true, 100));
  TEST_ASSERT_TRUE(held.update(true, 3100));
  SetupButton shortPress;
  shortPress.update(false, 0);
  shortPress.update(true, 100);
  shortPress.update(true, 200);
  shortPress.update(false, 1000);
  shortPress.update(false, 1100);
  TEST_ASSERT_FALSE(shortPress.update(false, 5000));
}
void success_page_has_no_form_or_troubleshooting() {
  const std::string page = WifiSuccessContent;
  for (const char *text : {"Connected!", "Your display is ready.", "You can close this page."})
    TEST_ASSERT_TRUE(page.find(text) != std::string::npos);
  for (const char *element : {"<form", "<input", "<select", "<button", "http-equiv", "password",
                              "2.4 GHz", "reconnect", "Retry"})
    TEST_ASSERT_TRUE(page.find(element) == std::string::npos);
}
void connecting_and_failure_content_excludes_configuration() {
  const std::string connecting = WifiConnectingContent;
  TEST_ASSERT_TRUE(connecting.find("Connecting…") != std::string::npos);
  TEST_ASSERT_TRUE(connecting.find("Setting up your display.") != std::string::npos);
  for (const char *element :
       {"<form", "<input", "<select", "<button", "seconds", "timeout", "DHCP"})
    TEST_ASSERT_TRUE(connecting.find(element) == std::string::npos);
  const std::string failure = WifiFailureContent;
  TEST_ASSERT_TRUE(failure.find("Couldn’t connect") != std::string::npos);
  TEST_ASSERT_TRUE(failure.find("Check your Wi-Fi password and try again.") != std::string::npos);
  TEST_ASSERT_TRUE(failure.find("Try again") != std::string::npos);
  for (const char *element : {"<form", "<input", "<select", "seconds", "DHCP"})
    TEST_ASSERT_TRUE(failure.find(element) == std::string::npos);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(scan_names_are_exact_unique_and_strongest_first);
  RUN_TEST(setup_credentials_match_uppercase_screen);
  RUN_TEST(connecting_and_failure_content_excludes_configuration);
  RUN_TEST(boot_hold_debounces_and_triggers_once);
  RUN_TEST(boot_hold_at_first_sample_and_short_press);
  RUN_TEST(success_page_has_no_form_or_troubleshooting);
  RUN_TEST(setup_request_origin_validation);
  RUN_TEST(ssid_diagnostics_are_bounded_single_line);
  RUN_TEST(connection_failures_distinguish_network_auth_and_timeout);
  RUN_TEST(setup_credentials_fit_static_panel);
  RUN_TEST(credential_boundaries);
  RUN_TEST(raw_psk_validation);
  RUN_TEST(reject_embedded_null);
  RUN_TEST(stored_record_validation);
  return UNITY_END();
}
