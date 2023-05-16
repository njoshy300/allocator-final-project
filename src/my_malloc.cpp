#include <assert.h>
#include <my_malloc.h>
#include <stdio.h>
#include <sys/mman.h>
#include <cmath>
#include <pthread.h>
#include <limits.h>

// Mutexes
pthread_mutex_t map_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t free_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t new_page_lock = PTHREAD_MUTEX_INITIALIZER;

// A pointer to the head of the free list.
map_t *start = NULL;
node_t *head = NULL;

// Assumes that the free_lock is already held.
node_t *heap() {
  pthread_mutex_lock(&map_lock);
  if (start == NULL) {
    start = (map_t *) mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    start->size = HEAP_SIZE;
    start->next = NULL;

    pthread_mutex_unlock(&map_lock);

    head = (node_t *) (start + 1);
    head->size = HEAP_SIZE - sizeof(node_t) - sizeof(map_t);
    head->next = NULL;

    return head;
  }
  pthread_mutex_unlock(&map_lock);

  return head;
}

node_t *insert_free_block(node_t *free_block) {
  if (free_block < head) {
    free_block->next = head;
    head = free_block;
    return head;
  }

  node_t *cur = head;
  while (cur->next != NULL) {
    if (cur < free_block && free_block < cur->next) {
      free_block->next = cur->next;
      cur->next = free_block;
      return cur;
    }

    cur = cur->next;
  }

  free_block->next = NULL;
  cur->next = free_block;
  return cur;
}

int map_new_pages(size_t size) {
  int pages_to_allocate = (int) ceil((size + sizeof(map_t) + sizeof(header_t)) / 4096.0);
  map_t *ptr = (map_t *) mmap(NULL, HEAP_SIZE * pages_to_allocate, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

  if (ptr == MAP_FAILED) {
    return -1;
  }

  ptr->size = pages_to_allocate * HEAP_SIZE;

  pthread_mutex_lock(&map_lock);
  ptr->next = start;
  start = ptr;
  pthread_mutex_unlock(&map_lock);

  node_t *free_block = (node_t *) (ptr + 1);
  free_block->size = (HEAP_SIZE * pages_to_allocate) - sizeof(node_t) - sizeof(map_t);

  pthread_mutex_lock(&free_lock);
  insert_free_block(free_block);
  pthread_mutex_unlock(&free_lock);

  return 0;
}

// Reallocates the heap.
void reset_heap() {
  pthread_mutex_lock(&free_lock);
  pthread_mutex_lock(&map_lock);

  map_t *next;
  while (start != NULL) {
    next = start->next;
    munmap(start, start->size);
    start = next;
  }

  start = NULL;
  head = NULL;

  pthread_mutex_unlock(&free_lock);
  pthread_mutex_unlock(&map_lock);

  heap();
}

// Returns a pointer to the head of the free list.
node_t *free_list() { return head; }

// Calculates the amount of free memory available in the heap.
size_t available_memory() {
  size_t n = 0;
  pthread_mutex_lock(&free_lock);
  node_t *p = heap();
  while (p != NULL) {
    n += p->size;
    p = p->next;
  }
  pthread_mutex_unlock(&free_lock);
  return n;
}

// Returns the number of nodes on the free list.
int number_of_free_nodes() {
  int count = 0;
  pthread_mutex_lock(&free_lock);
  node_t *p = heap();
  while (p != NULL) {
    count++;
    p = p->next;
  }
  pthread_mutex_unlock(&free_lock);
  return count;
}

// Prints the free list. Useful for debugging purposes.
void print_free_list() {
  pthread_mutex_lock(&free_lock);
  node_t *p = heap();
  while (p != NULL) {
    printf("Free(%zd)", p->size);
    p = p->next;
    if (p != NULL) {
      printf("->");
    }
  }
  printf("\n");
  pthread_mutex_unlock(&free_lock);
}

bool fits_in_block(node_t *block, size_t size) {
  return block->size > (size + sizeof(header_t));
}

void find_free(size_t size, node_t **found, node_t **previous) {
  int found_size = INT_MAX;
  pthread_mutex_lock(&free_lock);
  node_t *prev = heap();

  if (prev == NULL) {
    *found = NULL;
    return;
  }

  if (fits_in_block(prev, size)) {
    *found = prev;
    *previous = NULL;
    found_size = prev->size;
  }

  node_t *cur = prev->next;
  while (cur != NULL) {
    if (fits_in_block(cur, size) && (int) cur->size < found_size) {
      found_size = cur->size;
      *found = cur;
      *previous = prev;
    }

    prev = cur;
    cur = cur->next;
  }
  
  if (found_size < INT_MAX) {
    return;
  }
  
  pthread_mutex_unlock(&free_lock);
  *found = NULL;
}

void split(size_t size, node_t **previous, node_t **free_block, header_t **allocated) {
  node_t *orig = *free_block;
  
  size_t actual_size = size + sizeof(header_t);
  *free_block = (node_t *)(((char *)*free_block) + actual_size);
  (*free_block)->size = orig->size - actual_size;
  (*free_block)->next = orig->next;

  if (*previous == NULL) {
    head = *free_block;
  } else {
    (*previous)->next = *free_block;
  }
  pthread_mutex_unlock(&free_lock);

  *allocated = (header_t *) orig;
  (*allocated)->size = size;
  (*allocated)->magic = MAGIC;
}

void *my_malloc(size_t size) {
  node_t *found;
  node_t *previous;
  find_free(size, &found, &previous);

  if (found == NULL) {
    pthread_mutex_lock(&new_page_lock);
    find_free(size, &found, &previous);

    if (found == NULL) {
      int err = map_new_pages(size);

      if (err == -1) {
        pthread_mutex_unlock(&new_page_lock);
        return NULL;
      }

      find_free(size, &found, &previous);
    }
    
    pthread_mutex_unlock(&new_page_lock);
  }

  header_t *allocated;
  split(size, &previous, &found, &allocated);

  return (void *) (allocated + 1);
}

// Free
bool next_is_adjacent(node_t *block) {
  if (block->next == NULL) {
    return false;
  }

  size_t block_size = block->size + sizeof(node_t);
  return ((char *) block) + block_size == (char *) block->next;
}

int coalesce(node_t *free_block) {
  int c = 0;
  while (next_is_adjacent(free_block)) {
    free_block->size += free_block->next->size + sizeof(node_t);
    free_block->next = free_block->next->next;
    c++;
  }

  return c;
}

void my_free(void *allocated) {
  header_t *block = ((header_t *) allocated) - 1;
  size_t size = block->size;
  
  if (block->magic != MAGIC) {
    return;
  }

  node_t *free_block = (node_t *) block;
  free_block->size = size + sizeof(header_t) - sizeof(node_t);

  pthread_mutex_lock(&free_lock);
  node_t *prev = insert_free_block(free_block);
  if (coalesce(prev) == 0) {
    coalesce(free_block);
  }
  pthread_mutex_unlock(&free_lock);
}