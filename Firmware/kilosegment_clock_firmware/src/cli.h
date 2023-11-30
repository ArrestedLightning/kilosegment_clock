#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef void (*cli_handler)(char *cmd_str, char *response, size_t response_len);

typedef struct {
	cli_handler handler;
	const char *cmd_name;
	const char *cmd_help;
} cli_command;

#define cmd_def(handler, cmd, desc) {handler, cmd, desc}

bool parse_command(char *cmd_str, char *response, size_t response_size);