#pragma once

#define CLEANER_NAME(func) __##func##_cleaner
#define CLEANER_NONNULL(type, func) static inline void CLEANER_NAME(func)(const type *res) { if (res) func(*res); }
#define CLEANER_NULLABLE(type, func) static inline void CLEANER_NAME(func)(type *res) { if (res&&*res) func(*res); }
#define CLEANABLE(func) __attribute__((cleanup(CLEANER_NAME(func)))) __attribute__((unused))