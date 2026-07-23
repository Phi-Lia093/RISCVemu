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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <logger.h>

void *
map_file(const char *path, size_t *length, int pm)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        warn("could not open file %s", path);
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        warn("fstat failed");
        close(fd);
        return NULL;
    }

    if (st.st_size == 0)
    {
        warn("file %s is empty", path);
        close(fd);
        return NULL;
    }

    void *mapped = mmap(NULL, st.st_size, pm, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED)
    {
        warn("mmap failed");
        close(fd);
        return NULL;
    }
    if (length != NULL)
    {
        length[0] = st.st_size;
    }
    close(fd);
    return mapped;
}

void
unmap_file(void *addr, size_t length)
{
    if (munmap(addr, length) != 0)
    {
        warn("failed to unmap @ addr %p", addr);
    }
}
