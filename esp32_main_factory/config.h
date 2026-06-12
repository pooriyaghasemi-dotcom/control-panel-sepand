#ifndef CONFIG_H
#define CONFIG_H

// ============ تنظیمات سخت‌افزاری ============
#define BOARD_ID 1 // 1: سالن تولید، 2: دفتر، 3: حیاط
#define BOARD_NAME "Factory Main Server"

// ============ تنظیمات رله ============
#define RELAY_COUNT 16
#define RELAY_ACTIVE_HIGH true // true: HIGH برای روشن، false: LOW برای روشن

// ============ تنظیمات سنسورها ============
#define TEMP_ALERT_HIGH 40.0 // حد بالای دما برای هشدار
#define TEMP_ALERT_LOW 0.0   // حد پایین دما برای هشدار
#define HUMIDITY_ALERT_HIGH 90.0 // حد بالای رطوبت
#define HUMIDITY_ALERT_LOW 20.0   // حد پایین رطوبت

// ============ تنظیمات EEPROM ============
#define EEPROM_SIZE 4096
#define SAVE_INTERVAL 5000 // هر 5 ثانیه

// ============ تنظیمات Web Server ============
#define WEB_SERVER_PORT 80
#define API_TIMEOUT 5000 // 5 ثانیه

#endif
