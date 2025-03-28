#include "alloc.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ALIGNMENT 16 /**< The alignment of the memory blocks */

static free_block *HEAD = NULL; /**< Pointer to the first element of the free list */

/**
 * Split a free block into two blocks
 *
 * @param block The block to split
 * @param size The size of the first new split block
 * @return A pointer to the first block or NULL if the block cannot be split
 */
void *split(free_block *block, int size) {
    if((block->size < size + sizeof(free_block))) {
        return NULL;
    }

    void *split_pnt = (char *)block + size + sizeof(free_block);
    free_block *new_block = (free_block *) split_pnt;

    new_block->size = block->size - size - sizeof(free_block);
    new_block->next = block->next;

    block->size = size;

    return block;
}

/**
 * Find the previous neighbor of a block
 *
 * @param block The block to find the previous neighbor of
 * @return A pointer to the previous neighbor or NULL if there is none
 */
free_block *find_prev(free_block *block) {
    free_block *curr = HEAD;
    while(curr != NULL) {
        char *next = (char *)curr + curr->size + sizeof(free_block);
        if(next == (char *)block)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Find the next neighbor of a block
 *
 * @param block The block to find the next neighbor of
 * @return A pointer to the next neighbor or NULL if there is none
 */
free_block *find_next(free_block *block) {
    char *block_end = (char*)block + block->size + sizeof(free_block);
    free_block *curr = HEAD;

    while(curr != NULL) {
        if((char *)curr == block_end)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Remove a block from the free list
 *
 * @param block The block to remove
 */
void remove_free_block(free_block *block) {
    free_block *curr = HEAD;
    if(curr == block) {
        HEAD = block->next;
        return;
    }
    while(curr != NULL) {
        if(curr->next == block) {
            curr->next = block->next;
            return;
        }
        curr = curr->next;
    }
}

/**
 * Coalesce neighboring free blocks
 *
 * @param block The block to coalesce
 * @return A pointer to the first block of the coalesced blocks
 */
void *coalesce(free_block *block) {
    if (block == NULL) {
        return NULL;
    }

    free_block *prev = find_prev(block);
    free_block *next = find_next(block);

    // Coalesce with previous block if it is contiguous.
    if (prev != NULL) {
        char *end_of_prev = (char *)prev + prev->size + sizeof(free_block);
        if (end_of_prev == (char *)block) {
            prev->size += block->size + sizeof(free_block);

            // Ensure prev->next is updated to skip over 'block', only if 'block' is directly next to 'prev'.
            if (prev->next == block) {
                prev->next = block->next;
            }
            block = prev; // Update block to point to the new coalesced block.
        }
    }

    // Coalesce with next block if it is contiguous.
    if (next != NULL) {
        char *end_of_block = (char *)block + block->size + sizeof(free_block);
        if (end_of_block == (char *)next) {
            block->size += next->size + sizeof(free_block);

            // Ensure block->next is updated to skip over 'next'.
            block->next = next->next;
        }
    }

    return block;
}

/**
 * Call sbrk to get memory from the OS
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the allocated memory
 */

void *do_alloc(size_t size) {
    void *block = sbrk(size + sizeof(free_block));
    if (block == (void*) -1) { 
        return NULL;
    }

    free_block *new_block = (free_block *) block;
    new_block->size = size;
    new_block->next = NULL;
    return new_block;
}

//for extra cred: ptr to last allcated free blk
static free_block *next_fit_ptr = NULL; 
/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */



void *tumalloc(size_t size) {
    // Track and test extra cred Next fit print statements
    printf("Requesting allocation of size: %zu\n", size);
    printf("Next Fit pointer before allocation: %p\n", next_fit_ptr);

    // Align the size / rounding up to nearest block size
    size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1); 


    // next, start from next_fit_ptr (or the HEAD)
    free_block *current = next_fit_ptr ? next_fit_ptr : HEAD;  
    free_block *prev = NULL;

    // Traverse free list to find suitable block size
    while (current) {
        if (current->size >= size) {
            // If necessary, split block
            if (current->size > size + sizeof(free_block)) {
                split(current, size);
            }

            // Remove the block from the free list
            if (prev) {
                prev->next = current->next;
            } else {
                HEAD = current->next;
            }

            // Update the next_fit_ptr to the next free block
            next_fit_ptr = current->next ? current->next : HEAD;  

            printf("Allocated memory at: %p\n", (void *)(current + 1));
            return (void *)(current + 1);  
        }
        prev = current;
        current = current->next;
    }

    // If no suitable block, request new memory
    free_block *new_block = (free_block *)sbrk(size + sizeof(free_block));
    if (new_block == (void *)-1) {
        // sbrk fails, print:
        printf("Allocation failed: sbrk failed.\n");
        return NULL; 
    }

    new_block->size = size;
    new_block->next = NULL;

    // Update next_fit_ptr after sbrk allocation
    next_fit_ptr = NULL;  // Set to NULL; not needed atfer sbrk

    printf("Allocated new memory at: %p\n", (void *)(new_block + 1));
    return (void *)(new_block + 1);    
}
    

    

/**
 * Allocates and initializes a list of elements for the end user
 *
 * @param num How many elements to allocate
 * @param size The size of each element
 * @return A pointer to the requested block of initialized memory
 */
void *tucalloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = tumalloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);  // mem set to 0
    }
    return ptr;
}

/**
 * Reallocates a chunk of memory with a bigger size
 *
 * @param ptr A pointer to an already allocated piece of memory
 * @param new_size The new requested size to allocate
 * @return A new pointer containing the contents of ptr, but with the new_size
 */

void *turealloc(void *ptr, size_t new_size) {
    if (!ptr) return tumalloc(new_size);  // if null, return to malloc

    // snag block header
    free_block *block = (free_block *)ptr - 1;

    // If current block >, return ptr
    if (block->size >= new_size) return ptr;

    // allocate new block /copy  the data over
    void *new_ptr = tumalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);  // Cp data -> new blk
        tufree(ptr);  // Free prev block
    }
    return new_ptr;
}

/**
 * Removes used chunk of memory and returns it to the free list
 *
 * @param ptr Pointer to the allocated piece of memory
 */
void tufree(void *ptr) {
    // extra cred next fit test case 
    printf("Freeing block at: %p\n", ptr);

    if (!ptr) return;  // nah, do not free null ptr

    // Get the block header (before the memory block pointer)
    free_block *block = (free_block *)ptr - 1;

    // Add the block back to the free list
    block->next = HEAD;
    HEAD = block;

    printf("Free operation completed. Update the free_list:\n");  
}

