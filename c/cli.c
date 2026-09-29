#include "cli.h"
#include "time_utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static bool cli_byte_needs_hex_escape(unsigned char byte) {
   return byte < ' ' || byte == 0x7f || byte >= 0x80;
}

static bool cli_ascii_space(unsigned char byte) {
   return byte == ' ' || (byte >= '\t' && byte <= '\r');
}

static bool cli_value_needs_escape(const char *value) {
   if (value == NULL) {
      return false;
   }
   for (const unsigned char *p = (const unsigned char *)value; *p != '\0';
        p++) {
      if (cli_byte_needs_hex_escape(*p) || *p == '\\') {
         return true;
      }
   }
   return false;
}

static void cli_fprint_value(FILE *file, const char *value) {
   invariant(file != NULL);
   if (value == NULL) {
      fputs("(null)", file);
      return;
   }
   if (!cli_value_needs_escape(value)) {
      fputs(value, file);
      return;
   }

   for (const unsigned char *p = (const unsigned char *)value; *p != '\0';
        p++) {
      switch (*p) {
      case '\n':
         fputs("\\n", file);
         break;
      case '\r':
         fputs("\\r", file);
         break;
      case '\t':
         fputs("\\t", file);
         break;
      case '\\':
         fputs("\\\\", file);
         break;
      default:
         if (cli_byte_needs_hex_escape(*p)) {
            fprintf(file, "\\x%02x", (unsigned int)*p);
         } else {
            fputc((int)*p, file);
         }
         break;
      }
   }
}

static void cli_invalid_value(const char *value, const char *name) {
   invariant(name != NULL);
   fprintf(stderr, "invalid %s: ", name);
   cli_fprint_value(stderr, value);
   fputc('\n', stderr);
   exit(2);
}

static void cli_u64_too_large(const char *value, const char *name) {
   invariant(name != NULL);
   fprintf(stderr, "%s is too large: ", name);
   cli_fprint_value(stderr, value);
   fputc('\n', stderr);
   exit(2);
}

static u64 cli_scale_u64_or_exit(u64 parsed, u64 scale, const char *value,
                                 const char *name) {
   invariant(scale > 0);
   if (parsed > UINT64_MAX / scale) {
      cli_u64_too_large(value, name);
   }
   return parsed * scale;
}

static u64 cli_parse_digits_u64(const char *value, const char *digits,
                                const char *name, u64 limit) {
   invariant(name != NULL);
   invariant(digits != NULL);
   if (digits[0] == '\0') {
      cli_invalid_value(value, name);
   }

   u64 limit_quotient = limit / 10;
   u64 limit_remainder = limit % 10;
   u64 parsed = 0;
   for (const char *p = digits; *p != '\0'; p++) {
      if (*p < '0' || *p > '9') {
         cli_invalid_value(value, name);
      }

      u64 digit = (u64)(*p - '0');
      if (parsed > limit_quotient ||
          (parsed == limit_quotient && digit > limit_remainder)) {
         cli_u64_too_large(value, name);
      }
      parsed = parsed * 10 + digit;
   }

   invariant(parsed <= limit);
   return parsed;
}

u64 cli_parse_u64(const char *value, const char *name) {
   invariant(name != NULL);
   if (value == NULL) {
      cli_invalid_value(value, name);
   }
   return cli_parse_digits_u64(value, value, name, UINT64_MAX);
}

u64 cli_parse_positive_u64(const char *value, const char *name) {
   u64 parsed = cli_parse_u64(value, name);
   if (parsed == 0) {
      cli_invalid_value(value, name);
   }
   return parsed;
}

u64 cli_parse_u64_scaled(const char *value, const char *name, u64 scale) {
   return cli_scale_u64_or_exit(cli_parse_u64(value, name), scale, value, name);
}

u64 cli_parse_positive_u64_scaled(const char *value, const char *name,
                                  u64 scale) {
   return cli_scale_u64_or_exit(cli_parse_positive_u64(value, name), scale,
                                value, name);
}

u64 cli_parse_arena_capacity(const char *value, const char *name) {
   u64 parsed = cli_parse_u64(value, name);
   if (parsed > arena_max_capacity()) {
      cli_u64_too_large(value, name);
   }
   return parsed;
}

