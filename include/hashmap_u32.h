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

#ifndef HASHMAP_U32_H
#define HASHMAP_U32_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct hashmap_entry
{
    uint32_t key;
    uint32_t value;
    bool occupied;
    bool deleted;
};

struct hashmap
{
    struct hashmap_entry *entries;
    size_t capacity;
    size_t size;
    size_t tombstone_count;
};

void hashmap_init(struct hashmap *map);

void hashmap_destroy(struct hashmap *map);

void hashmap_put(struct hashmap *map, uint32_t key, uint32_t value);

bool hashmap_get(const struct hashmap *map, uint32_t key, uint32_t *value);

bool hashmap_remove(struct hashmap *map, uint32_t key);

void hashmap_clear(struct hashmap *map);

size_t hashmap_size(const struct hashmap *map);

size_t hashmap_capacity(const struct hashmap *map);

#endif
