/*
 * Header with function prototypes to help device tree manipulation using
 * libfdt. It also provides functions to read entries from device tree proc
 * interface.
 *
 * Copyright 2008 IBM Corporation.
 * Authors: Jerone Young <jyoung5@us.ibm.com>
 *          Hollis Blanchard <hollisb@us.ibm.com>
 *          Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * This work is licensed under the GNU GPL license version 2 or later.
 *
 */

#include "exec/hwaddr.h"

#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

#define FDT_ADDRESS_CELLS   "#address-cells"
#define FDT_SIZE_CELLS      "#size-cells"
#define FDT_REG             "reg"

void *create_device_tree(int *sizep);
void *load_device_tree(const char *filename_path, int *sizep);
void save_device_tree(void *fdt, const char* filename_path, Error **errp);
#ifdef CONFIG_LINUX
/**
 * load_device_tree_from_sysfs: reads the device tree information in the
 * /proc/device-tree directory and return the corresponding binary blob
 * buffer pointer. Asserts in case of error.
 */
void *load_device_tree_from_sysfs(void);
#endif

/**
 * qemu_fdt_node_path: return the paths of nodes matching a given
 * name and compat string
 * @fdt: pointer to the dt blob
 * @name: node name
 * @compat: compatibility string
 * @errp: handle to an error object
 *
 * returns a newly allocated NULL-terminated array of node paths.
 * Use g_strfreev() to free it. If one or more nodes were found, the
 * array contains the path of each node and the last element equals to
 * NULL. If there is no error but no matching node was found, the
 * returned array contains a single element equal to NULL. If an error
 * was encountered when parsing the blob, the function returns NULL
 *
 * @name may be NULL to wildcard names and only match compatibility
 * strings.
 */
char **qemu_fdt_node_path(void *fdt, const char *name, const char *compat,
                          Error **errp);

/**
 * qemu_fdt_find_node_path_by_label: return the node path of a label,
 * if the symbol table is present.
 * @fdt: pointer to the dt blob
 * @label: label name
 * @errp: handle to an error object
 * 
 * dt blobs often contain a symbol table linking label names with
 * nodes. This function retrives the symbol table, and returns the
 * node linked to the label.
 * 
 * If the symbol table is not present, or another error is found,
 * NULL is returned.
 */
const char *qemu_fdt_node_path_by_label(void *fdt, const char *label,
                          Error **errp);

/**
 * qemu_fdt_node_unit_path: return the paths of nodes matching a given
 * node-name, ie. node-name and node-name@unit-address
 * @fdt: pointer to the dt blob
 * @name: node name
 * @errp: handle to an error object
 *
 * returns a newly allocated NULL-terminated array of node paths.
 * Use g_strfreev() to free it. If one or more nodes were found, the
 * array contains the path of each node and the last element equals to
 * NULL. If there is no error but no matching node was found, the
 * returned array contains a single element equal to NULL. If an error
 * was encountered when parsing the blob, the function returns NULL
 */
char **qemu_fdt_node_unit_path(void *fdt, const char *name, Error **errp);

int qemu_fdt_setprop(void *fdt, const char *node_path,
                     const char *property, const void *val, int size);
int qemu_fdt_setprop_cell(void *fdt, const char *node_path,
                          const char *property, uint32_t val);
int qemu_fdt_setprop_u64(void *fdt, const char *node_path,
                         const char *property, uint64_t val);
int qemu_fdt_setprop_string(void *fdt, const char *node_path,
                            const char *property, const char *string);

/**
 * qemu_fdt_setprop_string_array: set a string array property
 *
 * @fdt: pointer to the dt blob
 * @name: node name
 * @prop: property array
 * @array: pointer to an array of string pointers
 * @len: length of array
 *
 * assigns a string array to a property. This function converts and
 * array of strings to a sequential string with \0 separators before
 * setting the property.
 */
int qemu_fdt_setprop_string_array(void *fdt, const char *node_path,
                                  const char *prop, char **array, int len);

int qemu_fdt_setprop_phandle(void *fdt, const char *node_path,
                             const char *property,
                             const char *target_node_path);
/**
 * qemu_fdt_getprop: retrieve the value of a given property
 * @fdt: pointer to the device tree blob
 * @node_path: node path
 * @property: name of the property to find
 * @lenp: fdt error if any or length of the property on success
 * @errp: handle to an error object
 *
 * returns a pointer to the property on success and NULL on failure
 */
const void *qemu_fdt_getprop(void *fdt, const char *node_path,
                             const char *property, int *lenp,
                             Error **errp);

