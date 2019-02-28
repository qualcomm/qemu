#include "libqemu/ctors.h"

void libqemu_early_register_ctor(QemuCtorFct fct)
{
    fct();
}
