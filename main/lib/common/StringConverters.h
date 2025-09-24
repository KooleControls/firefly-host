#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

// ----- FLOAT -----
static void stringToFloat(void* value, const char* str)
{
    float* floatValue = static_cast<float*>(value);
    *floatValue = strtof(str, nullptr);
}

static void floatToString(char* buffer, size_t size, void* value)
{
    float* floatValue = static_cast<float*>(value);
    snprintf(buffer, size, "%.2f", *floatValue);
}

// ----- BOOL -----
static void stringToBool(void* value, const char* str)
{
    bool* boolValue = static_cast<bool*>(value);
    *boolValue = (strcmp(str, "1") == 0 || strcasecmp(str, "true") == 0);
}

static void boolToString(char* buffer, size_t size, void* value)
{
    bool* boolValue = static_cast<bool*>(value);
    snprintf(buffer, size, "%s", *boolValue ? "true" : "false");
}

// ----- INT32 -----
static void stringToInt32(void* value, const char* str)
{
    int32_t* intValue = static_cast<int32_t*>(value);
    *intValue = static_cast<int32_t>(atoi(str));
}

static void int32ToString(char* buffer, size_t size, void* value)
{
    int32_t* intValue = static_cast<int32_t*>(value);
    snprintf(buffer, size, "%ld", *intValue);
}

// ----- ENUM -----
template<typename TEnum>
static void stringToEnum(void* value, const char* str)
{
    int intValue = atoi(str);
    *static_cast<TEnum*>(value) = static_cast<TEnum>(intValue);
}

template<typename TEnum>
static void enumToString(char* buffer, size_t size, void* value)
{
    int intValue = static_cast<int>(*static_cast<TEnum*>(value));
    snprintf(buffer, size, "%d", intValue);
}

template<typename TEnum, const char* (*ToStringFn)(TEnum)>
inline void enumToString(char* buffer, size_t size, void* value)
{
    TEnum e = *static_cast<TEnum*>(value);
    snprintf(buffer, size, "%s", ToStringFn(e));
}
