/*
 * Functions to help device tree manipulation using libfdt.
 * It also provides functions to read entries from device tree proc
 * interface.
 *
 * Copyright 2008 IBM Corporation.
 * Authors: Jerone Young <jyoung5@us.ibm.com>
 *          Hollis Blanchard <hollisb@us.ibm.com>
 *
 * This work is licensed under the GNU GPL license version 2 or later.
 *
 */

#include "qemu/osdep.h"

#ifdef CONFIG_LINUX
#include <dirent.h>
#endif

#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/bswap.h"
#include "qemu/cutils.h"
#include "qemu/guest-random.h"
#include "system/device_tree.h"
#include "hw/loader.h"
#include "hw/boards.h"
#include "qapi/qapi-commands-machine.h"

#include <libfdt.h>

#define FDT_MAX_SIZE  0x100000

#define FDT_PATH_MAX_LEN 512

struct reg64 {
    // ordering matters, it is used to work with qemu_fdt_setprop_sized_cells_from_array.
    union {
        struct {
            uint64_t address_cells;
            uint64_t addr;
            uint64_t size_cells;
            uint64_t size;
        };

        uint64_t raw[4];
    };
};

struct phandle_entry {
    const char* name;
    const char* size_name;
};

struct phandle_entry phandle_entries[] = {
    { .name = "clocks", .size_name = "#clock-cells" },
    { .name = "cooling-device", .size_name = "#cooling-cells" },
    { .name = "dmas", .size_name = "#dma-cells" },
    { .name = "hwlocks", .size_name = "#hwlock-cells" },
    { .name = "interconnects", .size_name = "#interconnect-cells" },
    { .name = "interrupts-extended", .size_name = "#interrupt-cells" },
    { .name = "io-channels", .size_name = "#io-channel-cells" },
    { .name = "iommus", .size_name = "#iommu-cells" },
    { .name = "mboxes", .size_name = "#mbox-cells" },
    { .name = "msi-parent", .size_name = "#msi-cells" },
    { .name = "mux-controls", .size_name = "#mux-control-cells" },
    { .name = "phys", .size_name = "#phy-cells" },
    { .name = "power-domains", .size_name = "#power-domain-cells" },
    { .name = "pwms", .size_name = "#pwm-cells" },
    { .name = "qcom,bcm-voters", .size_name = NULL }, // qcom specific
    { .name = "resets", .size_name = "#reset-cells" },
    { .name = "sound-dai", .size_name = "#sound-dai-cells" },
    { .name = "vdd_gfx-supply", .size_name = NULL }, // qcom specific
    { .name = "thermal-sensors", .size_name = "#thermal-sensor-cells" }
};

struct prop_iter {
    const void* prop;
    const char* name;
    int total_len;
    int current_offset;
};

static struct prop_iter create_prop_iter(void *fdt, int prop_offset) {
    struct prop_iter iter = { 0 };
    iter.prop = fdt_getprop_by_offset(fdt, prop_offset, &iter.name, &iter.total_len);

    return iter;
}

static bool peek_u32(struct prop_iter* iter, uint32_t* val) {
    // use subtraction to avoid possible overflows
    if (iter->total_len - iter->current_offset < sizeof(uint32_t)) {
        return false;
    }

    *val = be32_to_cpu(*(uint32_t*)(iter->prop + iter->current_offset));

    return true;
}

static bool get_u32(struct prop_iter* iter, uint32_t* val) {
    bool ret = peek_u32(iter, val);

    if (ret) {
        iter->current_offset += sizeof(uint32_t);
    }
    
    return ret;
}

static inline uint32_t prop_to_u32(const void* prop_data) {
    return be32_to_cpu(*(const uint32_t*) prop_data);
}

static inline uint32_t prop_to_phandle(const void* prop_data) {
    return prop_to_u32(prop_data);
}

static inline const char* prop_to_string(const void* prop_data) {
    return (const char*)prop_data;
}

static void skip_u32(struct prop_iter* iter, uint32_t nb_skips) {
    int total_skip = sizeof(uint32_t) * nb_skips;
    int remaining_len = iter->total_len - iter->current_offset; // safe since we control both variables

    if (total_skip > 0 && total_skip <= remaining_len) {
        iter->current_offset += total_skip;
    } else if (total_skip > remaining_len) {
        // assign total_len when we try to skip beyond the total length.
        iter->current_offset = iter->total_len;
    }
}

void *create_device_tree(int *sizep)
{
    void *fdt;
    int ret;

    *sizep = FDT_MAX_SIZE;
    fdt = g_malloc0(FDT_MAX_SIZE);
    ret = fdt_create(fdt, FDT_MAX_SIZE);
    if (ret < 0) {
        goto fail;
    }
    ret = fdt_finish_reservemap(fdt);
    if (ret < 0) {
        goto fail;
    }
    ret = fdt_begin_node(fdt, "");
    if (ret < 0) {
        goto fail;
    }
    ret = fdt_end_node(fdt);
    if (ret < 0) {
        goto fail;
    }
    ret = fdt_finish(fdt);
    if (ret < 0) {
        goto fail;
    }
    ret = fdt_open_into(fdt, fdt, *sizep);
    if (ret) {
        error_report("%s: Unable to copy device tree into memory: %s",
                     __func__, fdt_strerror(ret));
        exit(1);
    }

    return fdt;
fail:
    error_report("%s Couldn't create dt: %s", __func__, fdt_strerror(ret));
    exit(1);
}

void *load_device_tree(const char *filename_path, int *sizep)
{
    int dt_size;
    int dt_file_load_size;
    int ret;
    void *fdt = NULL;

    *sizep = 0;
    dt_size = get_image_size(filename_path);
    if (dt_size < 0) {
        error_report("Unable to get size of device tree file '%s'",
                     filename_path);
        goto fail;
    }
    if (dt_size > INT_MAX / 2 - 10000) {
        error_report("Device tree file '%s' is too large", filename_path);
        goto fail;
    }

    /* Expand to 2x size to give enough room for manipulation.  */
    dt_size += 10000;
    dt_size *= 2;
    /* First allocate space in qemu for device tree */
    fdt = g_malloc0(dt_size);

    dt_file_load_size = load_image_size(filename_path, fdt, dt_size);
    if (dt_file_load_size < 0) {
        error_report("Unable to open device tree file '%s'",
                     filename_path);
        goto fail;
    }

    ret = fdt_open_into(fdt, fdt, dt_size);
    if (ret) {
        error_report("%s: Unable to copy device tree into memory: %s",
                     __func__, fdt_strerror(ret));
        goto fail;
    }

    /* Check sanity of device tree */
    if (fdt_check_header(fdt)) {
        error_report("Device tree file loaded into memory is invalid: %s",
                     filename_path);
        goto fail;
    }
    *sizep = dt_size;
    return fdt;

fail:
    g_free(fdt);
    return NULL;
}

