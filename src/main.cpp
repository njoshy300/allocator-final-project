#include <my_malloc.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

void *worker(void *arg) {
  int id = *((int *) arg);

  for (int i = 0; i < 100; i++) {
    int *arr = (int *) my_malloc(150 * sizeof(int));
    printf("Thread %d allocated 600 bytes. Available memory after: %zd.\n", id, available_memory());

    for (int j = 0; j < 150; j++) {
      arr[j] = j * j;
    }

    my_free(arr);
    printf("Thread %d freed 600 bytes. Available memory after: %zd.\n", id, available_memory());
  }

  return NULL;
}

int main() {
  int num_threads = 20;
  int threadIDs[num_threads];
  pthread_t threads[num_threads];
  int rc;
  for (int i = 0; i < num_threads; i++) {
    threadIDs[i] = i;
		rc = pthread_create(&threads[i], NULL, worker, &threadIDs[i]);
    assert(rc == 0);
	}

  for (int i = 0; i < num_threads; i++) {
		pthread_join(threads[i], NULL);
	}

  print_free_list();
  printf("Available memory after: %zd.\n", available_memory());

  reset_heap();
  exit(1);

  void *allocated[100];

  // char *h = (char *)mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
  //                         MAP_ANON | MAP_PRIVATE, -1, 0);

  // printf("%p first %p\n", h + HEAP_SIZE, h);
  
  // char *h2 = (char *) mmap(h + HEAP_SIZE, HEAP_SIZE, PROT_READ | PROT_WRITE,
  //                         MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);

  //printf("first page %p, second page %p", h, h2);

  // printf("Available memory before: %zd.\n", available_memory());
  // test = my_malloc(4098);
  // print_free_list();
  // printf("Available memory after: %zd.\n", available_memory());

  // printf("Available memory before: %zd.\n", available_memory());
  // my_free(test);
  // print_free_list();
  // printf("Available memory after: %zd.\n", available_memory());

  reset_heap();

  printf("Available memory before: %zd.\n", available_memory());
  for (int i = 0; i < 10; i++) {
    allocated[i] = my_malloc(100);
  }
  print_free_list();
  printf("Available memory after: %zd.\n", available_memory());

  printf("Available memory before: %zd.\n", available_memory());
  for (int i = 0; i < 10; i++) {
    my_free(allocated[i]);
  }
  print_free_list();
  printf("Available memory after: %zd.\n", available_memory());

  // reset_heap();

  // printf("Available memory before: %zd.\n", available_memory());
  // for (int i = 0; i < 10; i++) {
  //   allocated[i] = my_malloc(100);
  // }
  // print_free_list();
  // printf("Available memory after: %zd.\n", available_memory());

  // printf("Available memory before: %zd.\n", available_memory());
  // for (int i = 9; i >= 0; i--) {
  //   my_free(allocated[i]);
  // }
  // print_free_list();
  // printf("Available memory after: %zd.\n", available_memory());

  // return 0;
}