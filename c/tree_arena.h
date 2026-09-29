#ifndef TREE_ARENA_H
#define TREE_ARENA_H

// Included by bst.h after node_t is defined.

typedef struct arena_chunk {
   node_t *buffer;
   u64 capacity;
   u64 offset;
   struct arena_chunk *next; // linked list for overflow chunks
} arena_chunk_t;

typedef struct arena {
   arena_chunk_t first;     // initial chunk (inline)
   arena_chunk_t *current;  // current chunk being allocated from
   struct arena *next_live; // intrusive list of initialized arenas
   struct arena *previous_installed;
} arena_t;

// Global arena (NULL = use malloc instead)
extern arena_t *global_arena;

static inline u64 arena_max_capacity(void) {
   return bst_array_max_count(sizeof(node_t));
}

void arena_init(arena_t *a, u64 capacity);
void arena_prewarm(arena_t *a);
void arena_reset(arena_t *a);
void arena_destroy(arena_t *a);

void arena_grow(arena_t *a);
bool arena_is_live(arena_t *a);
bool arena_is_installed(arena_t *a);
// Ownership predicate for every reserved slot in live chunks, allocated or not.
bool arena_contains(arena_t *a, node_t *p);
bool arena_contains_live(node_t *p);

static inline arena_t *arena_install(arena_t *a) {
   invariant(a != NULL);
   invariant(arena_is_live(a));
   invariant(a->first.buffer != NULL);
   invariant(a->current != NULL);
   invariant(global_arena != a);
   invariant(!arena_is_installed(a));
   invariant(a->previous_installed == NULL);
   arena_t *previous = global_arena;
   a->previous_installed = previous;
   global_arena = a;
   return previous;
}

static inline void arena_restore(arena_t *previous) {
   invariant(previous == NULL || arena_is_live(previous));
   invariant(global_arena != NULL);
   invariant(arena_is_live(global_arena));
   invariant(global_arena->previous_installed == previous);
   global_arena->previous_installed = NULL;
   global_arena = previous;
}

// Allocates memory for a new node and copies the given node into that memory.
static inline node_t *tree_allocate(node_t value) BST_RETURNS_NONNULL;
static inline node_t *tree_allocate(node_t value) {
#ifdef PERSISTENT
   invariant(value.refs == 0);
#endif
   measurement_record_allocation();
   node_t *p;
   if (global_arena) {
#ifdef DEBUG
      invariant(arena_is_live(global_arena));
#endif
      arena_chunk_t *c = global_arena->current;
      invariant(c != NULL);
      invariant(c->buffer != NULL);
      invariant(c->capacity > 0);
      invariant(c->offset <= c->capacity);
      if (c->offset == c->capacity) {
         arena_grow(global_arena);
         c = global_arena->current;
         invariant(c != NULL);
         invariant(c->buffer != NULL);
         invariant(c->capacity > 0);
         invariant(c->offset < c->capacity);
      }
      u64 offset = c->offset;
      invariant(offset < c->capacity);
      p = &c->buffer[offset];
      c->offset = size_after_inserting_one(offset);
   } else {
      p = (node_t *)bst_malloc(sizeof(node_t));
   }
   *p = value;
   return p;
}

#endif // TREE_ARENA_H