void save_device_tree(void *fdt, const char* filename_path, Error **errp)
{
    uint32_t dt_size = fdt_totalsize(fdt);
    int fd = qemu_create(filename_path, O_WRONLY | O_BINARY, 0644, errp);

    int ret = qemu_write_full(fd, fdt, dt_size);
    assert(ret == dt_size);

    qemu_close(fd);
}

#ifdef CONFIG_LINUX

#define SYSFS_DT_BASEDIR "/proc/device-tree"

/**
 * read_fstree: this function is inspired from dtc read_fstree
 * @fdt: preallocated fdt blob buffer, to be populated
 * @dirname: directory to scan under SYSFS_DT_BASEDIR
 * the search is recursive and the tree is searched down to the
 * leaves (property files).
 *
 * the function asserts in case of error
 */
static void read_fstree(void *fdt, const char *dirname)
{
    DIR *d;
    struct dirent *de;
    struct stat st;
    const char *root_dir = SYSFS_DT_BASEDIR;
    const char *parent_node;

    if (strstr(dirname, root_dir) != dirname) {
        error_report("%s: %s must be searched within %s",
                     __func__, dirname, root_dir);
        exit(1);
    }
    parent_node = &dirname[strlen(SYSFS_DT_BASEDIR)];

    d = opendir(dirname);
    if (!d) {
        error_report("%s cannot open %s", __func__, dirname);
        exit(1);
    }

    while ((de = readdir(d)) != NULL) {
        char *tmpnam;

        if (!g_strcmp0(de->d_name, ".")
            || !g_strcmp0(de->d_name, "..")) {
            continue;
        }

        tmpnam = g_strdup_printf("%s/%s", dirname, de->d_name);

        if (lstat(tmpnam, &st) < 0) {
            error_report("%s cannot lstat %s", __func__, tmpnam);
            exit(1);
        }

        if (S_ISREG(st.st_mode)) {
            gchar *val;
            gsize len;

            if (!g_file_get_contents(tmpnam, &val, &len, NULL)) {
                error_report("%s not able to extract info from %s",
                             __func__, tmpnam);
                exit(1);
            }

            if (strlen(parent_node) > 0) {
                qemu_fdt_setprop(fdt, parent_node,
                                 de->d_name, val, len);
            } else {
                qemu_fdt_setprop(fdt, "/", de->d_name, val, len);
            }
            g_free(val);
        } else if (S_ISDIR(st.st_mode)) {
            char *node_name;

            node_name = g_strdup_printf("%s/%s",
                                        parent_node, de->d_name);
            qemu_fdt_add_subnode(fdt, node_name);
            g_free(node_name);
            read_fstree(fdt, tmpnam);
        }

        g_free(tmpnam);
    }

    closedir(d);
}

/* load_device_tree_from_sysfs: extract the dt blob from host sysfs */
void *load_device_tree_from_sysfs(void)
{
    void *host_fdt;
    int host_fdt_size;

    host_fdt = create_device_tree(&host_fdt_size);
    read_fstree(host_fdt, SYSFS_DT_BASEDIR);
    if (fdt_check_header(host_fdt)) {
        error_report("%s host device tree extracted into memory is invalid",
                     __func__);
        exit(1);
    }
    return host_fdt;
}

#endif /* CONFIG_LINUX */

static int findnode_nofail(void *fdt, const char *node_path)
{
    int offset;

    offset = fdt_path_offset(fdt, node_path);
    if (offset < 0) {
        error_report("%s Couldn't find node %s: %s", __func__, node_path,
                     fdt_strerror(offset));
        exit(1);
    }

    return offset;
}

bool qemu_fdt_node_exists(void *fdt, const char *node_path)
{
    return fdt_path_offset(fdt, node_path) >= 0;
}

char **qemu_fdt_node_unit_path(void *fdt, const char *name, Error **errp)
{
    char *prefix =  g_strdup_printf("%s@", name);
    unsigned int path_len = 16, n = 0;
    GSList *path_list = NULL, *iter;
    const char *iter_name;
    int offset, len, ret;
    char **path_array;

    offset = fdt_next_node(fdt, -1, NULL);

    while (offset >= 0) {
        iter_name = fdt_get_name(fdt, offset, &len);
        if (!iter_name) {
            offset = len;
            break;
        }
        if (!strcmp(iter_name, name) || g_str_has_prefix(iter_name, prefix)) {
            char *path;

            path = g_malloc(path_len);
            while ((ret = fdt_get_path(fdt, offset, path, path_len))
                  == -FDT_ERR_NOSPACE) {
                path_len += 16;
                path = g_realloc(path, path_len);
            }
            path_list = g_slist_prepend(path_list, path);
            n++;
        }
        offset = fdt_next_node(fdt, offset, NULL);
    }
    g_free(prefix);

    if (offset < 0 && offset != -FDT_ERR_NOTFOUND) {
        error_setg(errp, "%s: abort parsing dt for %s node units: %s",
                   __func__, name, fdt_strerror(offset));
        for (iter = path_list; iter; iter = iter->next) {
            g_free(iter->data);
        }
        g_slist_free(path_list);
        return NULL;
    }

    path_array = g_new(char *, n + 1);
    path_array[n--] = NULL;

    for (iter = path_list; iter; iter = iter->next) {
        path_array[n--] = iter->data;
    }

    g_slist_free(path_list);

    return path_array;
}

char **qemu_fdt_node_path(void *fdt, const char *name, const char *compat,
                          Error **errp)
{
    int offset, len, ret;
    const char *iter_name;
    unsigned int path_len = 16, n = 0;
    GSList *path_list = NULL, *iter;
    char **path_array;

    offset = fdt_node_offset_by_compatible(fdt, -1, compat);

    while (offset >= 0) {
        iter_name = fdt_get_name(fdt, offset, &len);
        if (!iter_name) {
            offset = len;
            break;
        }
        if (!name || !strcmp(iter_name, name)) {
            char *path;

            path = g_malloc(path_len);
            while ((ret = fdt_get_path(fdt, offset, path, path_len))
                  == -FDT_ERR_NOSPACE) {
                path_len += 16;
                path = g_realloc(path, path_len);
            }
            path_list = g_slist_prepend(path_list, path);
            n++;
        }
        offset = fdt_node_offset_by_compatible(fdt, offset, compat);
    }

    if (offset < 0 && offset != -FDT_ERR_NOTFOUND) {
        error_setg(errp, "%s: abort parsing dt for %s/%s: %s",
                   __func__, name, compat, fdt_strerror(offset));
        for (iter = path_list; iter; iter = iter->next) {
            g_free(iter->data);
        }
        g_slist_free(path_list);
        return NULL;
    }

    path_array = g_new(char *, n + 1);
    path_array[n--] = NULL;

    for (iter = path_list; iter; iter = iter->next) {
        path_array[n--] = iter->data;
    }

    g_slist_free(path_list);

    return path_array;
}

