#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <logger.h>

char level;
FILE *file_output;

static void
generic_logger(char *level, char *fmt, va_list args)
{
    time_t raw_time;
    struct tm *time_info;
    char *time_buffer = (char *)malloc(sizeof(char) * 256);
    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(time_buffer, sizeof(char) * 256, "%H:%M:%S", time_info);
    char *buffer = (char *)malloc(sizeof(char) * 1024);
    vsnprintf(buffer, 1024 * sizeof(char), fmt, args);
    char *output_buffer = (char *)malloc(sizeof(char) * 2048);
    snprintf(output_buffer, 2048 * sizeof(char), "%s [%s] %s\n", time_buffer,
             level, buffer);
    printf("%s", output_buffer);
    if (file_output != NULL)
    {
        fputs(output_buffer, file_output);
    }
    free(time_buffer);
    free(buffer);
    free(output_buffer);
}

void
init_logger(char default_level, char *output_file)
{
    if (default_level == -1)
    {
        level = INFO;
    }
    else
    {
        level = default_level;
    }
    if (output_file != NULL)
    {
        file_output = fopen(output_file, "wa");
    }
}

void
terminate_logger(void)
{
    if (file_output != NULL)
    {
        fclose(file_output);
    }
}

void
set_level(char level_p)
{
    if (level >= DEBUG && level <= ERROR)
    {
        level = level_p;
    }
}

void
debug(char *fmt, ...)
{
    if (level > DEBUG) return;
    va_list args;
    va_start(args, fmt);
    generic_logger("DEBUG", fmt, args);
    va_end(args);
}

void
info(char *fmt, ...)
{
    if (level > INFO) return;
    va_list args;
    va_start(args, fmt);
    generic_logger("INFO", fmt, args);
    va_end(args);
}

void
warn(char *fmt, ...)
{
    if (level > WARN) return;
    va_list args;
    va_start(args, fmt);
    generic_logger("WARN", fmt, args);
    va_end(args);
}

void
error(char *fmt, ...)
{
    if (level > ERROR) return;
    va_list args;
    va_start(args, fmt);
    generic_logger("ERROR", fmt, args);
    va_end(args);
}

void
fatal(char *fmt, ...)
{
    if (level > ERROR) return;
    va_list args;
    va_start(args, fmt);
    generic_logger("!!!FATAL!!!", fmt, args);
    va_end(args);
    terminate_logger();
    exit(-1);
}
