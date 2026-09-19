#ifndef JGK_EVDEV_H
#define JGK_EVDEV_H

#include "jgk_tiles.h"
#include <poll.h>

int evdev_init(void);
void evdev_shutdown(void);
void evdev_set_pollfds(struct pollfd *fds, int *nfds);
void evdev_poll(App *app, struct pollfd *fds, int nfds);

#endif
