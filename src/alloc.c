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
    if (block == (void*) -1) { // sbrk failed
        return NULL;
    }

    free_block *new_block = (free_block *) block;
    new_block->size = size;
    new_block->next = NULL;
    return new_block;

}

/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */
void *tumalloc(size_t size) {
    // If HEAD is NULL (initialization or no free blocks)
    if (HEAD == NULL) {
        void *ptr = do_alloc(size); // Perform allocation
        return ptr;
    }

    // Iterate through the free list
    free_block *block = HEAD;
    while (block != NULL) {
        // If a block is big enough
        if (size <= block->size) {
            free_block *header = split(block, size + sizeof(free_block)); // Split block if necessary
            remove_free_block(header);  // Remove block from free list
            header->size = size;       // Set size of the allocated block
            header->magic = 0x01234567; // Set magic value to indicate allocated block
            return (void *)((char *)header + 1); // Return pointer to the allocated memory
        }
        block = block->next;
    }

    // If no block is large enough, allocate new memory
    void *ptr = do_alloc(size); 
    return ptr;
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
        memset(ptr, 0, total_size);
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
    if (!ptr) return tumalloc(new_size);  // Allocate new memory if ptr is NULL
    if (new_size == 0) {
        tufree(ptr);  
        return NULL;
    }

    free_block *block = (free_block *)ptr - 1; // Get the block header
    if (!block) {
        printf("Invalid block.\n");
        return NULL;
    }

    // Check if the new size is smaller or equal to the current size
    if (block->size >= new_size) {
        return ptr;  // No need to reallocate
    }

    void *new_ptr = tumalloc(new_size);
    if (new_ptr) {
        // Check that the old block size is not smaller than the new size before copying
        size_t copy_size = block->size < new_size ? block->size : new_size;
        memcpy(new_ptr, ptr, copy_size);  // Safely copy the memory
        tufree(ptr);  // Free the old block
    }
    return new_ptr;
}

/**
 * Removes used chunk of memory and returns it to the free list
 *
 * @param ptr Pointer to the allocated piece of memory
 */
void tufree(void *ptr) {
    // Retrieve the header (block metadata) from the pointer
    free_block *header = (free_block *)ptr - 1;

    // Check if the magic number is correct (indicating valid allocation)
    if (header->magic == 0x01234567) {
        free_block *free_block = (free_block *)header;
        free_block->size = header->size;
        free_block->next = HEAD;
        HEAD = free_block;  // Add the block to the free list

        coalesce(free_block);  // Attempt to coalesce neighboring blocks
    } else {
        printf("MEMORY CORRUPTION DETECTED\n");
        abort(); // Abort if memory corruption is detected
    }
}
