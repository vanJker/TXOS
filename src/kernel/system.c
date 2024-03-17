#include <xos/task.h>

mode_t sys_umask(mode_t mask) {
    task_t *current = current_task();
    mode_t old_mask = current->umask;
    current->umask = mask & 0777;
    return old_mask;
}
