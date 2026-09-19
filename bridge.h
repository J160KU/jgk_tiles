#ifndef JGK_BRIDGE_H
#define JGK_BRIDGE_H

int bridge_store_focus(void);
int bridge_get_workarea(int *x, int *y, int *w, int *h);
int bridge_resize(int x, int y, int w, int h);
int bridge_overlay_toggle(void);
int bridge_overlay_hide(void);
int bridge_available(void);

#endif
