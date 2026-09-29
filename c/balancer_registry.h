#ifndef BALANCER_REGISTRY_H
#define BALANCER_REGISTRY_H

#include "balancers.h"

#include <stdio.h>

extern const balancer_t *const balancer_registry_all[];
extern const int balancer_registry_count;

const balancer_t *balancer_registry_find(const char *name);
const balancer_t *balancer_registry_find_or_exit(const char *name);
const balancer_t *balancer_registry_find_optional_or_exit(const char *name);
void balancer_registry_fprint_names(FILE *file, const char *label);

#endif // BALANCER_REGISTRY_H
