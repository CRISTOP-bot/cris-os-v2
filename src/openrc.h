#ifndef OPENRC_H
#define OPENRC_H
#include <stdbool.h>

bool openrc_init(void);
int openrc_handle_command(const char *args);

#endif /* OPENRC_H */
