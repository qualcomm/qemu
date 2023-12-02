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

#ifndef _QUEUE_H
#define _QUEUE_H

#ifndef UARCH_FUNCTION_DECL
#define UARCH_FUNCTION_DECL(return_type, func_name, ...) \
  return_type func_name##_##debug(__VA_ARGS__);          \
  return_type func_name(__VA_ARGS__);
#endif

#include <stdbool.h>

#define MODULO_DEC_RHS(VAL, MAX) \
    ((VAL)  == 0 ? ((MAX)-1) : (VAL) - 1)
#define MODULO_DEC_LHS(VAL, MAX) \
     if ((--VAL) < 0) VAL = ((MAX) - 1)

#define MODULO_INC_RHS(VAL, MAX) \
    (((VAL) + 1) == (MAX) ? 0 : VAL + 1)
#define MODULO_INC_LHS(VAL, MAX) \
     if (++(VAL) == MAX) VAL = 0

struct ProcessorState;

struct queue_struct {
	int head;
	int tail;
	int depth;
	int occupied;
	int reserved;
    int circular;

	void *entries_ptr; /*for debug, or at least linking this control struct with some data.  ok to be null */
};

typedef struct queue_struct queue_t;

/* init */
UARCH_FUNCTION_DECL(void, queue_alloc, const struct ProcessorState *proc, queue_t *queue, void *entries_ptr, int depth);
UARCH_FUNCTION_DECL(void, circular_queue_alloc, const struct ProcessorState *proc, queue_t *queue, void *entries_ptr, int depth);

/* manipulate */
UARCH_FUNCTION_DECL(void, queue_reset, const struct ProcessorState *proc, queue_t *queue);
UARCH_FUNCTION_DECL(int, queue_undo_push, const struct ProcessorState *proc, int tnum, queue_t *);
UARCH_FUNCTION_DECL(int, queue_undo_pop_n, const struct ProcessorState *proc, int tnum, queue_t *, int n);
UARCH_FUNCTION_DECL(int, queue_pop, const struct ProcessorState *proc, int tnum, queue_t *);
UARCH_FUNCTION_DECL(int, queue_push, const struct ProcessorState *proc, int tnum, queue_t *);
UARCH_FUNCTION_DECL(int, queue_push_at_head, const struct ProcessorState *proc, int tnum, queue_t *);
UARCH_FUNCTION_DECL(void, queue_reserve_n, const struct ProcessorState *proc, int tnum, queue_t *, int n);
UARCH_FUNCTION_DECL(void, queue_unreserve_n, const struct ProcessorState *proc, int tnum, queue_t *, int n);

/* query */
UARCH_FUNCTION_DECL(int, queue_peek_head, const struct ProcessorState *proc, int tnum, const queue_t *);
UARCH_FUNCTION_DECL(int, queue_peek_tail, const struct ProcessorState *proc, int tnum, const queue_t *);
UARCH_FUNCTION_DECL(int, queue_peek_tail_plus_one, const struct ProcessorState *proc, int tnum, const queue_t *);
UARCH_FUNCTION_DECL(int, queue_peek_nth, const struct ProcessorState *proc, int tnum, const queue_t *, int);
UARCH_FUNCTION_DECL(int, queue_peek, const struct ProcessorState *proc, int tnum, const queue_t *);
UARCH_FUNCTION_DECL(int, queue_full, const struct ProcessorState *proc, int tnum, const queue_t *);
UARCH_FUNCTION_DECL(int, queue_depth, const struct ProcessorState *proc, int tnum, const queue_t *);
UARCH_FUNCTION_DECL(int, queue_empty, const struct ProcessorState *proc, int tnum,  const queue_t *);
UARCH_FUNCTION_DECL(int, queue_occupied, const struct ProcessorState *proc, int tnum,  const queue_t *);
UARCH_FUNCTION_DECL(int, queue_reserved, const struct ProcessorState *proc, int tnum, const queue_t *);
UARCH_FUNCTION_DECL(int, queue_reservable, const struct ProcessorState *proc, int tnum, const queue_t *);
UARCH_FUNCTION_DECL(void *, queue_get_entries, const struct ProcessorState *proc, int tnum, const queue_t *);

////////////////////////////////////////////////////////////////////////////////
/// Iterator
////////////////////////////////////////////////////////////////////////////////

typedef struct {
	const queue_t * const queue;
  int offset;
} queue_iterator_t;

static inline queue_iterator_t
queue_begin_iterator(const queue_t *queue)
{
	const queue_iterator_t iter = {
		.queue = queue,
		.offset = 0
	};

	return iter;
}

static inline queue_iterator_t
queue_end_iterator(const queue_t *queue)
{
	const queue_iterator_t iter = {
		.queue = queue,
		.offset = queue->occupied
	};

	return iter;
}

static inline bool
queue_iterator_gt(
	const queue_iterator_t left,
	const queue_iterator_t right
) {
	return left.offset > right.offset;
}

static inline bool
queue_iterator_eq(
	const queue_iterator_t left,
	const queue_iterator_t right
) {
	return left.offset == right.offset;
}



static inline void
queue_iterator_inc(queue_iterator_t * const iter)
{
	++iter->offset;
}

static inline void
queue_iterator_dec(queue_iterator_t * const iter)
{
	--iter->offset;
}

static inline int
queue_iterator_get(queue_iterator_t iter)
{
	return (iter.queue->head + iter.offset) % iter.queue->depth;
}

#endif							//QUEUE_H