const char *qemu_fdt_node_path_by_label(void *fdt, const char *label,
                          Error **errp)
{
    const void *label_node = qemu_fdt_getprop_string(fdt, "/__symbols__", label, errp);
    if (!label) {
        error_setg(errp, "%s: Couldn't find the symbol table, "
                         "or the label %s does not exist",
                         __func__, label);
    }

    return label_node;
}

int qemu_fdt_setprop(void *fdt, const char *node_path,
                     const char *property, const void *val, int size)
{
    int r;

    r = fdt_setprop(fdt, findnode_nofail(fdt, node_path), property, val, size);
    if (r < 0) {
        error_report("%s: Couldn't set %s/%s: %s", __func__, node_path,
                     property, fdt_strerror(r));
        exit(1);
    }

    return r;
}

int qemu_fdt_setprop_bool(void *fdt, const char *node_path,
                     const char *property)
{
    return qemu_fdt_setprop(fdt, node_path, property, NULL, 0);
}

int qemu_fdt_setprop_cell(void *fdt, const char *node_path,
                          const char *property, uint32_t val)
{
    int r;

    r = fdt_setprop_cell(fdt, findnode_nofail(fdt, node_path), property, val);
    if (r < 0) {
        error_report("%s: Couldn't set %s/%s = %#08x: %s", __func__,
                     node_path, property, val, fdt_strerror(r));
        exit(1);
    }

    return r;
}

int qemu_fdt_setprop_u64(void *fdt, const char *node_path,
                         const char *property, uint64_t val)
{
    val = cpu_to_be64(val);
    return qemu_fdt_setprop(fdt, node_path, property, &val, sizeof(val));
}

int qemu_fdt_setprop_string(void *fdt, const char *node_path,
                            const char *property, const char *string)
{
    int r;

    r = fdt_setprop_string(fdt, findnode_nofail(fdt, node_path), property, string);
    if (r < 0) {
        error_report("%s: Couldn't set %s/%s = %s: %s", __func__,
                     node_path, property, string, fdt_strerror(r));
        exit(1);
    }

    return r;
}

/*
 * libfdt doesn't allow us to add string arrays directly but they are
 * test a series of null terminated strings with a length. We build
 * the string up here so we can calculate the final length.
 */
int qemu_fdt_setprop_string_array(void *fdt, const char *node_path,
                                  const char *prop, char **array, int len)
{
    int ret, i, total_len = 0;
    char *str, *p;
    for (i = 0; i < len; i++) {
        total_len += strlen(array[i]) + 1;
    }
    p = str = g_malloc0(total_len);
    for (i = 0; i < len; i++) {
        int offset = strlen(array[i]) + 1;
        pstrcpy(p, offset, array[i]);
        p += offset;
    }

    ret = qemu_fdt_setprop(fdt, node_path, prop, str, total_len);
    g_free(str);
    return ret;
}

const void *qemu_fdt_getprop(void *fdt, const char *node_path,
                             const char *property, int *lenp, Error **errp)
{
    int len;
    const void *r;

    if (!lenp) {
        lenp = &len;
    }
    r = fdt_getprop(fdt, findnode_nofail(fdt, node_path), property, lenp);
    if (!r) {
        error_setg(errp, "%s: Couldn't get %s/%s: %s", __func__,
                  node_path, property, fdt_strerror(*lenp));
    }
    return r;
}

const char *qemu_fdt_getprop_string(void *fdt, const char *node_path,
                             const char* property, Error **errp)
{
    int len;
    const void* ret = qemu_fdt_getprop(fdt, node_path, property, &len, errp);

    // exit early if there was an error while parsing @property
    if (!ret) {
        return ret;
    }

    // cast to string, and check correctness
    const char* ret_str = (const char*) ret;
    if (ret_str[len - 1] != '\0') {
        error_setg(errp, "%s: Could not cast %s as string - "
                         "property is not NULL-terminated (len: %d, end char: %c)",
                         __func__, ret_str, len, ret_str[len - 1]);
    }

    return ret_str;
}

uint32_t qemu_fdt_getprop_cell(void *fdt, const char *node_path,
                               const char *property, int *lenp, Error **errp)
{
    int len;
    const uint32_t *p;

    if (!lenp) {
        lenp = &len;
    }
    p = qemu_fdt_getprop(fdt, node_path, property, lenp, errp);
    if (!p) {
        return 0;
    } else if (*lenp != 4) {
        error_setg(errp, "%s: %s/%s not 4 bytes long (not a cell?)",
                   __func__, node_path, property);
        *lenp = -EINVAL;
        return 0;
    }
    return be32_to_cpu(*p);
}

static bool getprop_reg(void *fdt,
                          int node_offset,
                          uint32_t *nb_reg_cells,
                          uint32_t *nb_size_cells,
                          uint32_t *total_nb_regs,
                          uint32_t **reg,
                          Error **errp)
{
    int parent_node = fdt_parent_offset(fdt, node_offset);
    int len;

    const uint32_t *address_cells = fdt_getprop(fdt, parent_node, QEMU_FDT_PROP_ADDRESS_CELLS, &len);
    if (len != 4) {
        return false;
    }

    assert(len == 4);
    *nb_reg_cells = be32_to_cpu(*address_cells);

    const uint32_t *size_cells = fdt_getprop(fdt, parent_node, QEMU_FDT_PROP_SIZE_CELLS, &len);
    assert(len == 4);
    *nb_size_cells = be32_to_cpu(*size_cells);

    uint32_t cell_size = (*nb_reg_cells + *nb_size_cells) * sizeof(uint32_t);

    const uint32_t *in_reg = fdt_getprop(fdt, node_offset, QEMU_FDT_PROP_REG, &len);

    if (len < 0) {
        return false;
    }

    *total_nb_regs = len / cell_size;
    *reg = g_new(uint32_t, *total_nb_regs * (*nb_reg_cells + *nb_size_cells));

    assert(len == sizeof(uint32_t) * *total_nb_regs * (*nb_reg_cells + *nb_size_cells));

    memcpy(*reg, in_reg, len);

    return true;
}

