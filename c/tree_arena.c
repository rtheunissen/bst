#include "bst.h"

#include <unistd.h>

arena_t *global_arena = NULL;
static arena_t *live_arenas = NULL;

bool arena_is_live(arena_t *a) {
   invariant(a != NULL);
   for (arena_t *live = live_arenas; live != NULL; live = live->next_live) {
      if (live == a) {
         return true;
      }
   }
   return false;
}

static void arena_register(arena_t *a) {
   invariant(a != NULL);
   invariant(!arena_is_live(a));
   a->next_live = live_arenas;
   live_arenas = a;
}

static void arena_unregister(arena_t *a) {
   invariant(a != NULL);
   arena_t **link = &live_arenas;
   while (*link != NULL && *link != a) {
      link = &(*link)->next_live;
   }
   invariant(*link == a);
   *link = a->next_live;
   a->next_live = NULL;
}

bool arena_is_installed(arena_t *a) {
   invariant(a != NULL);
   for (arena_t *installed = global_arena; installed != NULL;
        installed = installed->previous_installed) {
      invariant(arena_is_live(installed));
      if (installed == a) {
         return true;
      }
   }
   return false;
}

static node_t *arena_alloc_buffer(u64 capacity) {
   invariant(capacity > 0);
   invariant(capacity <= arena_max_capacity());

   return (node_t *)bst_malloc_array(capacity, sizeof(node_t));
}

static u64 arena_next_chunk_capacity(const arena_t *a) {
   invariant(a != NULL);
   invariant(a->current != NULL);
   invariant(a->current->capacity > 0);
   invariant(a->current->capacity <= arena_max_capacity());

   u64 capacity = a->current->capacity;
   u64 max_capacity = arena_max_capacity();
   if (capacity < max_capacity) {
      u64 needed = size_after_inserting_one(capacity);
      capacity = bst_array_next_cap(capacity, needed, capacity, sizeof(node_t));
   }
   invariant(capacity > 0);
   invariant(capacity <= max_capacity);
   return capacity;
}

static void arena_require_zero_state(const arena_t *a) {
   invariant(a != NULL);
   invariant(a->first.buffer == NULL);
   invariant(a->first.capacity == 0);
   invariant(a->first.offset == 0);
   invariant(a->first.next == NULL);
   invariant(a->current == NULL);
   invariant(a->next_live == NULL);
   invariant(a->previous_installed == NULL);
}

void arena_init(arena_t *a, u64 capacity) {
   invariant(a != NULL);
   invariant(!arena_is_live(a));
   if (capacity == 0) {
      capacity = 1;
   }
   invariant(capacity <= arena_max_capacity());

   a->first.buffer = arena_alloc_buffer(capacity);
   a->first.capacity = capacity;
   a->first.offset = 0;
   a->first.next = NULL;
   a->current = &a->first;
   a->previous_installed = NULL;
   arena_register(a);
}

void arena_grow(arena_t *a) {
   invariant(a != NULL);
   invariant(arena_is_live(a));
   invariant(a->current != NULL);
   invariant(a->current->buffer != NULL);
   invariant(a->current->capacity > 0);
   invariant(a->current->offset == a->current->capacity);
   invariant(a->current->next == NULL);

   u64 cap = arena_next_chunk_capacity(a);
   arena_chunk_t *chunk = (arena_chunk_t *)bst_malloc(sizeof(arena_chunk_t));
   chunk->buffer = arena_alloc_buffer(cap);
   chunk->capacity = cap;
   chunk->offset = 0;
   chunk->next = NULL;
   a->current->next = chunk;
   a->current = chunk;
}

static size_t arena_prewarm_page_size(void) {
#ifdef _SC_PAGESIZE
   long page_size = sysconf(_SC_PAGESIZE);
   if (page_size > 0) {
      return (size_t)page_size;
   }
#endif
   return 4096;
}

static void arena_prewarm_buffer(node_t *buffer, u64 capacity) {
   invariant(buffer != NULL);
   invariant(capacity > 0);
   invariant(capacity <= arena_max_capacity());

   volatile unsigned char *bytes = (volatile unsigned char *)buffer;
   size_t byte_count = bst_array_bytes(capacity, sizeof(buffer[0]));
   size_t page_size = arena_prewarm_page_size();
   invariant(byte_count > 0);
   invariant(page_size > 0);

   for (size_t offset = 0; offset < byte_count; offset += page_size) {
      bytes[offset] = 0;
   }
   bytes[byte_count - 1] = 0;
}

void arena_prewarm(arena_t *a) {
   invariant(a != NULL);
   invariant(arena_is_live(a));
   invariant(a->first.buffer != NULL);
   invariant(a->first.capacity > 0);
   invariant(a->first.offset == 0);
   invariant(a->first.next == NULL);
   invariant(a->current == &a->first);

   arena_prewarm_buffer(a->first.buffer, a->first.capacity);
}

void arena_reset(arena_t *a) {
   invariant(a != NULL);
   invariant(arena_is_live(a));
   invariant(a->first.buffer != NULL);
   invariant(a->first.capacity > 0);
   invariant(a->first.offset <= a->first.capacity);
   invariant(a->current != NULL);

   arena_chunk_t *c = a->first.next;
   while (c) {
      invariant(c->buffer != NULL);
      invariant(c->capacity > 0);
      invariant(c->offset <= c->capacity);
      arena_chunk_t *next = c->next;
      free(c->buffer);
      free(c);
      c = next;
   }
   a->first.offset = 0;
   a->first.next = NULL;
   a->current = &a->first;
}

void arena_destroy(arena_t *a) {
   invariant(a != NULL);
   invariant(arena_is_live(a));
   invariant(!arena_is_installed(a));
   invariant(a->first.buffer != NULL);
   invariant(a->previous_installed == NULL);

   arena_reset(a);
   arena_unregister(a);
   free(a->first.buffer);
   a->first.buffer = NULL;
   a->first.capacity = 0;
   a->first.offset = 0;
   a->first.next = NULL;
   a->current = NULL;
   a->previous_installed = NULL;
}

bool arena_contains(arena_t *a, node_t *p) {
   if (a == NULL || p == NULL) {
      return false;
   }
   if (a->first.buffer == NULL) {
      arena_require_zero_state(a);
      return false;
   }

   invariant(a->current != NULL);
   uintptr_t addr = (uintptr_t)p;
   bool saw_current = false;
   for (arena_chunk_t *c = &a->first; c != NULL; c = c->next) {
      invariant(c->buffer != NULL);
      invariant(c->capacity > 0);
      invariant(c->offset <= c->capacity);
      if (c == a->current) {
         saw_current = true;
         invariant(c->next == NULL);
      }
      size_t bytes = bst_array_bytes(c->capacity, sizeof(c->buffer[0]));
      invariant(bytes <= UINTPTR_MAX);
      uintptr_t start = (uintptr_t)c->buffer;
      invariant(start <= UINTPTR_MAX - bytes);
      uintptr_t end = start + (uintptr_t)bytes;
      if (addr >= start && addr < end &&
          (addr - start) % sizeof(c->buffer[0]) == 0) {
         return true;
      }
   }
   invariant(saw_current);
   return false;
}

bool arena_contains_live(node_t *p) {
   for (arena_t *a = live_arenas; a != NULL; a = a->next_live) {
      if (arena_contains(a, p)) {
         return true;
      }
   }
   return false;
}
