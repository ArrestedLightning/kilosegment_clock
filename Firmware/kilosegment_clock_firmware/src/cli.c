#include "cli.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

extern const cli_command cli_commands[];


bool parse_command(char *cmd_str, char *response, size_t response_len) {
    bool result = false;
    const cli_command *current_command = &cli_commands[0];
    while (current_command->cmd_name) {
        if (strncmp(current_command->cmd_name, cmd_str, strlen(current_command->cmd_name)) == 0) {
            //The complete command string is passed to the handler, the command name needs to be parsed out by them.
            //possible future optimization is to parse out the command string here and pass just the args to the handler.
            current_command->handler(cmd_str, response, response_len);
            result = true;
            break;
        }
        current_command += 1;
    }
    return result;
}