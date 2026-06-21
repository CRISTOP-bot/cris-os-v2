#ifndef SYSTEMD_H
#define SYSTEMD_H

int systemd_init(void);
int systemd_handle_command(const char *args);

#endif /* SYSTEMD_H */