static bool getprop_reg_as_u64(void* fdt, int node_offset, struct reg64** reg, uint32_t* nb_regs, Error **errp)
{
    uint32_t nb_reg_cells, nb_size_cells;

    uint32_t *reg_32 = NULL;
    if (!getprop_reg(fdt, node_offset, &nb_reg_cells, &nb_size_cells, nb_regs, &reg_32, errp)) {
        *nb_regs = 0;
        return false;
    }
    assert(nb_reg_cells == 1 || nb_reg_cells == 2);
    assert(nb_size_cells <= 2);

    struct reg64 *new_reg = g_new(struct reg64, *nb_regs);

    for (size_t i = 0; i < *nb_regs; i++) {
        new_reg[i].address_cells = nb_reg_cells;
        new_reg[i].addr = (((uint64_t) be32_to_cpu(reg_32[4 * i + 0])) << 32) + be32_to_cpu(reg_32[4 * i + 1]);
        new_reg[i].size_cells = nb_size_cells;
        new_reg[i].size = (((uint64_t) be32_to_cpu(reg_32[4 * i + 2])) << 32) + be32_to_cpu(reg_32[4 * i + 3]);
    }

    g_free(reg_32);

    *reg = new_reg;

    return true;
}

bool qemu_fdt_getprop_reg(void* fdt,
                            const char* node_path,
                            struct fdt_reg** regs,
                            uint32_t* nb_regs,
                            Error **errp)
{
    int node_offset = findnode_nofail(fdt, node_path);
    struct reg64* regs_64;

    if (getprop_reg_as_u64(fdt, node_offset, &regs_64, nb_regs, errp)) {
        *regs = g_new(struct fdt_reg, *nb_regs);

        for (size_t i = 0; i < *nb_regs; ++i) {
            (*regs)[i].addr = regs_64[i].addr;
            (*regs)[i].size = regs_64[i].size;
        }

        g_free(regs_64);

        return true;
    }

    return false;
}

uint32_t qemu_fdt_get_phandle(void *fdt, const char *path)
{
    uint32_t r;

    r = fdt_get_phandle(fdt, findnode_nofail(fdt, path));
    if (r == 0) {
        error_report("%s: Couldn't get phandle for %s: %s", __func__,
                     path, fdt_strerror(r));
        exit(1);
    }

    return r;
}

int qemu_fdt_setprop_phandle(void *fdt, const char *node_path,
                             const char *property,
                             const char *target_node_path)
{
    uint32_t phandle = qemu_fdt_get_phandle(fdt, target_node_path);
    return qemu_fdt_setprop_cell(fdt, node_path, property, phandle);
}

uint32_t qemu_fdt_alloc_phandle(void *fdt)
{
    static int phandle = 0x0;

    /*
     * We need to find out if the user gave us special instruction at
     * which phandle id to start allocating phandles.
     */
    if (!phandle) {
        phandle = machine_phandle_start(current_machine);
    }

    if (!phandle) {
        /*
         * None or invalid phandle given on the command line, so fall back to
         * default starting point.
         */
        phandle = 0x8000;
    }

    return phandle++;
}

int qemu_fdt_nop_node(void *fdt, const char *node_path)
{
    int r;

    r = fdt_nop_node(fdt, findnode_nofail(fdt, node_path));
    if (r < 0) {
        error_report("%s: Couldn't nop node %s: %s", __func__, node_path,
                     fdt_strerror(r));
        exit(1);
    }

    return r;
}

int qemu_fdt_add_subnode(void *fdt, const char *name)
{
    char *dupname = g_strdup(name);
    char *basename = strrchr(dupname, '/');
    int retval;
    int parent = 0;

    if (!basename) {
        g_free(dupname);
        return -1;
    }

    basename[0] = '\0';
    basename++;

    if (dupname[0]) {
        parent = findnode_nofail(fdt, dupname);
    }

    retval = fdt_add_subnode(fdt, parent, basename);
    if (retval < 0) {
        error_report("%s: Failed to create subnode %s: %s",
                     __func__, name, fdt_strerror(retval));
        exit(1);
    }

    g_free(dupname);
    return retval;
}

/*
 * qemu_fdt_add_path: Like qemu_fdt_add_subnode(), but will add
 * all missing subnodes from the given path.
 */
int qemu_fdt_add_path(void *fdt, const char *path)
{
    const char *name;
    int namelen, retval;
    int parent = 0;

    if (path[0] != '/') {
        return -1;
    }

    do {
        name = path + 1;
        path = strchr(name, '/');
        namelen = path != NULL ? path - name : strlen(name);

        retval = fdt_subnode_offset_namelen(fdt, parent, name, namelen);
        if (retval < 0 && retval != -FDT_ERR_NOTFOUND) {
            error_report("%s: Unexpected error in finding subnode %.*s: %s",
                         __func__, namelen, name, fdt_strerror(retval));
            exit(1);
        } else if (retval == -FDT_ERR_NOTFOUND) {
            retval = fdt_add_subnode_namelen(fdt, parent, name, namelen);
            if (retval < 0) {
                error_report("%s: Failed to create subnode %.*s: %s",
                             __func__, namelen, name, fdt_strerror(retval));
                exit(1);
            }
        }

        parent = retval;
    } while (path);

    return retval;
}

int qemu_fdt_setprop_sized_cells_from_array(void *fdt,
                                            const char *node_path,
                                            const char *property,
                                            int numvalues,
                                            uint64_t *values)
{
    uint32_t *propcells;
    uint64_t value;
    int cellnum, vnum, ncells;
    uint32_t hival;
    int ret;

    propcells = g_new0(uint32_t, numvalues * 2);

    cellnum = 0;
    for (vnum = 0; vnum < numvalues; vnum++) {
        ncells = values[vnum * 2];
        if (ncells != 1 && ncells != 2) {
            ret = -1;
            goto out;
        }
        value = values[vnum * 2 + 1];
        hival = cpu_to_be32(value >> 32);
        if (ncells > 1) {
            propcells[cellnum++] = hival;
        } else if (hival != 0) {
            ret = -1;
            goto out;
        }
        propcells[cellnum++] = cpu_to_be32(value);
    }

    ret = qemu_fdt_setprop(fdt, node_path, property, propcells,
                           cellnum * sizeof(uint32_t));
out:
    g_free(propcells);
    return ret;
}

static bool qemu_fdt_setprop_reg_as_u64(void* fdt, const char* node_path, struct reg64* reg, uint32_t nb_regs, Error **errp)
{
    return qemu_fdt_setprop_sized_cells_from_array(fdt, node_path, QEMU_FDT_PROP_REG, nb_regs * 2, (uint64_t*) reg) >= 0;
}

void qmp_dumpdtb(const char *filename, Error **errp)
{
    ERRP_GUARD();

    g_autoptr(GError) err = NULL;
    uint32_t size;

    if (!current_machine->fdt) {
        error_setg(errp, "This machine doesn't have an FDT");
        error_append_hint(errp,
                          "(Perhaps it doesn't support FDT at all, or perhaps "
                          "you need to provide an FDT with the -fdt option?)\n");
        return;
    }

    size = fdt_totalsize(current_machine->fdt);

    g_assert(size > 0);

    if (!g_file_set_contents(filename, current_machine->fdt, size, &err)) {
        error_setg(errp, "Error saving FDT to file %s: %s",
                   filename, err->message);
    }
}

