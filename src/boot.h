#ifndef BOOT_H
#define BOOT_H
#include <stdbool.h>

bool boot_init(void);
int boot_handle_command(const char *args);

#endif /* BOOT_H */
