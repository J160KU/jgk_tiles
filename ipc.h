#ifndef JGK_IPC_H
#define JGK_IPC_H

void ipc_set_toggle_flag(int sig);
int ipc_toggle_requested(void);
void ipc_write_pid(void);
void ipc_remove_pid(void);
int ipc_send_toggle(void);
void ipc_install_signals(void);

#endif
