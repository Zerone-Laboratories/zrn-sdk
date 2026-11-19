/**
 * zrnui - id.c
 * The handle table: a Zid is a slot index and a generation, so a destroyed handle is detected, not dereferenced.
 */

#include "zrnui/internal/id.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================================
 *  handles
 * ============================================================================ */

static constexpr int ZRN_INDEX_BITS = 20;
#define ZRN_INDEX_MASK ((1u << ZRN_INDEX_BITS) - 1)

typedef struct zrn_slot {
	void* ptr;
	uint16_t generation;
	bool b_is_root;
} zrn_slot_t;

static zrn_slot_t* g_slots;
static uint32_t g_slot_count, g_slot_cap;
static uint32_t* g_free;
static uint32_t g_free_count, g_free_cap;

Zid zrn_handle_alloc(void* const ptr, const bool b_is_root) {
	uint32_t index;
	if (g_free_count) {
		index = g_free[--g_free_count];
	} else {
		if (g_slot_count == g_slot_cap) {
			const uint32_t cap = g_slot_cap ? g_slot_cap * 2 : 256;
			zrn_slot_t* slots = realloc(g_slots, cap * sizeof *slots);
			if (!slots) return Z_INVALID_ID;
			memset(slots + g_slot_cap, 0, (cap - g_slot_cap) * sizeof *slots);
			g_slots = slots;
			g_slot_cap = cap;
		}
		index = g_slot_count++;
	}
	g_slots[index].ptr = ptr;
	g_slots[index].b_is_root = b_is_root;
	if (g_slots[index].generation == 0) g_slots[index].generation = 1;
	return ((Zid)g_slots[index].generation << ZRN_INDEX_BITS) | (index + 1);
}

void zrn_handle_free(const Zid id) {
	const uint32_t index = (id & ZRN_INDEX_MASK) - 1;
	if ((id & ZRN_INDEX_MASK) == 0 || index >= g_slot_count) return;
	g_slots[index].ptr = nullptr;
	g_slots[index].generation = (uint16_t)((g_slots[index].generation + 1) & 0xFFF);
	if (g_slots[index].generation == 0) g_slots[index].generation = 1;
	if (g_free_count == g_free_cap) {
		const uint32_t cap = g_free_cap ? g_free_cap * 2 : 64;
		uint32_t* list = realloc(g_free, cap * sizeof *list);
		if (!list) return;
		g_free = list;
		g_free_cap = cap;
	}
	g_free[g_free_count++] = index;
}

void* zrn_handle_get(const Zid id, const bool b_is_root) {
	const uint32_t index = (id & ZRN_INDEX_MASK) - 1;
	if ((id & ZRN_INDEX_MASK) == 0 || index >= g_slot_count) return nullptr;
	if (g_slots[index].ptr == nullptr || g_slots[index].generation != (id >> ZRN_INDEX_BITS)) return nullptr;
	if (g_slots[index].b_is_root != b_is_root) return nullptr;
	return g_slots[index].ptr;
}

uint32_t zrn_handle_count(void) { return g_slot_count; }

void* zrn_handle_at(const uint32_t index, bool* const b_is_root) {
	if (index >= g_slot_count) return nullptr;
	*b_is_root = g_slots[index].b_is_root;
	return g_slots[index].ptr;
}

void zrn_handle_reset(void) {
	free(g_slots);
	free(g_free);
	g_slots = nullptr;
	g_free = nullptr;
	g_slot_count = g_slot_cap = g_free_count = g_free_cap = 0;
}