void qemu_fdt_randomize_seeds(void *fdt)
{
    int noffset, poffset, len;
    const char *name;
    uint8_t *data;

    for (noffset = fdt_next_node(fdt, 0, NULL);
         noffset >= 0;
         noffset = fdt_next_node(fdt, noffset, NULL)) {
        for (poffset = fdt_first_property_offset(fdt, noffset);
             poffset >= 0;
             poffset = fdt_next_property_offset(fdt, poffset)) {
            data = (uint8_t *)fdt_getprop_by_offset(fdt, poffset, &name, &len);
            if (!data || strcmp(name, "rng-seed"))
                continue;
            qemu_guest_getrandom_nofail(data, len);
        }
    }
}

// returns true if out_fdt has been updated.
static bool handle_phandle_node(void *out_fdt, void *in_fdt, const char* node_path, struct phandle_entry* pentry, int in_prop_offset, const char* nodes_path[], Error **errp)
{
    int len, in_node_offset;
    bool updated = false;
    char phandle_node_path[FDT_PATH_MAX_LEN] = { 0 };
    int res;

    struct prop_iter iter = create_prop_iter(in_fdt, in_prop_offset);
    
    uint32_t phandle, val;
    while (get_u32(&iter, &phandle)) {
        in_node_offset = fdt_node_offset_by_phandle(in_fdt, phandle);

        if (in_node_offset < 0) {
            goto fail;
        }

        // in theory, we should dereference after the assert...
        int cell_size;
        if (pentry->size_name) {
            cell_size = prop_to_u32(fdt_getprop(in_fdt, in_node_offset, pentry->size_name, &len));
            assert(len == 4);
        } else {
            cell_size = 0;
        }

        res = fdt_get_path(in_fdt, in_node_offset, phandle_node_path, FDT_PATH_MAX_LEN);
        assert(res >= 0);
        
        // check if node exists in out fdt.
        int out_phandle_offset = fdt_path_offset(out_fdt, phandle_node_path);
        if (out_phandle_offset == -FDT_ERR_NOTFOUND) {
            // check if the phandle destination could be added later on in the node path group
            const char** added_node;
            bool skip = false;
            for (added_node = nodes_path; added_node != NULL && *added_node != NULL; ++added_node) {
                if (strncmp(phandle_node_path, *added_node, strlen(*added_node)) == 0) {
                    skip = true;
                }
            }

            if (!skip) {
                warn_report("The node %s has a phandle reference to the node %s, but it is not present in the new DT. It is most likely a device dependency that should be implemented and added beforehand.", node_path, phandle_node_path);
            }
        } else if (out_phandle_offset < 0) {
            in_node_offset = out_phandle_offset;
            goto fail;
        }

        skip_u32(&iter, cell_size);

        // sometimes there is a 0 that should not be there...
        // bug in dts file?
        // TODO: remove when fix is pushed
        if (peek_u32(&iter, &val) && val == 0) {
            skip_u32(&iter, 1);
        }
    }

    return updated;

fail:
    error_setg(errp, "%s: Couldn't find phandle 0x%x for property %s: %s", __func__, phandle, pentry->name, fdt_strerror(in_node_offset));

    return updated;
}

// return the new out_node_offset, since it could be updated.
static int copy_properties(void* out_fdt, void* in_fdt, int out_node_offset, int in_node_offset, const char* in_node_path, const char* nodes_path[], bool ignore_existing_props, Error **errp)
{
    const void *prop;
    const char *prop_name = NULL;
    int prop_offset, len, ret;
    const char *path;
    bool path_allocated = false;

    if (in_node_path == NULL) {
        path_allocated = true;

        path = g_new0(char, FDT_PATH_MAX_LEN);
        ret = fdt_get_path(in_fdt, in_node_offset, (char*) path, FDT_PATH_MAX_LEN);
        assert(ret >= 0);
    } else {
        path = in_node_path;
    }

    fdt_for_each_property_offset(prop_offset, in_fdt, in_node_offset) {
        prop = fdt_getprop_by_offset(in_fdt, prop_offset, &prop_name, &len);

        if (!prop) {
            ret = len;
            goto fail;
        }

        // check the property does not exist already
        {
            int out_len;
            const void* out_prop = fdt_getprop(out_fdt, out_node_offset, prop_name, &out_len);

            if (out_prop) {
                if (ignore_existing_props) {
                    continue;
                } else {
                    ret = FDT_ERR_EXISTS;
                    goto fail;
                }
            } else if (out_len != -FDT_ERR_NOTFOUND) {
                ret = out_len;
                goto fail;
            }
        }

        // check nodes by phandle if they contain some.
        bool out_fdt_updated = false;
        if (!strcmp(prop_name, QEMU_FDT_PROP_INTERRUPT_PARENT)) {
            // when an interrupt parent property is met, try to update the phandle
            // to a compatible interrupt controller.
            int in_ic_node_offset = fdt_node_offset_by_phandle(in_fdt, prop_to_phandle(prop));
            int compatible_prop_len;

            const char* ic_compatible = prop_to_string(
                fdt_getprop(in_fdt, in_ic_node_offset, QEMU_FDT_PROP_COMPATIBLE, &compatible_prop_len)
            );
            assert(ic_compatible);
            assert(compatible_prop_len > 0);

            int compatible_out_ic_node_offset;
            int offset = 0;
            while(offset < compatible_prop_len) {
                compatible_out_ic_node_offset = fdt_node_offset_by_compatible(out_fdt, -1, ic_compatible);
                if (compatible_out_ic_node_offset >= 0) {
                    break;
                }

                size_t compatible_len = strlen(ic_compatible) + 1;

                ic_compatible += compatible_len;
                offset += compatible_len;
            }

            if (compatible_out_ic_node_offset < 0) {
                warn_report("The interrupt controller %s from the input DT did not find a compatible IC in the output DT.", path);
            } else {
                // we found a compatible node in the out DT, update the phandle accordingly.
                uint32_t out_phandle = fdt_get_phandle(out_fdt, compatible_out_ic_node_offset);
                fdt_setprop_inplace_cell(out_fdt, out_node_offset, QEMU_FDT_PROP_PHANDLE, out_phandle);
                prop = NULL;
            }
        } else {
            for (size_t i = 0; i < ARRAY_SIZE(phandle_entries); ++i) {
                if (!strcmp(prop_name, phandle_entries[i].name)) {
                    out_fdt_updated |= handle_phandle_node(out_fdt, in_fdt, path, &phandle_entries[i], prop_offset, nodes_path, errp);
                }
            }
        }

        // if phandle nodes have been added, update the node offset.
        if (out_fdt_updated) {
            out_node_offset = fdt_path_offset(out_fdt, path);
        }

        if (prop) {
            ret = fdt_setprop(out_fdt, out_node_offset, prop_name, prop, len);

            if (ret < 0) {
                goto fail;
            }
        }
    }

    if (path_allocated) {
        g_free((char*) path);
    }

    return out_node_offset;

fail:
    if (path_allocated) {
        g_free((char*) path);
    }

    error_setg(errp, "%s: Couldn't copy property %s in node %s: %s", __func__, prop_name, in_node_path, fdt_strerror(ret));

    return out_node_offset;
}

