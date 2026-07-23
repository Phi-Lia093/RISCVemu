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

#include <assert.h>
#include <hashmap_u32.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 128
#define LOAD_FACTOR 0.75f
#define GROWTH_FACTOR 2

static uint32_t
hash_uint32(uint32_t key)
{
    uint32_t h = key;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static size_t
probe(const struct hashmap *map, uint32_t key, bool *found)
{
    uint32_t hash = hash_uint32(key);
    size_t index = (size_t)(hash % map->capacity);
    size_t first_deleted = (size_t)-1;

    for (size_t i = 0; i < map->capacity; ++i)
    {
        size_t idx = (index + i) % map->capacity;
        struct hashmap_entry *entry = &map->entries[idx];

        if (!entry->occupied)
        {
            if (first_deleted != (size_t)-1)
            {
                *found = false;
                return first_deleted;
            }
            *found = false;
            return idx;
        }

        if (entry->deleted)
        {
            if (first_deleted == (size_t)-1)
            {
                first_deleted = idx;
            }
            continue;
        }

        if (entry->key == key)
        {
            *found = true;
            return idx;
        }
    }

    *found = false;
    return 0;
}

static void
rehash(struct hashmap *map, size_t new_capacity)
{
    if (new_capacity < INITIAL_CAPACITY)
    {
        new_capacity = INITIAL_CAPACITY;
    }

    struct hashmap_entry *old_entries = map->entries;
    size_t old_capacity = map->capacity;

    map->entries = (struct hashmap_entry *)calloc(new_capacity,
                                                  sizeof(struct hashmap_entry));
    if (!map->entries)
    {
        map->entries = old_entries;
        return;
    }

    map->capacity = new_capacity;
    map->size = 0;
    map->tombstone_count = 0;

    for (size_t i = 0; i < old_capacity; ++i)
    {
        struct hashmap_entry *entry = &old_entries[i];
        if (entry->occupied && !entry->deleted)
        {
            bool found;
            size_t idx = probe(map, entry->key, &found);
            assert(!found);
            map->entries[idx] = *entry;
            map->entries[idx].deleted = false;
            map->entries[idx].occupied = true;
            map->size++;
        }
    }

    free(old_entries);
}

void
hashmap_init(struct hashmap *map)
{
    map->capacity = INITIAL_CAPACITY;
    map->size = 0;
    map->tombstone_count = 0;
    map->entries = (struct hashmap_entry *)calloc(map->capacity,
                                                  sizeof(struct hashmap_entry));
}

void
hashmap_destroy(struct hashmap *map)
{
    if (map->entries)
    {
        free(map->entries);
        map->entries = NULL;
    }
    map->capacity = 0;
    map->size = 0;
    map->tombstone_count = 0;
}

void
hashmap_put(struct hashmap *map, uint32_t key, uint32_t value)
{
    if ((float)(map->size + map->tombstone_count) / map->capacity
        >= LOAD_FACTOR)
    {
        size_t new_capacity = map->capacity * GROWTH_FACTOR;
        rehash(map, new_capacity);
    }

    bool found;
    size_t idx = probe(map, key, &found);

    if (found)
    {
        map->entries[idx].value = value;
    }
    else
    {
        if (map->entries[idx].deleted)
        {
            map->tombstone_count--;
        }
        map->entries[idx].key = key;
        map->entries[idx].value = value;
        map->entries[idx].occupied = true;
        map->entries[idx].deleted = false;
        map->size++;
    }
}

bool
hashmap_get(const struct hashmap *map, uint32_t key, uint32_t *value)
{
    bool found;
    size_t idx = probe(map, key, &found);
    if (found)
    {
        *value = map->entries[idx].value;
        return true;
    }
    return false;
}

bool
hashmap_remove(struct hashmap *map, uint32_t key)
{
    bool found;
    size_t idx = probe(map, key, &found);
    if (found)
    {
        map->entries[idx].deleted = true;
        map->size--;
        map->tombstone_count++;
        return true;
    }
    return false;
}

void
hashmap_clear(struct hashmap *map)
{
    memset(map->entries, 0, map->capacity * sizeof(struct hashmap_entry));
    map->size = 0;
    map->tombstone_count = 0;
}

size_t
hashmap_size(const struct hashmap *map)
{
    return map->size;
}

size_t
hashmap_capacity(const struct hashmap *map)
{
    return map->capacity;
}
