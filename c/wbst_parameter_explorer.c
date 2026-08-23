#include "random.h"
#include "wbst_topdown.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
   u64 delta_num;
   u64 delta_den;
   u64 gamma_num;
   u64 gamma_den;
} parameter_pair_t;

typedef struct {
   node_t *node;
   u64 size;
   u64 depth;
} verify_frame_t;

typedef struct {
   bool found;
   const char *kind;
   u64 node_size;
   u64 left_size;
   u64 right_size;
   u64 depth;
} imbalance_t;

typedef struct {
   bool found;
   imbalance_t imbalance;
   const char *operation;
   u64 position;
   u64 tree_size;
   u64 operation_index;
   u64 trial;
   u64 run_seed;
} counterexample_t;

typedef struct {
   u64 delta_min;
   u64 delta_max;
   u64 delta_step;
   u64 gamma_min;
   u64 gamma_max;
   u64 gamma_step;
   u64 scale;
   u64 trials;
   u64 operations;
   u64 max_size;
   u64 seed;
   const char *output;
} explorer_config_t;

static parameter_pair_t active_pair;
static int signal_output = -1;
static counterexample_t signal_counterexample;

static bool counterexample_is_better(counterexample_t candidate,
                                     counterexample_t current);

static void write_counterexample_or_exit(int fd,
                                         const counterexample_t *counterexample) {
   const unsigned char *bytes = (const unsigned char *)counterexample;
   size_t remaining = sizeof(*counterexample);
   while (remaining > 0) {
      ssize_t written = write(fd, bytes, remaining);
      if (written <= 0) {
         _exit(2);
      }
      bytes += (size_t)written;
      remaining -= (size_t)written;
   }
}

static void explorer_signal_handler(int signal_number) {
   signal_counterexample.found = true;
   signal_counterexample.imbalance.found = true;
   signal_counterexample.imbalance.kind = "algorithm-signal";
   signal_counterexample.imbalance.node_size = (u64)signal_number;
   if (signal_output >= 0) {
      write_counterexample_or_exit(signal_output, &signal_counterexample);
   }
   _exit(128 + signal_number);
}

static void install_signal_handlers(void) {
   signal(SIGABRT, explorer_signal_handler);
   signal(SIGBUS, explorer_signal_handler);
   signal(SIGFPE, explorer_signal_handler);
   signal(SIGSEGV, explorer_signal_handler);
}

static bool parameter_is_balanced(u64 x, u64 y) {
   __uint128_t left =
       (__uint128_t)active_pair.delta_num * weight_u128_plus_one(x);
   __uint128_t right =
       (__uint128_t)active_pair.delta_den * weight_u128_plus_one(y);
   return left >= right;
}

static bool parameter_is_single_rotation(u64 x, u64 y) {
   __uint128_t left =
       (__uint128_t)active_pair.gamma_num * weight_u128_plus_one(x);
   __uint128_t right =
       (__uint128_t)active_pair.gamma_den * weight_u128_plus_one(y);
   return left > right;
}

static int parameter_max_height(u64 size) {
   (void)size;
   return INT_MAX;
}

static weight_balance_t parameter_weight_balance(void) {
   return (weight_balance_t){
       .isBalanced = parameter_is_balanced,
       .singleRotation = parameter_is_single_rotation,
       .maxHeight = parameter_max_height,
   };
}

static u64 parse_u64(const char *value, const char *name) {
   if (value == NULL || value[0] == '\0' || value[0] == '-') {
      fprintf(stderr, "invalid %s\n", name);
      exit(2);
   }
   errno = 0;
   char *end = NULL;
   uintmax_t parsed = strtoumax(value, &end, 10);
   if (errno != 0 || end == value || *end != '\0' || parsed > UINT64_MAX) {
      fprintf(stderr, "invalid %s: %s\n", name, value);
      exit(2);
   }
   return (u64)parsed;
}

static const char *next_arg(int argc, char **argv, int *index,
                            const char *name) {
   if (*index + 1 >= argc) {
      fprintf(stderr, "missing value for %s\n", name);
      exit(2);
   }
   *index = *index + 1;
   return argv[*index];
}

