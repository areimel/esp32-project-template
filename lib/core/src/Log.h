#pragma once
#include <Arduino.h>

// Levels: 0 off, 1 error, 2 warn, 3 info (default), 4 debug. Override with -DLOG_LEVEL=n.
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#define APP_LOG(tag, fmt, ...) Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__)

#if LOG_LEVEL >= 1
#define LOG_E(fmt, ...) APP_LOG("E", fmt, ##__VA_ARGS__)
#else
#define LOG_E(fmt, ...) ((void)0)
#endif
#if LOG_LEVEL >= 2
#define LOG_W(fmt, ...) APP_LOG("W", fmt, ##__VA_ARGS__)
#else
#define LOG_W(fmt, ...) ((void)0)
#endif
#if LOG_LEVEL >= 3
#define LOG_I(fmt, ...) APP_LOG("I", fmt, ##__VA_ARGS__)
#else
#define LOG_I(fmt, ...) ((void)0)
#endif
#if LOG_LEVEL >= 4
#define LOG_D(fmt, ...) APP_LOG("D", fmt, ##__VA_ARGS__)
#else
#define LOG_D(fmt, ...) ((void)0)
#endif