/**
 * qemu_fdt_getprop_string: retrieve the value of a given property as a string
 * @fdt: pointer to the device tree blob
 * @node_path: node path
 * @property: name of the property to find
 * @lenp: fdt error if any or length of the property on success
 * @errp: handle to an error object
 *
 * returns a string pointer to the property on success and NULL on failure
 */
const char *qemu_fdt_getprop_string(void *fdt, const char *node_path,
                             const char *property, Error **errp);

/**
 * qemu_fdt_getprop_reg: retrieve the value of a given property as a "reg"
 * according to dt specification v0.4.
 * 
 * @fdt: pointer to the device tree blob
 * @node_path: node path
 * @nb_addr_cells: set to the number of u32 per addr cell.
 * @nb_size_cells: set to the number of u32 per size cell.
 * @total_nb_regs: set to the number of registers found. It is the number of (addr, size) pairs.
 * @values: set to an array containing the value of each register
 * @errp: handle to an error object
 *
 * returns true on success and false on failure.
 */
bool qemu_fdt_getprop_reg(void *fdt,
                          int node_offset,
                          uint32_t *nb_reg_cells,
                          uint32_t *nb_size_cells,
                          uint32_t *total_nb_regs,
                          uint32_t **reg,
                          Error **errp);

/**
 * qemu_fdt_getprop_reg_as_u64: retrieve the value of a given property as a "reg"
 * in the same format as for the "qemu_fdt_setprop_sized_cells_from_array" function.
 * 
 * @fdt: pointer to the device tree blob
 * @node_offset: offset in the tdb to the node.
 * @nb_addr_cells: set to the number of u32 per addr cell.
 * @nb_size_cells: set to the number of u32 per size cell.
 * @total_nb_regs: set to the number of registers found. It is the number of (addr, size) pairs.
 * @values: set to an array containing the value of each register
 * @errp: handle to an error object
 *
 * returns true on success and false on failure.
 */
bool qemu_fdt_getprop_reg_as_u64(void* fdt,
                          int node_offset,
                          uint64_t** reg,
                          uint32_t* nb_regs,
                          Error **errp);

/**
 * qemu_fdt_getprop_cell: retrieve the value of a given 4 byte property
 * @fdt: pointer to the device tree blob
 * @node_path: node path
 * @property: name of the property to find
 * @lenp: fdt error if any or -EINVAL if the property size is different from
 *        4 bytes, or 4 (expected length of the property) upon success.
 * @errp: handle to an error object
 *
 * returns the property value on success
 */
uint32_t qemu_fdt_getprop_cell(void *fdt, const char *node_path,
                               const char *property, int *lenp,
                               Error **errp);
uint32_t qemu_fdt_get_phandle(void *fdt, const char *path);
uint32_t qemu_fdt_alloc_phandle(void *fdt);
int qemu_fdt_nop_node(void *fdt, const char *node_path);
int qemu_fdt_add_subnode(void *fdt, const char *name);
int qemu_fdt_add_path(void *fdt, const char *path);

#define qemu_fdt_setprop_cells(fdt, node_path, property, ...)                 \
    do {                                                                      \
        uint32_t qdt_tmp[] = { __VA_ARGS__ };                                 \
        for (unsigned i_ = 0; i_ < ARRAY_SIZE(qdt_tmp); i_++) {               \
            qdt_tmp[i_] = cpu_to_be32(qdt_tmp[i_]);                           \
        }                                                                     \
        qemu_fdt_setprop(fdt, node_path, property, qdt_tmp,                   \
                         sizeof(qdt_tmp));                                    \
    } while (0)

/**
 * qemu_fdt_setprop_sized_cells_from_array:
 * @fdt: device tree blob
 * @node_path: node to set property on
 * @property: property to set
 * @numvalues: number of values
 * @values: array of number-of-cells, value pairs
 *
 * Set the specified property on the specified node in the device tree
 * to be an array of cells. The values of the cells are specified via
 * the values list, which alternates between "number of cells used by
 * this value" and "value".
 * number-of-cells must be either 1 or 2 (other values will result in
 * an error being returned). If a value is too large to fit in the
 * number of cells specified for it, an error is returned.
 *
 * This function is useful because device tree nodes often have cell arrays
 * which are either lists of addresses or lists of address,size tuples, but
 * the number of cells used for each element vary depending on the
 * #address-cells and #size-cells properties of their parent node.
 * If you know all your cell elements are one cell wide you can use the
 * simpler qemu_fdt_setprop_cells(). If you're not setting up the
 * array programmatically, qemu_fdt_setprop_sized_cells may be more
 * convenient.
 *
 * Return value: 0 on success, <0 on error.
 */
