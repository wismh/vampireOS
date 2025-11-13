#include "e820.h"
#include "vga.h"

static const char *e820_type_name(uint32_t type)
{
    switch (type) {
    case 1:
        return "usable";
    case 2:
        return "reserved";
    case 3:
        return "acpi";
    case 4:
        return "nvs";
    default:
        return "?";
    }
}

int e820_print(const struct e820_map *map)
{
    uint32_t i;
    uint32_t n = 0;
    int row = 2;

    if (map != 0 && map->entry_size == E820_ENTRY_SIZE) {
        n = map->count;
        if (n > E820_MAX) {
            n = E820_MAX;
        }
    }

    vga_write_at(row, 0, "mem ");
    vga_write_dec_at(row, 4, n);
    row++;

    for (i = 0; i < n && row < VGA_HEIGHT - 2; i++) {
        const struct e820_entry *e = &map->entries[i];

        vga_write_hex64_at(row, 0, e->base);
        vga_write_hex64_at(row, 17, e->length);
        vga_write_at(row, 34, e820_type_name(e->type));
        row++;
    }

    return row;
}
