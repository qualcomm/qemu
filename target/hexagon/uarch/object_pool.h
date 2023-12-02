/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

////////////////////////////////////////////////////////////////////////////////
/// @file
/// This module defines an object pooling allocator.
/// The semantics of the allocator should match those of [cm]alloc/free.
///
////////////////////////////////////////////////////////////////////////////////

#include "lll.h"

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
/// Types
////////////////////////////////////////////////////////////////////////////////

typedef struct {
	dll_node_t freeList;
	size_t elemSize;
} object_pool_t;

////////////////////////////////////////////////////////////////////////////////
/// Public Functions
////////////////////////////////////////////////////////////////////////////////

static inline void
object_pool_init(
	object_pool_t *pool,
	char *storage,
	size_t elemSize,
	size_t poolSize
) {
	assert(elemSize >= sizeof(dll_node_t) && "Pooled elements too small.");

	const char *const poolEnd = storage + elemSize*poolSize;
	dll_node_t *last = &pool->freeList;
	last->prev = NULL;

	while (storage < poolEnd) {
		dll_node_t *const curr = (dll_node_t*) storage;
		dll_insert_after(last, curr);

		last = curr;
		storage += elemSize;
	}

	last->next = NULL;
	pool->elemSize = elemSize;
}

static inline void *
object_pool_malloc(object_pool_t* pool, size_t size)
{
	assert(size == pool->elemSize);

	dll_node_t *object = pool->freeList.next;

	if (object != NULL) {
		dll_remove(object);
	}

	return object;
}

static inline void
object_pool_free(object_pool_t* pool, void* ptr)
{
	dll_insert_after(&pool->freeList, (dll_node_t *)ptr);
}

/// TODO: Do we want to take a count parameter?
static inline void *
object_pool_calloc(object_pool_t* pool, size_t size)
{
	assert(size == pool->elemSize);

	dll_node_t *object = object_pool_malloc(pool, size);

	if (object != NULL) {
		memset(object, 0, pool->elemSize);
	}

	return object;
}

