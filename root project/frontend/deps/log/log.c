#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

static log_level_t current_level = LOG_LEVEL_INFO;
static const char *app_name = "uid";
static FILE *log_file = NULL;
static int use_file = 0;

static const char *level_str(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default:              return "?";
    }
}

void log_init(const char *app, log_level_t level) {
    app_name = app ? app : "uid";
    current_level = level;
}

void log_set_level(log_level_t level) {
    current_level = level;
}

void log_set_output(const char *filepath) {
    if (log_file) fclose(log_file);
    log_file = fopen(filepath, "a");
    use_file = (log_file != NULL);
}

void log_write(log_level_t level, const char *file, int line, const char *fmt, ...) {
    if (level > current_level) return;

    struct timeval tv;
    struct tm tm;
    char timebuf[32];
    char prefix[256];

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm);

    snprintf(prefix, sizeof(prefix), "[%s.%03ld][%s][%s] %s:%d ",
             timebuf, (long)tv.tv_usec / 1000,
             app_name, level_str(level), file, line);

    va_list args;
    va_start(args, fmt);

    if (use_file && log_file) {
        fprintf(log_file, "%s", prefix);
        vfprintf(log_file, fmt, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    } else {
        fprintf(stdout, "%s", prefix);
        vfprintf(stdout, fmt, args);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    va_end(args);
}
