#ifndef E820_H
#define E820_H

#include <stdint.h>

/*
 * The bootloader stores the BIOS E820 map here:
 *
 * 0x5000 -> number of entries (16-bit)
 * 0x5004 -> first E820 entry
 */
#define E820_COUNT_ADDR   0x5000
#define E820_BUFFER_ADDR  0x5004

/*
 * BIOS E820 memory region types.
 */
#define E820_TYPE_USABLE    1
#define E820_TYPE_RESERVED  2
#define E820_TYPE_ACPI      3
#define E820_TYPE_NVS       4
#define E820_TYPE_BAD       5

/*
 * One BIOS E820 entry.
 *
 * This MUST remain packed because the bootloader stores
 * each entry as exactly 24 bytes.
 */
typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} e820_entry_t;


/*
 * Return the number of E820 entries detected by the bootloader.
 */
static inline uint16_t e820_get_entry_count(void) {
    volatile uint16_t *count =
        (volatile uint16_t *)E820_COUNT_ADDR;

    return *count;
}


/*
 * Return the E820 memory map.
 */
static inline volatile e820_entry_t *e820_get_entries(void) {
    return (volatile e820_entry_t *)E820_BUFFER_ADDR;
}

#endif