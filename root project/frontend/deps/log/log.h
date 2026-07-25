#ifndef _SERVECOSYS_FRONTEND_LOG_H_
#define _SERVECOSYS_FRONTEND_LOG_H_

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3,
} log_level_t;

void log_init(const char *app_name, log_level_t level);
void log_set_level(log_level_t level);
void log_set_output(const char *filepath);
void log_write(log_level_t level, const char *file, int line, const char *fmt, ...);

#define log_error(fmt, ...)  log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)   log_write(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)   log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...)  log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