void qemu_fdt_copy_node_properties(void *out_fdt, void *in_fdt, const char *node_path, Error **errp)
{
    int in_node_offset = findnode_nofail(in_fdt, node_path);
    int out_node_offset = findnode_nofail(out_fdt, node_path);

    const char* nodes_path[] = {
        node_path,
        NULL
    };

    copy_properties(out_fdt, in_fdt, out_node_offset, in_node_offset, node_path, nodes_path, false, errp);
}

// returns the new subnode offset, or the fdt error code.
static int qemu_fdt_copy_subnode_recursive(void *out_fdt, void *in_fdt, int in_node_offset, int out_parent_node_offset, const char* subnode_name, const char* nodes_path[], Error **errp)
{
    int len, err;

    // first, add the new subnode.
    int out_node_offset = fdt_add_subnode(out_fdt, out_parent_node_offset, subnode_name);

    // next, add all the missing properties
    out_node_offset = copy_properties(out_fdt, in_fdt, out_node_offset, in_node_offset, NULL, nodes_path, false, errp);

    // add the subnodes recursively
    int in_subnode_offset;
    fdt_for_each_subnode(in_subnode_offset, in_fdt, in_node_offset) {
        subnode_name = fdt_get_name(in_fdt, in_subnode_offset, &len);

        if (subnode_name == NULL) {
            err = len;
            goto fail;
        }

        qemu_fdt_copy_subnode_recursive(out_fdt, in_fdt, in_subnode_offset, out_node_offset, subnode_name, nodes_path, errp);
    }

    return out_node_offset;

fail:
    error_setg(errp, "%s: Couldn't create subnode %s: %s", __func__, subnode_name, fdt_strerror(err));
    return err;
}

// initialize the given node path, alongside the parent nodes and their properties if they are not instanciated yet.
static int initialize_root_node(void* out_fdt, void* in_fdt, const char* node_path, const char* nodes_path[], Error **errp)
{
    int in_node_offset, out_node_offset, out_parent_node_offset, err;
    const char* parent_end;

    if (!strcmp(node_path, "/")) {
        // stop recursion if we are at the root of the tree
        return fdt_path_offset(out_fdt, "/");
    }

    parent_end = strrchr(node_path, '/');
    assert(parent_end != NULL);

    size_t parent_path_len = parent_end == node_path ? 1 : parent_end - node_path;

    char* parent_node_path = g_new0(char, parent_path_len + 1);
    memcpy(parent_node_path, node_path, parent_path_len);

    out_parent_node_offset = fdt_path_offset(out_fdt, parent_node_path);
    if (out_parent_node_offset == -FDT_ERR_NOTFOUND) {
        // parent not found, we need to create it recursively
        out_parent_node_offset = initialize_root_node(out_fdt, in_fdt, parent_node_path, nodes_path, errp);
    } else if (out_parent_node_offset < 0) {
        err = out_parent_node_offset;
        goto fail;
    }

    g_free(parent_node_path);
    
    // fatal error, this should never happen
    if (out_parent_node_offset < 0) {
        err = out_parent_node_offset;
        goto fail;
    }

    // the parent is found, we can now create the node with the properties
    out_node_offset = fdt_add_subnode(out_fdt, out_parent_node_offset, parent_end + 1);
    if (out_node_offset < 0) {
        err = out_node_offset;
        goto fail;
    }

    // locate the node to copy from fdt input.
    in_node_offset = fdt_path_offset(in_fdt, node_path);
    if (in_node_offset < 0) {
        err = in_node_offset;
        goto fail;
    }

    return copy_properties(out_fdt, in_fdt, out_node_offset, in_node_offset, node_path, nodes_path, false, errp);

fail:
    error_setg(errp, "%s: Couldn't create node %s: %s", __func__, node_path, fdt_strerror(err));
    return err;
}

// null terminated list of strings
bool qemu_fdt_copy_nodes(void *out_fdt, void *in_fdt, const char* nodes_path[], Error **errp)
{
    int len;
    const char *subnode_name;
    int in_node_offset;
    int ret = 0;
    bool copied = false;

    const char** node_path;

    for (node_path = nodes_path; *node_path != NULL; ++node_path) {
        in_node_offset = findnode_nofail(in_fdt, *node_path);

        // check if node exists in out fdt.
        if (fdt_path_offset(out_fdt, *node_path) >= 0) {
            continue;
        }

        // first, add the root node and its parent, with their respective properties.
        int out_node_offset = initialize_root_node(out_fdt, in_fdt, *node_path, nodes_path, errp);

        // now, we can add all the subnodes recursively, since the root node is set.
        int in_subnode_offset;
        fdt_for_each_subnode(in_subnode_offset, in_fdt, in_node_offset) {
            subnode_name = fdt_get_name(in_fdt, in_subnode_offset, &len);

            if (!subnode_name) {
                ret = len;
                goto fail;
            }

            qemu_fdt_copy_subnode_recursive(out_fdt, in_fdt, in_subnode_offset, out_node_offset, subnode_name, nodes_path, errp);
        }

        copied = true;
    }

    return copied;

fail:
    error_setg(errp, "%s: Couldn't copy node %s: %s", __func__, *node_path, fdt_strerror(ret));

    return copied;
}

bool qemu_fdt_copy_node(void *out_fdt, void *in_fdt, const char *node_path, Error **errp)
{
    const char* nodes[] = {
        node_path,
        NULL
    };

    return qemu_fdt_copy_nodes(out_fdt, in_fdt, nodes, errp);
}

void qemu_fdt_delprop(void *fdt, const char *node_path, const char *property, Error **errp)
{
    int ret;
    int node_offset = findnode_nofail(fdt, node_path);

    ret = fdt_delprop(fdt, node_offset, property);

    if (ret < 0) {
        error_setg(errp, "%s: Couldn't delete property %s from node %s: %s", __func__, property, node_path, fdt_strerror(ret));
    }
}

