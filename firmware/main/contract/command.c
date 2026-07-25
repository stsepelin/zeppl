#include "command.h"
#include <stddef.h>

static command_handler_fn s_handler;

void command_register_handler(command_handler_fn handler)
{
    s_handler = handler;
}

bool command_dispatch(const command_t *cmd)
{
    if (cmd == NULL || s_handler == NULL)
        return false;
    s_handler(cmd);
    return true;
}
