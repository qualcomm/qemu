#include "hvx-multi.h"
#include <stdio.h>
#include <hexagon_standalone.h>

enum {
  resource_unmapped = -1,
  resource_max      = 4,
  vector_unit_mask  = 0x38000000,
  vector_unit_0     = 0x20000000,  /* XA:100b */
  vector_unit_1     = 0x28000000,  /* XA:101b */
  vector_unit_2     = 0x30000000,  /* XA:110b */
  vector_unit_3     = 0x38000000,  /* XA:111b */
};

static inline uint32_t ssr()
{
    uint32_t reg;
    asm volatile ("%0=ssr;"
                  : "=r"(reg));
    return reg;
}

int main()
{
    unsigned int va[32] = {0};
    unsigned int vb[32] = {0};

    enable_vector_unit(vector_unit_0);
    setv0();
    store_vector_0(va);
    enable_vector_unit(vector_unit_1);
    store_vector_0(vb);

/*
 * At this point vector unit 0 v0 is all 1's
 * vector unit 1 v0 is all 0's
 * This test verifies that a new unit was selected and is
 * independend of the thread.
 */
    if ((vb[0] == 0) && (va[1] == 0xffffffff)) {
        printf("PASS: vb[0] = 0x%x\n", vb[0]);
        return 0;
    }
    printf("FAIL: va[0] = 0x%x\n", va[0]);
    printf("      vb[0] = 0x%x\n", vb[0]);
    return 1;

}
