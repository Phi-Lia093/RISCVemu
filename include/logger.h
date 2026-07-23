/*
 * SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of RISCVemu.
 * RISCVemu is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * RISCVemu is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

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
void init_logger(char default_level, char *output_file);

// terminate logger
// must be called before program exit or log file won't be closed
// that would cause problem on ancient libc(s) :)
void terminate_logger(void);

// set lowest log show level
// level: lowest log show level
//        I think you will not pass -1 now
void set_level(char level);

// show different levels of message
// params: same as "printf"
// THE MAX LENGTH OF FORMAT STRING IS 1024
void debug(char *fmt, ...);
void info(char *fmt, ...);
void warn(char *fmt, ...);
void error(char *fmt, ...);
void fatal(char *fmt, ...);

#endif
