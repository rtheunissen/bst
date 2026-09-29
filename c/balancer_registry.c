#include "balancer_registry.h"

#include "filesystem.h"

#include <stdlib.h>
#include <string.h>

const balancer_t *const balancer_registry_all[] = {
    &balancer_median, &balancer_height, &balancer_weight,
    &balancer_log,    &balancer_cost,   &balancer_dsw,
};

const int balancer_registry_count =
    sizeof(balancer_registry_all) / sizeof(balancer_registry_all[0]);

static bool balancer_registry_name_is_lookupable(const char *name) {
   return name != NULL && name[0] != '\0' && balancer_name_is_component(name);
}

static void balancer_registry_require_slice(const balancer_t *const *balancers,
                                            int count) {
   invariant(count >= 0);
   invariant(count == 0 || balancers != NULL);
   for (int i = 0; i < count; i++) {
      const balancer_t *balancer = balancers[i];
      balancer_require(balancer);
      for (int j = i + 1; j < count; j++) {
         const balancer_t *other = balancers[j];
         balancer_require(other);
         invariant(balancer != other);
         invariant(strcmp(balancer->name, other->name) != 0);
      }
   }
}

static void balancer_registry_require(void) {
   invariant(balancer_registry_count > 0);
   balancer_registry_require_slice(balancer_registry_all,
                                   balancer_registry_count);
}

static const balancer_t *balancer_registry_find_lookupable(const char *name) {
   invariant(balancer_registry_name_is_lookupable(name));
   balancer_registry_require();
   for (int i = 0; i < balancer_registry_count; i++) {
      const balancer_t *balancer = balancer_registry_all[i];
      balancer_require(balancer);
      if (strcmp(name, balancer->name) == 0) {
         return balancer;
      }
   }
   return NULL;
}

const balancer_t *balancer_registry_find(const char *name) {
   if (!balancer_registry_name_is_lookupable(name)) {
      return NULL;
   }
   return balancer_registry_find_lookupable(name);
}

const balancer_t *balancer_registry_find_or_exit(const char *name) {
   if (name == NULL) {
      fprintf(stderr, "balancer name is null\n");
      balancer_registry_fprint_names(stderr, "balancers");
      exit(2);
   }
   if (name[0] == '\0') {
      fprintf(stderr, "balancer name is empty\n");
      balancer_registry_fprint_names(stderr, "balancers");
      exit(2);
   }
   if (!balancer_name_is_component(name)) {
      fprintf(stderr, "balancer name is not a path component\n");
      balancer_registry_fprint_names(stderr, "balancers");
      exit(2);
   }

   const balancer_t *balancer = balancer_registry_find_lookupable(name);
   if (balancer == NULL) {
      fprintf(stderr, "unknown balancer: %s\n", name);
      balancer_registry_fprint_names(stderr, "balancers");
      exit(2);
   }
   return balancer;
}

const balancer_t *balancer_registry_find_optional_or_exit(const char *name) {
   if (name == NULL) {
      return NULL;
   }
   return balancer_registry_find_or_exit(name);
}

void balancer_registry_fprint_names(FILE *file, const char *label) {
   invariant(file != NULL);
   invariant(label != NULL);
   balancer_registry_require();

   fprintf(file, "%s:", label);
   for (int i = 0; i < balancer_registry_count; i++) {
      const balancer_t *balancer = balancer_registry_all[i];
      balancer_require(balancer);
      fprintf(file, " %s", balancer->name);
   }
   fprintf(file, "\n");
}
