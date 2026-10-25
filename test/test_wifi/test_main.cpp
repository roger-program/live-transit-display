#include "wifi_credentials.h"
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
int main() {
  UNITY_BEGIN();
  RUN_TEST(setup_credentials_fit_static_panel);
  RUN_TEST(credential_boundaries);
  RUN_TEST(raw_psk_validation);
  RUN_TEST(reject_embedded_null);
  RUN_TEST(stored_record_validation);
  return UNITY_END();
}