static explorer_config_t parse_config(int argc, char **argv) {
   explorer_config_t config = {
       .delta_min = 200,
       .delta_max = 480,
       .delta_step = 5,
       .gamma_min = 100,
       .gamma_max = 220,
       .gamma_step = 5,
       .scale = 100,
       .trials = 64,
       .operations = 512,
       .max_size = 256,
       .seed = 0x6a09e667f3bcc909ULL,
       .output =
           "docs/plots/figures/polytope/topdown-counterexamples.csv",
   };

   for (int i = 1; i < argc; i++) {
      const char *arg = argv[i];
      if (strcmp(arg, "-delta-min") == 0) {
         config.delta_min =
             parse_u64(next_arg(argc, argv, &i, arg), "delta minimum");
      } else if (strcmp(arg, "-delta-max") == 0) {
         config.delta_max =
             parse_u64(next_arg(argc, argv, &i, arg), "delta maximum");
      } else if (strcmp(arg, "-delta-step") == 0) {
         config.delta_step =
             parse_u64(next_arg(argc, argv, &i, arg), "delta step");
      } else if (strcmp(arg, "-gamma-min") == 0) {
         config.gamma_min =
             parse_u64(next_arg(argc, argv, &i, arg), "gamma minimum");
      } else if (strcmp(arg, "-gamma-max") == 0) {
         config.gamma_max =
             parse_u64(next_arg(argc, argv, &i, arg), "gamma maximum");
      } else if (strcmp(arg, "-gamma-step") == 0) {
         config.gamma_step =
             parse_u64(next_arg(argc, argv, &i, arg), "gamma step");
      } else if (strcmp(arg, "-scale") == 0) {
         config.scale = parse_u64(next_arg(argc, argv, &i, arg), "scale");
      } else if (strcmp(arg, "-trials") == 0) {
         config.trials = parse_u64(next_arg(argc, argv, &i, arg), "trials");
      } else if (strcmp(arg, "-operations") == 0) {
         config.operations =
             parse_u64(next_arg(argc, argv, &i, arg), "operations");
      } else if (strcmp(arg, "-max-size") == 0) {
         config.max_size =
             parse_u64(next_arg(argc, argv, &i, arg), "maximum size");
      } else if (strcmp(arg, "-seed") == 0) {
         config.seed = parse_u64(next_arg(argc, argv, &i, arg), "seed");
      } else if (strcmp(arg, "-output") == 0) {
         config.output = next_arg(argc, argv, &i, arg);
      } else {
         fprintf(stderr, "unknown argument: %s\n", arg);
         exit(2);
      }
   }

   if (config.scale == 0 || config.delta_step == 0 ||
       config.gamma_step == 0 || config.trials == 0 ||
       config.operations == 0 || config.max_size == 0 ||
       config.delta_min > config.delta_max ||
       config.gamma_min > config.gamma_max) {
      fprintf(stderr, "invalid explorer configuration\n");
      exit(2);
   }
   return config;
}

static imbalance_t find_imbalance(node_t *root, u64 size,
                                  verify_frame_t *stack, u64 stack_cap) {
   imbalance_t result = {0};
   if (root == NULL) {
      if (size != 0) {
         return (imbalance_t){
             .found = true,
             .kind = "root-shape",
             .node_size = size,
         };
      }
      return result;
   }

   u64 len = 0;
   stack[len++] = (verify_frame_t){.node = root, .size = size, .depth = 0};
   while (len > 0) {
      verify_frame_t frame = stack[--len];
      node_t *node = frame.node;
      if (node == NULL || frame.size == 0 || node->s >= frame.size) {
         return (imbalance_t){
             .found = true,
             .kind = "node-shape",
             .node_size = frame.size,
             .depth = frame.depth,
         };
      }

      u64 left_size = node->s;
      u64 right_size = frame.size - left_size - 1;
      if ((node->l == NULL) != (left_size == 0) ||
          (node->r == NULL) != (right_size == 0)) {
         return (imbalance_t){
             .found = true,
             .kind = "child-shape",
             .node_size = frame.size,
             .left_size = left_size,
             .right_size = right_size,
             .depth = frame.depth,
         };
      }

      if (frame.size >= 3 &&
          (!parameter_is_balanced(left_size, right_size) ||
           !parameter_is_balanced(right_size, left_size))) {
         return (imbalance_t){
             .found = true,
             .kind = "weight-balance",
             .node_size = frame.size,
             .left_size = left_size,
             .right_size = right_size,
             .depth = frame.depth,
         };
      }

      if (node->l != NULL) {
         if (len == stack_cap) {
            fprintf(stderr, "verification stack exhausted\n");
            exit(2);
         }
         stack[len++] = (verify_frame_t){
             .node = node->l,
             .size = left_size,
             .depth = frame.depth + 1,
         };
      }
      if (node->r != NULL) {
         if (len == stack_cap) {
            fprintf(stderr, "verification stack exhausted\n");
            exit(2);
         }
         stack[len++] = (verify_frame_t){
             .node = node->r,
             .size = right_size,
             .depth = frame.depth + 1,
         };
      }
   }
   return result;
}

