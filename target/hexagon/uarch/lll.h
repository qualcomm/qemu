/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

////////////////////////////////////////////////////////////////////////////////
/// @file
/// This module defines a lightweight doubly linked list type.
///
////////////////////////////////////////////////////////////////////////////////

#include <stdbool.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
/// Types
////////////////////////////////////////////////////////////////////////////////

/// A node in a singly linked list
///
/// This opaque type should not be used directly.
struct sll_node {
	struct sll_node *next;
};
typedef struct sll_node sll_node_t;

/// A node in a doubly linked list
///
/// This opaque type should not be used directly.
struct dll_node {
	union {
		struct {
			struct dll_node *next;
			struct dll_node *prev;
		};
		struct {
			sll_node_t forward;
		};
	};
};
typedef struct dll_node dll_node_t;

////////////////////////////////////////////////////////////////////////////////
/// Public Functions - Doubly Linked
////////////////////////////////////////////////////////////////////////////////

/// Insert a node into a doubly linked list.
///
/// @param after The node after which the new node will be inserted.
/// @param toInsert The node to insert into the list
static inline void
dll_insert_after(dll_node_t *after, dll_node_t *toInsert);

/// Remove a node from a doubly linked list.
///
/// @param node The node which will be removed from its containing list.
static inline void
dll_remove(dll_node_t *node);

////////////////////////////////////////////////////////////////////////////////
/// Public Functions - Singly Linked
////////////////////////////////////////////////////////////////////////////////

/// Check whether singly linked list contains a node.
///
/// @param after The node after which the new node will be inserted.
/// @param toInsert The node to insert into the list
static inline bool
sll_contains(const sll_node_t * list, const sll_node_t * const item);
static inline bool
dll_contains(const dll_node_t * list, const dll_node_t * const item);

////////////////////////////////////////////////////////////////////////////////
/// Inline Function Definitions
////////////////////////////////////////////////////////////////////////////////

static inline void
dll_insert_after(dll_node_t *after, dll_node_t *toInsert)
{
	toInsert->next = after->next;
	toInsert->prev = after;

	toInsert->prev->next = toInsert;
	if (toInsert->next != NULL) {
		toInsert->next->prev = toInsert;
	}
}

static inline void
dll_remove(dll_node_t *node)
{
	if (node->next != NULL) {
		node->next->prev = node->prev;
	}

	if (node->prev != NULL) {
		node->prev->next = node->next;
	}

	node->prev = NULL;
	node->next = NULL;
}

static inline bool
sll_contains(const sll_node_t * list, const sll_node_t * const item)
{
  while (list != NULL) {
    if (list == item) {
      return true;
    }
    list = list->next;
  }

  return false;
}

static inline bool
dll_contains(const dll_node_t * list, const dll_node_t * const item)
{
  while (list != NULL) {
    if (list == item) {
      return true;
    }
    list = list->next;
  }

  return false;
}
