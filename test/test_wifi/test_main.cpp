#include "wifi_credentials.h"
#include "wifi_setup_diagnostics.h"
#include "wifi_setup_screen.h"
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
  TEST_ASSERT_EQUAL_STRING("metro-display", SetupSsid);
  TEST_ASSERT_EQUAL_STRING("bcnmetro", SetupPassword);
  TEST_ASSERT_TRUE(validCredentials(SetupSsid, SetupPassword));
  for (const std::string &line :
       {std::string(SetupSsid), "PW " + std::string(SetupPassword), std::string("LOWERCASE"),
        std::string("OPEN HTTP"), std::string("192.168.4.1")}) {
    TEST_ASSERT_TRUE(typography::c2Supports(line));
    TEST_ASSERT_LESS_OR_EQUAL(64, typography::c2Width(line));
  }
}
void setup_request_origin_validation() {
  TEST_ASSERT_TRUE(setupHost("192.168.4.1"));
  TEST_ASSERT_TRUE(setupHost("192.168.4.1:80"));
  TEST_ASSERT_TRUE(
      setupOrigin("")); // Some local clients omit Origin; form token is still required.
  TEST_ASSERT_TRUE(setupOrigin("http://192.168.4.1"));
  TEST_ASSERT_TRUE(setupOrigin("http://192.168.4.1:80"));
  for (const std::string &value : {std::string("192.168.4.1.attacker.example"),
                                   std::string("192.168.4.1:81"), std::string("localhost")})
    TEST_ASSERT_FALSE(setupHost(value));
  TEST_ASSERT_FALSE(setupOrigin("http://192.168.4.1.attacker.example"));
  TEST_ASSERT_FALSE(setupOrigin("null"));
  TEST_ASSERT_FALSE(setupOrigin("https://192.168.4.1"));
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
int main() {
  UNITY_BEGIN();
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
