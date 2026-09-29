#ifndef CLI_H
#define CLI_H

#include "bst.h"

u64 cli_parse_u64(const char *value, const char *name);
u64 cli_parse_positive_u64(const char *value, const char *name);
u64 cli_parse_u64_scaled(const char *value, const char *name, u64 scale);
u64 cli_parse_positive_u64_scaled(const char *value, const char *name,
                                  u64 scale);
u64 cli_parse_arena_capacity(const char *value, const char *name);
u64 cli_parse_positive_arena_capacity(const char *value, const char *name);
u64 cli_parse_array_count(const char *value, const char *name,
                          size_t elem_size);
u64 cli_parse_counter_count(const char *value, const char *name);
u64 cli_parse_positive_counter_count(const char *value, const char *name);
u64 cli_parse_step_count(const char *value, const char *name);
u64 cli_parse_positive_step_count(const char *value, const char *name);
u64 cli_parse_sample_count(const char *value, const char *name);
u64 cli_parse_positive_sample_count(const char *value, const char *name);
u64 cli_parse_iteration_count(const char *value, const char *name);
u64 cli_parse_positive_iteration_count(const char *value, const char *name);
u64 cli_parse_duration_ms_ns(const char *value, const char *name);
int cli_parse_int(const char *value, const char *name);
bool cli_parse_choice_line(const char *line, int count, int *out);
const char *cli_parse_nonempty_string(const char *value, const char *name);
const char *cli_arg_value(int argc, char **argv, int *index, const char *flag);
int cli_unknown_arg(const char *arg);

#endif // CLI_H
