
# Allocator Final Project

For my final project for CS377, I extended the memory allocator we implemented in project 5 that includes implementations of `my_malloc` and `my_free`. The changes I made include,

1. Improving `coalesce` so that all adjacent free blocks of memory will always be merged. The coalescing algorithm we implemented in project 5 will often fail to coalesce adjacent free blocks if they are not freed in the right order.
2. Allowing new pages to be allocated when there is insufficient free memory remaining. The allocator implemented in project 5 allocated only a single page of 4096 bytes. It was unable to allocate more memory, and if all 4096 bytes were used up, or a `malloc` call was made for more than 4096 bytes, it would fail.
3. Making the allocator thread safe. The allocator implemented in project 5 was not thread safe. Calling `malloc` or `free` in a multithreaded application would almost always lead to a segmentation fault.
4. Changing `find_free` to use a best fit algorithm to choose a free_block, which will generally lead to less memory fragmentation.

## Improving coalesce

The allocator from project 5 would coalesce free blocks by checking if the next free block in the list was adjacent to the current one, and merge them if so. This did not always work because free blocks were inserted into the free list in an arbitrary order when blocks were freed. This meant that there would be cases where all of the memory was free, but the memory was still fragmented into many small free blocks. As an example, a simple program that calls `malloc(100)` 10 times, and then frees them in the order they were allocated leads to the following output.

![Image1.png](images/Image1.png)

All of the 100 byte blocks were freed, but none of them were coalesced because they were not freed in the right order. As a result, we would not be able to `malloc` 3000 bytes even though we have far more than that available. Furthermore, we have lost total available memory because of the extra memory overhead in the headers of the free blocks.

I improved this by changing the `free` method. Instead of adding new free blocks to the start of the free list, I inserted it based on its address, so that the node before it had a lower address and the node after it had a larger address. I wrote a new method, `insert_free_block` to accomplish this. Inserting blocks this way means that the blocks are always in the free list in the same order they are in physically in memory. Therefore, the same coalescing algorithm of checking if the next block in the free list is adjacent to the current will work accurately to free adjacent blocks. I also called coalesce on the previous block in the free list to make sure that an adjacent block before the newly freed block would also be merged. This leads to the following output from the same program.

![Image2.png](images/Image2.png)

## Allocating new pages

I extended the allocator the able to allocate new pages of memory using `mmap` when more is requested. I implemented a new method `map_new_pages` that takes one parameter: size. Then, this method will allocate enough pages using `mmap` such that it can contain a new allocated block of that size. Then, when the `find_free` method returns `NULL` the allocator will call `map_new_pages` before trying `find_free` again, ensuring that there will be a suitable free block to allocate the requested memory in.

I created a new header type `map_t` to keep track of the allocated pages and their sizes so that they could be eventually unmapped with `munmap`, for example in `reset_heap`. 
```
typedef struct __map_t {
  size_t size;
  struct __map_t *next;
} map_t;
```
This type has a field for the size of mapped pages, as well as the next `map_t` so that they can be stored in a linked list. Then, I placed the `map_t` at the pointer returned from `mmap` so that it would act as a header to the mapped pages. Then, given a `map_t` object, it could be unmapped by the following,
```
munmap(map_t, map_t->size);
```
Then, I created a new free block with a header right after the `map_t` variable and size equal to the remaining newly mapped memory. The end result looks like,
```
map_t *ptr = (map_t *) mmap(NULL, HEAP_SIZE * pages_to_allocate, 
                                PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
node_t *free_block = (node_t *) (ptr + 1);
free_block->size = (HEAP_SIZE * pages_to_allocate) - sizeof(node_t) - sizeof(map_t);
insert_free_block(free_block);
```

## Making the allocator thread safe

The list of mapped pages and the list of free blocks are both shared variables between threads. This means that they have to be protected with locks in a multithreaded program. Implementing a simple multithreaded program that calls `my_malloc` and `my_free` from project 5 consistently causes segmentation faults, as these variables are not protected at all and the linked list structures are broken by context switches at inopportune moments.

I created two new mutexes, one for the list of mapped pages, and one for the list of free blocks to fix this problem. The mutex for the list of mapped pages, map\_lock, is used in critical sections of `heap` and `map_new_pages` where new `map_t` variables are added to the linked list. 

The other mutex, free\_lock, is for the list of free blocks. This mutex is used to cover any critical section where the free list is manipulated including: adding a new free block to the list, searching the free list, and removing free blocks from the list. This mutex is locked during `find_free` as the free list is traversed to find a suitable free block, and then it is kept locked until halfway through `split` where its size is adjusted and it is reinserted into the free list. It must be locked the entire way so that other threads cannot also find the same free block and then attempt to allocate it as well. The search must also be locked so that it is not interupted by another thread attempting to add a new free block into the free list. When freeing blocks, the free\_lock is held for the entirety of `insert_free_block` and `coalesce` as these methods almost exclusively manipulate the free list. 

Before adding these changes, a simple program that makes 5 threads that call `my_malloc` and `my_free` a few times each will almost certainly cause a segmentation fault. After protecting these critical sections, the same program runs completely fine.

I also added one more mutex, the new\_page\_lock. This was to cover an edge case where the allocator ran out of memory and then many context switches caused many individual threads to all call `map_new_pages` even though each only needed a small fraction of the new page that was being allocated. This lock is held before calling `map_new_pages` so that only one thread will actually map a new page, and then the following threads will attempt to call `find_free` again, only calling `map_new_pages` if there is still not enough memory.

## Best fit allocation

I changed the `find_free` method to use the best fit method of find free blocks. The allocator implemented in project 5 used the first fit approach, which finds the first free block that can fit the requested size, regardless of that free blocks size. The best fit approach will instead search every free block and find the one that most closely matches the requested size. This approach has more overhead, since every free block will have to be checked even if the first one would have worked, but generally can lead to less fragmentation of memory, meaning that the allocated memory will be utilized more efficiently.

I implemented this method by creating a variable, found\_size, at the start of the `find_free` method and setting it to `INT_MAX`. Then, when a suitable free block was found, it was only used if its size was less than found\_size. Then, instead of returning immediately, I continued searching the free list and only returned once no more free blocks were available. This meant that the free block with the smallest size that worked was selected, which is a best fit.
