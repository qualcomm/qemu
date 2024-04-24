import hex_common

def handle_coproc(tag, regs, f):
    if "A_COPROC" not in hex_common.attribdict[tag]:
        return False

    f.write(hex_common.code_fmt(f"""\
    paddr_t __attribute__((unused)) phys = 0;
    int __attribute__((unused)) prot;
    int32_t __attribute__((unused)) excp;
    CoprocArgs args = {{0}};
    args.opcode = {tag};
    args.unit = env->threadId;
"""))
    for i, (regtype, regid) in enumerate(regs, 1):
        if (regtype == 'R' and regid == 's'):
            f.write(hex_common.code_fmt(f"""\
    hex_tlb_find_match(env, {regtype}{regid}V, MMU_DATA_LOAD,
        &phys, &prot, &args.page_size, &excp,
        cpu_mmu_index(env_cpu(thread), false));
    args.arg{i} = phys + ({regtype}{regid}V & (args.page_size - 1));
"""))
        else:
            f.write(f"    args.arg{i} = {regtype}{regid}V;\n")
    f.write("""    coproc(&args);
""")
    return True
