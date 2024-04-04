/*
 *  Copyright(c) 2022-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

/*
 * This test runs a loop in a thread until the main thread sets a global
 * variable. This checks whether QEMU is properly switching threads, in order
 * to allow the main thread to set the variable.
 */

#include <hexagon_standalone.h>
#include <stdio.h>


volatile int running = 1;

#define STACK_SZ 1024
long long stack[STACK_SZ];
void thread_fn(void *id)
{
    while (running)
        ;
}

int main()
{
    thread_create(thread_fn, &stack[STACK_SZ - 1], 1, NULL);
    running = 0;
    thread_join(1 << 1);
    printf("PASS\n");
    return 0;
}