static u64 pair_seed(const explorer_config_t *config, u64 delta, u64 gamma,
                     u64 trial) {
   u64 seed = random_derive_seed(config->seed, "wbst-topdown-parameter");
   seed = random_derive_u64(seed, delta);
   seed = random_derive_u64(seed, gamma);
   return random_derive_u64(seed, trial);
}

static counterexample_t run_trial(const explorer_config_t *config, u64 delta,
                                  u64 gamma, u64 trial, arena_t *arena,
                                  verify_frame_t *verify_stack) {
   arena_reset(arena);
   node_t *root = NULL;
   u64 size = 0;
   u64 seed = pair_seed(config, delta, gamma, trial);
   xoshiro256_t random;
   xoshiro256_seed(&random, seed);
   weight_balance_t wb = parameter_weight_balance();

   for (u64 operation_index = 1; operation_index <= config->operations;
        operation_index++) {
      bool insert =
          size == 0 ||
          (size < config->max_size &&
           random_less_than_src(100, &random) < 58);
      u64 position = insert ? random_less_than_src(size + 1, &random)
                            : random_less_than_src(size, &random);
      signal_counterexample = (counterexample_t){
          .found = true,
          .operation = insert ? "insert" : "delete",
          .position = position,
          .tree_size = insert ? size + 1 : size - 1,
          .operation_index = operation_index,
          .trial = trial,
          .run_seed = seed,
      };
      if (insert) {
         wbst_td_insert(wb, &root, size, position, operation_index);
         size++;
      } else {
         (void)wbst_td_delete_internal(wb, &root, size, position);
         size--;
      }

      imbalance_t imbalance =
          find_imbalance(root, size, verify_stack, config->max_size + 1);
      if (imbalance.found) {
         return (counterexample_t){
             .found = true,
             .imbalance = imbalance,
             .operation = insert ? "insert" : "delete",
             .position = position,
             .tree_size = size,
             .operation_index = operation_index,
             .trial = trial,
             .run_seed = seed,
         };
      }
   }
   return (counterexample_t){0};
}

static counterexample_t run_pair(const explorer_config_t *config, u64 delta,
                                 u64 gamma) {
   arena_t arena = {0};
   arena_init(&arena, config->operations + config->max_size + 16);
   arena_t *previous = arena_install(&arena);
   verify_frame_t *verify_stack =
       bst_malloc_array(config->max_size + 1, sizeof(verify_frame_t));

   counterexample_t best = {0};
   for (u64 trial = 0; trial < config->trials; trial++) {
      counterexample_t candidate =
          run_trial(config, delta, gamma, trial, &arena, verify_stack);
      if (counterexample_is_better(candidate, best)) {
         best = candidate;
      }
   }

   free(verify_stack);
   arena_restore(previous);
   arena_destroy(&arena);
   return best;
}