u64 cli_parse_positive_arena_capacity(const char *value, const char *name) {
   u64 parsed = cli_parse_arena_capacity(value, name);
   if (parsed == 0) {
      cli_invalid_value(value, name);
   }
   return parsed;
}

u64 cli_parse_array_count(const char *value, const char *name,
                          size_t elem_size) {
   u64 parsed = cli_parse_u64(value, name);
   u64 max_count = bst_array_max_count(elem_size);
   if (parsed > max_count) {
      cli_u64_too_large(value, name);
   }
   return parsed;
}

u64 cli_parse_counter_count(const char *value, const char *name) {
   u64 parsed = cli_parse_u64(value, name);
   if (parsed == UINT64_MAX) {
      cli_u64_too_large(value, name);
   }
   return parsed;
}

u64 cli_parse_positive_counter_count(const char *value, const char *name) {
   u64 parsed = cli_parse_counter_count(value, name);
   if (parsed == 0) {
      cli_invalid_value(value, name);
   }
   return parsed;
}

u64 cli_parse_step_count(const char *value, const char *name) {
   return cli_parse_counter_count(value, name);
}

u64 cli_parse_positive_step_count(const char *value, const char *name) {
   return cli_parse_positive_counter_count(value, name);
}

u64 cli_parse_sample_count(const char *value, const char *name) {
   return cli_parse_counter_count(value, name);
}

u64 cli_parse_positive_sample_count(const char *value, const char *name) {
   return cli_parse_positive_counter_count(value, name);
}

u64 cli_parse_iteration_count(const char *value, const char *name) {
   return cli_parse_counter_count(value, name);
}

u64 cli_parse_positive_iteration_count(const char *value, const char *name) {
   return cli_parse_positive_counter_count(value, name);
}

u64 cli_parse_duration_ms_ns(const char *value, const char *name) {
   return cli_parse_positive_u64_scaled(value, name, TIME_NS_PER_MS);
}

int cli_parse_int(const char *value, const char *name) {
   invariant(name != NULL);
   if (value == NULL || value[0] == '\0') {
      cli_invalid_value(value, name);
   }

   bool negative = value[0] == '-';
   const char *digits = negative ? value + 1 : value;
   u64 min_int_magnitude = size_after_inserting_one((u64)INT_MAX);
   u64 limit = negative ? min_int_magnitude : (u64)INT_MAX;
   u64 parsed = cli_parse_digits_u64(value, digits, name, limit);

   if (!negative) {
      return (int)parsed;
   }
   if (parsed == min_int_magnitude) {
      return INT_MIN;
   }
   return -(int)parsed;
}

bool cli_parse_choice_line(const char *line, int count, int *out) {
   invariant(line != NULL);
   invariant(out != NULL);
   invariant(count > 0);

   const unsigned char *p = (const unsigned char *)line;
   if (*p < '0' || *p > '9') {
      return false;
   }

   int value = 0;
   int limit_quotient = count / 10;
   int limit_remainder = count % 10;
   do {
      int digit = (int)(*p - '0');
      if (value > limit_quotient ||
          (value == limit_quotient && digit > limit_remainder)) {
         return false;
      }
      value = value * 10 + digit;
      p++;
   } while (*p >= '0' && *p <= '9');

   while (cli_ascii_space(*p)) {
      p++;
   }
   if (*p != '\0' || value < 1) {
      return false;
   }

   invariant(value <= count);
   *out = (int)rank_of_last((u64)value);
   return true;
}

const char *cli_parse_nonempty_string(const char *value, const char *name) {
   invariant(name != NULL);
   if (value == NULL || value[0] == '\0') {
      cli_invalid_value(value, name);
   }
   return value;
}

const char *cli_arg_value(int argc, char **argv, int *index, const char *flag) {
   invariant(argc >= 0);
   invariant(argv != NULL);
   invariant(index != NULL);
   invariant(*index >= 0);
   invariant(*index < argc);
   invariant(flag != NULL);

   if (*index + 1 >= argc) {
      fputs("missing value for ", stderr);
      cli_fprint_value(stderr, flag);
      fputc('\n', stderr);
      exit(2);
   }

   *index = *index + 1;
   return argv[*index];
}

int cli_unknown_arg(const char *arg) {
   fputs("unknown or incomplete argument: ", stderr);
   cli_fprint_value(stderr, arg);
   fputc('\n', stderr);
   return 2;
}
