#ifndef CONFIG_H
#define CONFIG_H

// ============ تنظیمات سخت‌افزاری ============
#define BOARD_ID 3 // حیاط
#define BOARD_NAME "Yard Client"

// ============ تنظیمات رله ============
#define RELAY_COUNT 4
#define RELAY_ACTIVE_HIGH true

// ============ تنظیمات سنسورها ============
#define TEMP_ALERT_HIGH 40.0
#define TEMP_ALERT_LOW 0.0
#define HUMIDITY_ALERT_HIGH 90.0
#define HUMIDITY_ALERT_LOW 20.0
#define LASER_COUNT 3

// ============ تنظیمات سرور ============
#define SERVER_IP "192.168.1.100"
#define SERVER_PORT 80
#define SYNC_INTERVAL 5000 // 5 ثانیه

#endif