static counterexample_t run_pair_isolated(const explorer_config_t *config,
                                          u64 delta, u64 gamma) {
   int descriptors[2];
   if (pipe(descriptors) != 0) {
      perror("pipe");
      exit(2);
   }

   pid_t child = fork();
   if (child < 0) {
      perror("fork");
      exit(2);
   }
   if (child == 0) {
      close(descriptors[0]);
      signal_output = descriptors[1];
      install_signal_handlers();
      counterexample_t result = run_pair(config, delta, gamma);
      write_counterexample_or_exit(signal_output, &result);
      close(signal_output);
      _exit(0);
   }

   close(descriptors[1]);
   counterexample_t result = {0};
   unsigned char *bytes = (unsigned char *)&result;
   size_t remaining = sizeof(result);
   while (remaining > 0) {
      ssize_t received = read(descriptors[0], bytes, remaining);
      if (received < 0 && errno == EINTR) {
         continue;
      }
      if (received <= 0) {
         break;
      }
      bytes += (size_t)received;
      remaining -= (size_t)received;
   }
   close(descriptors[0]);

   int status = 0;
   if (waitpid(child, &status, 0) < 0) {
      perror("waitpid");
      exit(2);
   }
   if (remaining != 0) {
      return (counterexample_t){
          .found = true,
          .imbalance =
              {
                  .found = true,
                  .kind = "algorithm-exit",
                  .node_size =
                      WIFSIGNALED(status) ? (u64)WTERMSIG(status) : (u64)status,
              },
      };
   }
   return result;
}

static bool counterexample_is_better(counterexample_t candidate,
                                     counterexample_t current) {
   if (!candidate.found) {
      return false;
   }
   if (!current.found) {
      return true;
   }
   if (candidate.tree_size != current.tree_size) {
      return candidate.tree_size < current.tree_size;
   }
   if (candidate.operation_index != current.operation_index) {
      return candidate.operation_index < current.operation_index;
   }
   return candidate.trial < current.trial;
}

static void write_header(FILE *output) {
   fprintf(output,
           "delta,gamma,status,counterexample_size,operation,position,"
           "operation_index,trial,seed,kind,node_size,left_size,right_size,"
           "depth,tested_operations,max_size\n");
}

static void write_result(FILE *output, const explorer_config_t *config,
                         u64 delta, u64 gamma, counterexample_t counterexample) {
   double delta_value = (double)delta / (double)config->scale;
   double gamma_value = (double)gamma / (double)config->scale;
   if (!counterexample.found) {
      fprintf(output,
              "%.8g,%.8g,survived,0,,,,,0,,0,0,0,0,%" PRIu64 ",%" PRIu64
              "\n",
              delta_value, gamma_value,
              config->trials * config->operations, config->max_size);
      return;
   }

   const char *operation =
       counterexample.operation == NULL ? "unknown" : counterexample.operation;
   const char *kind = counterexample.imbalance.kind == NULL
                          ? "algorithm-exit"
                          : counterexample.imbalance.kind;
   fprintf(output,
           "%.8g,%.8g,failed,%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64
           ",%" PRIu64 ",%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64
           ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
           delta_value, gamma_value, counterexample.tree_size,
           operation, counterexample.position,
           counterexample.operation_index, counterexample.trial,
           counterexample.run_seed, kind,
           counterexample.imbalance.node_size,
           counterexample.imbalance.left_size,
           counterexample.imbalance.right_size, counterexample.imbalance.depth,
           config->trials * config->operations, config->max_size);
}

int main(int argc, char **argv) {
   explorer_config_t config = parse_config(argc, argv);
   FILE *output = fopen(config.output, "w");
   if (output == NULL) {
      perror(config.output);
      return 2;
   }

   write_header(output);
   for (u64 delta = config.delta_min;;) {
      for (u64 gamma = config.gamma_min;;) {
         active_pair = (parameter_pair_t){
             .delta_num = delta,
             .delta_den = config.scale,
             .gamma_num = gamma,
             .gamma_den = config.scale,
         };

         counterexample_t best = run_pair_isolated(&config, delta, gamma);
         write_result(output, &config, delta, gamma, best);
         if (config.gamma_max - gamma < config.gamma_step) {
            break;
         }
         gamma += config.gamma_step;
      }
      if (config.delta_max - delta < config.delta_step) {
         break;
      }
      delta += config.delta_step;
   }

   if (fclose(output) != 0) {
      perror(config.output);
      return 2;
   }
   return 0;
}