bool qemu_fdt_get_node_addr(void *fdt, const char *node_path, hwaddr *addr, Error **errp)
{
    const char* last_node = strrchr(node_path, '/');
    const char* addr_str = strrchr(node_path, '@');

    if (!addr_str || addr_str < last_node) {
        return false;
    }

    addr_str++;

    // note on dt spec v0.4: not clear it should always be in base 16
    int ret = parse_uint(addr_str, NULL, 16, addr);

    if (ret < 0) {
        error_setg(errp, "%s: Could not parse integer: %s", __func__, addr_str);
        return false;
    }

    return true;
}

char* qemu_fdt_set_node_addr(void *fdt, const char *node_path, hwaddr node_base_addr, Error **errp)
{
    int node_offset = findnode_nofail(fdt, node_path);
    const char* node_name = strrchr(node_path, '/') + 1; // cannot fail
    const char* addr_str = strrchr(node_path, '@');
    hwaddr old_base_addr;
    char* new_name = NULL;

    if (!addr_str || addr_str < node_name) {
        return NULL;
    }

    // note on dt spec v0.4: not clear it should always be in base 16
    int ret = parse_uint(addr_str + 1, NULL, 16, &old_base_addr);

    // dirty fix to pass illegal addresses used by some nodes.
    // TODO: remove when fixed in the actual specification.
    if (old_base_addr <= 128) {
        return NULL;
    }

    size_t str_len = addr_str - node_name;
    char* stripped_name = g_new0(char, str_len + 1); // null terminated

    strncpy(stripped_name, node_name, str_len);

    new_name = g_strdup_printf("%s@%lx", stripped_name, node_base_addr);
    assert(new_name);

    // first, edit register addresses.
    // the dt specification v0.4 enforces the first register to be equal to the new addr.
    // we will try to edit every register as well, if applicable.
    struct reg64* reg;
    uint32_t nb_regs;
    assert(getprop_reg_as_u64(fdt, node_offset, &reg, &nb_regs, errp));

    for (size_t i = 0; i < nb_regs; i++) {
        reg[i].addr = reg[i].addr - old_base_addr + node_base_addr;
    }

    assert(qemu_fdt_setprop_reg_as_u64(fdt, node_path, reg, nb_regs, errp));
    
    // then, change the final name
    // we do it after so that "node_path" remains valid
    // until this point.
    ret = fdt_set_name(fdt, node_offset, new_name);

    g_free(reg);

    if (ret < 0) {
        g_free(new_name);
        error_setg(errp, "%s: Could not set addr 0x%lx for node %s: %s", __func__, node_base_addr, node_path, fdt_strerror(ret));
        return NULL;
    }


    return new_name;
}

static char* get_root_node(const char* node_path)
{
    char *root_pos = strrchr(node_path, '/');
    if (root_pos == node_path) {
        return NULL;
    }

    char* root_path = g_new0(char, root_pos - node_path + 1);
    memcpy(root_path, node_path, root_pos - node_path);


    return root_path;
}

void qemu_fdt_set_nodes_addr(void *fdt, const char *node_path, hwaddr root_node_base_addr, Error **errp)
{
    int len;
    const char *subnode_name;
    char* subnode_path;
    int node_offset = findnode_nofail(fdt, node_path);
    char* new_node_path = NULL;
    int ret = 0;
    hwaddr addr;

    // TODO: remove when handled...
    if (strstr(node_path, "qcom,gpu-pwrlevel-bins")) {
        return;
    }

    // check if the current node has an address to reset.
    if (qemu_fdt_get_node_addr(fdt, node_path, &addr, errp)) {
        char* node_root = get_root_node(node_path);
        char* new_node_name = qemu_fdt_set_node_addr(fdt, node_path, root_node_base_addr + addr, errp);
        if (new_node_name) {
            if (node_root) {
                new_node_path = g_strdup_printf("%s/%s", node_root, new_node_name);
                g_free(node_root);
            } else {
                new_node_path = g_strdup_printf("/%s", new_node_name);
            }
            g_free(new_node_name);
        }
    }

    // now, we can add all the subnodes recursively, since the root node is set.
    int subnode_offset;
    fdt_for_each_subnode(subnode_offset, fdt, node_offset) {
        subnode_name = fdt_get_name(fdt, subnode_offset, &len);

        if (!subnode_name) {
            ret = len;
            goto fail;
        }

        if (new_node_path) {
            subnode_path = g_strdup_printf("%s/%s", new_node_path, subnode_name);
        } else {
            subnode_path = g_strdup_printf("%s/%s", node_path, subnode_name);
        }

        qemu_fdt_set_nodes_addr(fdt, subnode_path, root_node_base_addr, errp);
        g_free(subnode_path);
    }

    if (new_node_path) {
        g_free(new_node_path);
    }

    return;

fail:
    error_setg(errp, "%s: Couldn't copy node %s: %s", __func__, node_path, fdt_strerror(ret));
}

void qemu_fdt_check_memory_consistency(void* fdt, const char* node_path, MemoryRegion* root_mem, Error **errp)
{
    int node_offset = findnode_nofail(fdt, node_path);
    struct reg64* regs;
    uint32_t nb_regs;
    const char *subnode_name;
    char* subnode_path;
    int len, ret;

    // TODO: remove when handled...
    if (strstr(node_path, "qcom,gpu-pwrlevel-bins")) {
        return;
    }

    bool is_mapped = true;
    if (getprop_reg_as_u64(fdt, node_offset, &regs, &nb_regs, errp)) {
        for (size_t i = 0; i < nb_regs; ++i) {
            hwaddr addr = regs[i].addr;
            uint64_t size = regs[i].size;
            
            if (!address_space_access_valid(&address_space_memory, addr,
                                size, true,
                                MEMTXATTRS_UNSPECIFIED)) {
                is_mapped = false;
                break;
            }
        }
    }

    if (!is_mapped) {
        warn_report("The node \"%s\" is not fully mapped in memory, although it appears in the DT blob.\n"
            "This may cause issues in the target.",
            node_path);

        for (size_t i = 0; i < nb_regs; ++i) {
            hwaddr addr = regs[i].addr;
            uint64_t size = regs[i].size;
            
            if (!address_space_access_valid(&address_space_memory, addr,
                                size, true,
                                MEMTXATTRS_UNSPECIFIED)) {
                printf("\tAddress:  0x%lx -> 0x%lx (%lx bytes)\n",
                    addr, addr + size,
                    size);
                is_mapped = false;
            }
        }

    }

    // now, we can add all the subnodes recursively, since the root node is set.
    int subnode_offset;
    fdt_for_each_subnode(subnode_offset, fdt, node_offset) {
        subnode_name = fdt_get_name(fdt, subnode_offset, &len);

        if (!subnode_name) {
            ret = len;
            goto fail;
        }

        subnode_path = g_strdup_printf("%s/%s", node_path, subnode_name);
        qemu_fdt_check_memory_consistency(fdt, subnode_path, root_mem, errp);
        g_free(subnode_path);
    }

    return;
fail:
    error_setg(errp, "%s: Couldn't check node %s: %s", __func__, node_path, fdt_strerror(ret));
}

