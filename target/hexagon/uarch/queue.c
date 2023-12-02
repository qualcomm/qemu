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

#include "qemu/osdep.h"
#include "queue.h"

#ifndef UARCH_FUNCTION
#define UARCH_FUNCTION(func_name) func_name
#endif


#define QUEUE_CALC_PTR(QUEUE_PTR, IDX) \
	(void *)((((char *)((QUEUE_PTR)->entries_ptr)) + (IDX)*((QUEUE_PTR)->entry_size)))

#define QUEUE_REVERSE_CALC_PTR(QUEUE_PTR, ENTRY_PTR) \
	(((char *)(ENTRY_PTR) - (char *)((QUEUE_PTR)->entries_ptr))/((QUEUE_PTR)->entry_size))


typedef struct ProcessorState processor_t;

/*------*/
/* Init */
/*------*/
void
UARCH_FUNCTION(_queue_alloc) (const processor_t *proc, queue_t *queue,
		void *entries_ptr, int depth, int is_circular);
void
UARCH_FUNCTION(_queue_alloc) (const processor_t *proc, queue_t *queue,
		void *entries_ptr, int depth, int is_circular)
{
	queue->entries_ptr = entries_ptr;
	queue->depth = depth;
	queue->occupied = 0;
	queue->head = 0;
	queue->tail = 0;
    queue->circular = is_circular;
}

void
UARCH_FUNCTION(queue_alloc) (const processor_t *proc, queue_t *queue,
		void *entries_ptr, int depth)
{
    UARCH_FUNCTION(_queue_alloc)(proc, queue, entries_ptr, depth, 0);
}

void
UARCH_FUNCTION(circular_queue_alloc) (const processor_t *proc, queue_t *queue,
		void *entries_ptr, int depth)
{
    UARCH_FUNCTION(_queue_alloc)(proc, queue, entries_ptr, depth, 1);
}

void
UARCH_FUNCTION(queue_reset) (const processor_t *proc, queue_t *queue)
{
	queue->occupied = 0;
	queue->head = 0;
	queue->tail = 0;
}

/*------------*/
/* Manipulate */
/*------------*/
int
UARCH_FUNCTION(queue_undo_push) (const processor_t *proc, int tnum, queue_t *queue)
{
  assert(queue->occupied && "nothing pushed to undo");

  MODULO_DEC_LHS(queue->tail, queue->depth);
  queue->occupied--;

  return queue->tail;
}

int
UARCH_FUNCTION(queue_undo_pop_n) (const processor_t * proc, int tnum, queue_t* queue, int n){
    queue->head -= n;
    if (queue->head < 0)
        queue->head += queue->depth;
    queue->occupied += n;
    return queue->head;
}

int
UARCH_FUNCTION(queue_pop) (const processor_t *proc, int tnum, queue_t *queue)
{
	int popped = queue->head;
	assert((queue->circular || queue->occupied) && "nothing to pop");

	MODULO_INC_LHS(queue->head, queue->depth);
    if (queue->occupied) {
        queue->occupied--;
    }

	return popped;
}

int
UARCH_FUNCTION(queue_push) (const processor_t *proc, int tnum, queue_t *queue)
{
	int pushed = queue->tail;

    assert((queue->circular || (queue->occupied < queue->depth)) && "Vqueue over occupied");
	MODULO_INC_LHS(queue->tail, queue->depth);
    if (queue->occupied < queue->depth) {
        queue->occupied++;
    }

	return pushed;
}

int
UARCH_FUNCTION(queue_push_at_head) (const processor_t *proc, int tnum, queue_t *queue)
{
	assert((queue->circular || (queue->occupied < queue->depth)) && "Vqueue over occupied");
	MODULO_DEC_LHS(queue->head, queue->depth);
    int pushed = queue->head;

    if (queue->occupied < queue->depth) {
        queue->occupied++;
    }

	return pushed;
}

void
UARCH_FUNCTION(queue_reserve_n) (const processor_t *proc, int tnum, queue_t *queue, int res_num)
{

    queue->reserved += res_num;

    assert((queue->reserved + queue->occupied) <= queue->depth
           && "queue overcommited");
}

void
UARCH_FUNCTION(queue_unreserve_n) (const processor_t *proc, int tnum, queue_t *queue, int res_num)
{
    queue->reserved -= res_num;

    assert(queue->reserved >= 0
           && "queue - unreserve request > reserved available");
}

/*-------*/
/* Query */
/*-------*/

int
UARCH_FUNCTION(queue_peek_tail) (const processor_t *proc, int tnum, const queue_t *queue)
{
	int tail = queue->tail;

	if (queue->occupied == 0) {
		return -1;
	}
	MODULO_DEC_LHS(tail, queue->depth);
	return tail;
}

int
UARCH_FUNCTION(queue_peek_tail_plus_one) (const processor_t *proc, int tnum, const queue_t *queue)
{
	if (UARCH_FUNCTION(queue_full)(proc, tnum, queue)) {
		return -1;
	}
	return queue->tail;
}

int
UARCH_FUNCTION(queue_peek_head) (const processor_t *proc, int tnum, const queue_t * queue)
{
	if (queue->occupied == 0) {
		return -1;
	}
	return queue->head;
}

int
UARCH_FUNCTION(queue_peek) (const processor_t *proc, int tnum, const queue_t * queue)
{
	return UARCH_FUNCTION(queue_peek_head) (proc, tnum, queue);
}

int
UARCH_FUNCTION(queue_peek_nth) (const processor_t *proc, int tnum, const queue_t * queue, int n)
{
	if (queue->occupied == 0) {
		return -1;
	}
	return (queue->head + n) % queue->depth;
}

int
UARCH_FUNCTION(queue_full) (const processor_t *proc, int tnum, const queue_t *queue)
{
	return (queue->occupied == queue->depth);
}

int
UARCH_FUNCTION(queue_empty) (const processor_t *proc, int tnum, const queue_t *queue)
{
	return (queue->occupied == 0);
}


int
UARCH_FUNCTION(queue_occupied) (const processor_t *proc, int tnum, const queue_t *queue)
{
	return queue->occupied;
}

int
UARCH_FUNCTION(queue_reservable) (const processor_t *proc, int tnum, const queue_t *queue)
{
    return queue->depth - queue->occupied - queue->reserved;
}


int
UARCH_FUNCTION(queue_reserved) (const processor_t *proc, int tnum, const queue_t *queue)
{
    return queue->reserved;
}

int
UARCH_FUNCTION(queue_depth) (const processor_t *proc, int tnum, const queue_t *queue)
{
    return queue->depth;
}


void *
UARCH_FUNCTION(queue_get_entries) (const processor_t *proc, int tnum, const queue_t *queue)
{
	return queue->entries_ptr;
}

