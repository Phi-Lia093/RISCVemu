#ifndef LOGGER_H
#define LOGGER_H

// level enums
#define DEBUG 0
#define INFO 1
#define WARN 2
#define ERROR 3
#define DEFAULT_LEVEL INFO

// init logger
// must be called before any logger functions or the program state is unknown
// default_level: lowest log show level
//                        pass -1 for default level
// output_file: output log file name
//                          pass NULL for no log files
//                          default operation for exist file is truncate
//                          create file if not exist
void
init_logger(char default_level, char *output_file);

// terminate logger
// must be called before program exit or log file won't be closed
// that would cause problem on ancient libc(s) :)
void
terminate_logger(void);

// set lowest log show level
// level: lowest log show level
//        I think you will not pass -1 now
void
set_level(char level);

// show different levels of message
// params: same as "printf"
// THE MAX LENGTH OF FORMAT STRING IS 1024
void
debug(char *fmt, ...);
void
info(char *fmt, ...);
void
warn(char *fmt, ...);
void
error(char *fmt, ...);

#endif