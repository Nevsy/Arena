//
// Created by Nev on 7/15/25.
//
// ARENA implementation for POSIX compliant systems
// Implementation of the Arena API by Ryan Fleury
// https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
// (...Zero functions were adapted to ...ZeroInit, for clarity)
// (added PopArray and PopStruct for convenience)
//
// Usage notes:
// - uses the same conventions as the malloc API, aka the functions return a NULL pointer when they fail
// - NO THREAD-SAFETY is implemented, neither in the main arena API, neither in the scratch arena API
//

#ifndef ARENA_ARENA_H
#define ARENA_ARENA_H

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
// #include <windows.h> // TODO: windows stuff

#ifdef ARENA_DEBUG
	#include <stdio.h>
	#define debug_print(...) printf(__VA_ARGS__)
#else
	#define debug_print(...) ((void)0)
#endif

// Base struct for an Arena
typedef struct Arena {
	void *base; // points to the beginning of the Arena in virtual memory
	uint64_t size; // total mmap'ed virtual memory, aka total available memory

	// relative to *base pointer
	// base + pos, points to the beginning of the memory segment we have not yet ascribed to the user
	uint64_t pos;
} Arena;

// create or destroy a 'stack' - an "arena"
// Memory Optimization Folks: careful! The total allocated space is rounded to page size
static Arena * ArenaAlloc(const uint64_t size) {
	if (!size || size > SIZE_MAX - sizeof(Arena)) return NULL;

	// Get system page size
	const long page_size = sysconf(_SC_PAGESIZE);
	if (page_size == -1) return NULL;

	// Align total size to page-boundary
	const uint64_t total_size = ((size + sizeof(Arena) + page_size - 1)
								& ~(page_size - 1));

	// mmap the virtual memory
	void *mem = mmap(NULL, total_size,
					PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS,
					-1, 0);
	if (mem == MAP_FAILED) {
		debug_print("[ARENA] OS OOM: %llu bytes requested\n", size);
		return NULL;
	}

	Arena *arena = (Arena *)mem;
	arena->base = (char *)mem + sizeof(Arena);
	arena->size = total_size - sizeof(Arena);  // Note: actual usable size might be larger than requested
	arena->pos = 0;

	debug_print("[ARENA] mmap'ed %llu bytes\n", size);

	return arena;
}


// unmaps the virtual memory, initially allocated in ArenaAlloc()
static int ArenaRelease(const Arena *arena){
	if (!arena) return -1;

	// unmap data + pointer, see ArenaAlloc
	const int ret =
		munmap((arena->base - sizeof(Arena)), arena->size + sizeof(Arena));
	return (ret);
}

// push some bytes onto the 'stack' - the way to allocate
// works in a malloc style i.e., returns NULL on failure
static void *ArenaPush(Arena *arena, const uint64_t size) {
	// Check for NULL and zero sizes
	if (!arena || !size) return NULL;

	const uint64_t current = arena->pos;

	// check for mmap overflow
	if (arena->pos + size > arena->size) {
		debug_print("[ARENA] Arena OOM: %llu bytes requested\n", size);
		return NULL;
	}
	arena->pos += size;

	return arena->base + arena->pos;
}



// same as ArenaPush, but memory is init'ed with 0
// can come in handy when freeing or clearing memory in the Arena
// (which doesn't memset(...0...) the memory)
static void *ArenaPushZeroInit(Arena *arena, const uint64_t size) {

	void *ptr = ArenaPush(arena, size);
	if (ptr == 0) {
		return (NULL);
	}

	// pos is alr incremented in ArenaPush()

	// 'zero initialization' of the acquired memory
	// sets 'size' bytes from 'ptr' on to value 0
	memset(ptr, 0, size);
	return (ptr);
}

// pop some bytes off the 'stack' - the way to free
static int ArenaPop(Arena *arena, const uint64_t size) {
	// if the user tries to pop more than even pushed in the first place
	if (size > arena->pos) {
		debug_print("[ARENA] over free: %llu bytes requested\n", size);
		return (-1);
	}

	arena->pos -= size;
	return (0);
}

// *** *** ***
// EXTRA: EASE OF LIFE
// *** *** ***

// sets the arena block pointer 'pos' to specified pos
static int ArenaSetPosBack(Arena *arena, const uint64_t pos) {
	// if the user tries to advance the pos, instead of freeing i.e., rolling back the pointer
	if (pos > arena->pos) {
		debug_print("[ARENA] backward free: requested to set allocate, instead of setting pointer back\n", size);
		return (-1);
	}
	arena->pos = pos;
	return (0);
}

// frees the whole arena, without memsetting it!
static void ArenaClear(Arena *arena) {
	arena->pos = 0;
}

// get the # of bytes currently allocated.
static uint64_t ArenaGetPos(const Arena *arena) {
	return (arena->pos);
}
// get the # of bytes initially mmap'ed
static uint64_t ArenaGetSize(const Arena *arena) {
	return (arena->size);
}
// get the arena's base pointer
static const void *ArenaGetBase(const Arena *arena) {
	return (arena->base);
}

// some macro helpers that I've found nice:

// checks if sizeof(type) > 0 and if we don't go over SIZE_MAX with total array size
#define ARENA_CHECK_OVERFLOW(a, b) ((a) > 0 && (b) > SIZE_MAX / (a))
// push an array to the top of the 'stack' - arena
#define PushArray(arena, type, count) \
	(ARENA_CHECK_OVERFLOW(sizeof(type), count) ? NULL : \
	(type *)ArenaPush((arena), sizeof(type)*(count)))

// push an array to the top of the 'stack' - arena
// memory is memset'ed to 0 before returning
#define PushArrayZeroInit(arena, type, count) \
	(ARENA_CHECK_OVERFLOW(sizeof(type), count) ? NULL : \
	(type *)ArenaPushZeroInit((arena), sizeof(type)*(count)))

// push a struct to the top of the 'stack' - arena
#define PushStruct(arena, type) PushArray((arena), (type), 1)

// push a struct to the top of the 'stack' - arena
// memory is memset'ed to 0 before returning
#define PushStructZeroInit(arena, type) PushArrayZeroInit((arena), (type), 1)

// pop an array off of the 'stack' - arena
#define PopArray(arena, type, count) ArenaPop((arena), sizeof(type)*(count))
// pop a struct off of the 'stack' - arena
#define PopStruct(arena, type) PopArray((arena), (type), 1)


// *** *** ***
// Scratch Arrays (NON-THREAD SAFE!)
// *** *** ***
typedef struct ArenaScratch {
	Arena *arena;
	uint64_t pos;
} ArenaTemp;

static ArenaTemp ArenaScratchBegin(Arena *arena) {
	return (ArenaTemp){arena, arena->pos};
}

static void ArenaScratchEnd(const ArenaTemp temp) {
	if (temp.arena) temp.arena->pos = temp.pos;
}


#endif //ARENA_ARENA_H