int qemu_fdt_setprop_sized_cells_from_array(void *fdt,
                                            const char *node_path,
                                            const char *property,
                                            int numvalues,
                                            uint64_t *values);

/**
 * qemu_fdt_setprop_sized_cells:
 * @fdt: device tree blob
 * @node_path: node to set property on
 * @property: property to set
 * @...: list of number-of-cells, value pairs
 *
 * Set the specified property on the specified node in the device tree
 * to be an array of cells. The values of the cells are specified via
 * the variable arguments, which alternates between "number of cells
 * used by this value" and "value".
 *
 * This is a convenience wrapper for the function
 * qemu_fdt_setprop_sized_cells_from_array().
 *
 * Return value: 0 on success, <0 on error.
 */
#define qemu_fdt_setprop_sized_cells(fdt, node_path, property, ...)       \
    ({                                                                    \
        uint64_t qdt_tmp[] = { __VA_ARGS__ };                             \
        qemu_fdt_setprop_sized_cells_from_array(fdt, node_path,           \
                                                property,                 \
                                                ARRAY_SIZE(qdt_tmp) / 2,  \
                                                qdt_tmp);                 \
    })


/**
 * qemu_fdt_randomize_seeds:
 * @fdt: device tree blob
 *
 * Re-randomize all "rng-seed" properties with new seeds.
 */
void qemu_fdt_randomize_seeds(void *fdt);

/**
 * qemu_fdt_copy_node: copy a node from an input fdt to an
 * output fdt. All the subnodes and properties get recursively
 * copied over the output fdt as well.
 * 
 * Returns false if the node already exists in out_fdt, true otherwise.
 * 
 * TODO: rename phandles to avoid collision with existing phandles...
 * 
 * @out_fdt: pointer to the output dt blob
 * @in_fdt: pointer to the input dt blob, with the given node path.
 * @node_path: path to the target node
 * @errp: handle to an error object
 */
bool qemu_fdt_copy_node(void *out_fdt, void *in_fdt, const char *node_path,
                        Error **errp);

/**
 * qemu_fdt_copy_node_properties: copy a node's properties from an input fdt
 * to an output fdt.
 * 
 * @out_fdt: pointer to the output dt blob
 * @in_fdt: pointer to the input dt blob, with the given node path.
 * @node_path: path to the target node
 * @errp: handle to an error object
 */
void qemu_fdt_copy_node_properties(void *out_fdt, void *in_fdt, const char *node_path,
                        Error **errp);

/**
 * qemu_fdt_delprop: delete a node's property.
 * 
 * @fdt: pointer to the dt blob
 * @node_path: path to the target node
 * @property: the property to delete
 * @errp: handle to an error object
 */
void qemu_fdt_delprop(void *fdt, const char *node_path, const char *property, Error **errp);

/**
 * qemu_fdt_get_node_addr: get a node's address, if there is one.
 * An error is raised if no address can be found.
 * 
 * @fdt: pointer to the dt blob
 * @node_path: path to the target node
 * @addr: the addr, if any
 * @errp: handle to an error object
 * 
 * returns true if the address was found, false otherwise.
 */
bool qemu_fdt_get_node_addr(void *fdt, const char *node_path, hwaddr *addr, Error **errp);

/**
 * qemu_fdt_set_node_addr: set a node's address.
 * 
 * @fdt: pointer to the dt blob
 * @node_path: path to the target node
 * @addr: the address to set
 * @errp: handle to an error object
 */
bool qemu_fdt_set_node_addr(void *fdt, const char *node_path, hwaddr node_base_addr, Error **errp);

/**
 * use the current node addr (if there is any) as offset addr.
 */
bool qemu_fdt_set_nodes_addr(void *fdt, const char *node_path, hwaddr root_node_base_addr, Error **errp);

#define FDT_PCI_RANGE_RELOCATABLE          0x80000000
#define FDT_PCI_RANGE_PREFETCHABLE         0x40000000
#define FDT_PCI_RANGE_ALIASED              0x20000000
#define FDT_PCI_RANGE_TYPE_MASK            0x03000000
#define FDT_PCI_RANGE_MMIO_64BIT           0x03000000
#define FDT_PCI_RANGE_MMIO                 0x02000000
#define FDT_PCI_RANGE_IOPORT               0x01000000
#define FDT_PCI_RANGE_CONFIG               0x00000000

#endif /* DEVICE_TREE_H */