/**
 * qemu_fdt_of_is_compatible() - Check if the node matches given constraints
 * @compat: required compatible string, NULL or "" for any match
 * @type: required device_type value, NULL or "" for any match
 * @name: required node name, NULL or "" for any match
 * 
 * Adapted from the linux kernel, may change in the future.
 *
 * Checks if the given @compat, @type and @name strings match the
 * properties of the given @device. A constraints can be skipped by
 * passing NULL or an empty string as the constraint.
 *
 * Returns 0 for no match, and a positive integer on match. The return
 * value is a relative score with larger values indicating better
 * matches. The score is weighted for the most specific compatible value
 * to get the highest score. Matching type is next, followed by matching
 * name. Practically speaking, this results in the following priority
 * order for matches:
 *
 * 1. specific compatible && type && name
 * 2. specific compatible && type
 * 3. specific compatible && name
 * 4. specific compatible
 * 5. general compatible && type && name
 * 6. general compatible && type
 * 7. general compatible && name
 * 8. general compatible
 * 9. type && name
 * 10. type
 * 11. name
 */
int qemu_fdt_of_is_compatible(void *fdt, const char* node_path, const char* compat, const char* type, const char* name)
{
    const void* prop;
    int len;
	int index = 0, score = 0;

    int node_offset = fdt_path_offset(fdt, node_path);
    assert(node_offset >= 0);

	/* Compatible match has highest priority */
	if (compat && compat[0]) {
        if (fdt_node_check_compatible(fdt, node_offset, compat) == 0) {
            score = INT_MAX/2 - (index << 2);
        }

		if (!score) {
			return 0;
        }
	}

	/* Matching type is better than matching name */
	if (type && type[0]) {
        prop = fdt_getprop(fdt, node_offset, "device_type", &len);
        bool is_type = prop && !strcmp((const char*) prop, type);
        if (!is_type) {
            return 0;
        }

        score += 2;
	}

	/* Matching name is a bit better than not */
	if (name && name[0]) {
        const char* node_name = fdt_get_name(fdt, node_offset, &len);

        len = strchrnul(node_name, '@') - node_name;
        bool name_matches = strlen(name) == len && strncmp(node_name, name, len) == 0;

		if (!name_matches) {
			return 0;
        }
		score++;
	}

	return score;
}

bool qemu_fdt_find_parent_interrupt_phandle(void* fdt, const char* node_path, uint32_t* phandle)
{
    int node_offset = findnode_nofail(fdt, node_path);
    int len;
    const void* prop_data;

    while(!(prop_data = fdt_getprop(fdt, node_offset, QEMU_FDT_PROP_INTERRUPT_PARENT, &len))) {
        node_offset = fdt_parent_offset(fdt, node_offset);
        if (node_offset < 0) {
            return false;
        }
    }
    assert(len == 4);

    *phandle = prop_to_u32(prop_data);

    return true;
}

bool qemu_fdt_getprop_interrupts(void* fdt, const char* node_path, struct fdt_interrupts** interrupts, Error** errp)
{
    int node_offset = findnode_nofail(fdt, node_path);
    const void* prop_data;
    uint32_t interrupt_size;
    int interrupts_len, len;
    int controller_node_offset;

    if (!(prop_data = fdt_getprop(fdt, node_offset, QEMU_FDT_PROP_INTERRUPTS, &interrupts_len))) {
        *interrupts = NULL;
        return false;
    }

    *interrupts = g_new(struct fdt_interrupts, 1);
    struct fdt_interrupts* _interrupts = *interrupts;

    // find interrupt controller phandle
    assert(qemu_fdt_find_parent_interrupt_phandle(fdt, node_path, &_interrupts->interrupt_controller_phandle));

    // get interrupt controller node offset
    // printf("phandle: 0x%x\n", _interrupts->interrupt_controller_phandle);
    // save_device_tree(fdt, "/tmp/lol.dtb", errp);
    controller_node_offset = fdt_node_offset_by_phandle(fdt, _interrupts->interrupt_controller_phandle);
    assert(controller_node_offset >= 0);

    // find the cell size for interrupts
    interrupt_size = prop_to_u32(fdt_getprop(fdt, controller_node_offset, QEMU_FDT_PROP_INTERRUPT_CELLS, &len));
    assert(len == 4);

    _interrupts->nb_interrupts = interrupts_len / (sizeof(uint32_t) * interrupt_size);
    _interrupts->interrupts = g_new(uint32_t, interrupts_len / sizeof(uint32_t));

    for (size_t i = 0; i < interrupts_len / sizeof(uint32_t); ++i) {
        _interrupts->interrupts[i] = prop_to_u32(prop_data);
        prop_data += sizeof(uint32_t);
    }

    return true;
}

void qemu_fdt_delnode(void *fdt, const char *node_path, Error **errp)
{
    int node_offset = findnode_nofail(fdt, node_path);
    assert(fdt_del_node(fdt, node_offset) == 0);
}

bool qemu_fdt_merge_node(void *out_fdt, void *in_fdt, const char *out_node_path, const char *in_node_path, Error **errp)
{
    int len;
    const char *subnode_name;
    int out_node_offset = findnode_nofail(out_fdt, out_node_path);
    int in_node_offset = findnode_nofail(in_fdt, in_node_path);
    int ret;

    out_node_offset = copy_properties(out_fdt, in_fdt, out_node_offset, in_node_offset, in_node_path, NULL, true, errp);

    // now, we can add all the subnodes recursively, since the root node is set.
    int in_subnode_offset;
    fdt_for_each_subnode(in_subnode_offset, in_fdt, in_node_offset) {
        subnode_name = fdt_get_name(in_fdt, in_subnode_offset, &len);

        if (!subnode_name) {
            ret = len;
            goto fail;
        }

        qemu_fdt_copy_subnode_recursive(out_fdt, in_fdt, in_subnode_offset, out_node_offset, subnode_name, NULL, errp);
    }

    return true;

fail:
    error_setg(errp, "%s: Couldn't copy node %s: %s", __func__, in_node_path, fdt_strerror(ret));

    return false;
}

bool qemu_fdt_check(const void* fdt, Error **errp)
{
    size_t dt_size = fdt_totalsize(fdt);

    int ret = fdt_check_full(fdt, dt_size);

    if (ret < 0) {
        error_setg(errp, "%s: found error in the dt: %s",
                   __func__, fdt_strerror(ret));
        return false;
    }

    return true;
}
