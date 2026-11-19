/**
 * zrnui - id.h
 * The handle table of id.c: it holds elements and roots.
 */

#pragma once

#include "zrnui/id.h"

/**
 * Stores `ptr` in a free slot and returns its handle, (generation << 20) | (index + 1); Z_INVALID_ID on no memory. The slot count is not
 * limited, but the index has 20 bits.
 */
Zid zrn_handle_alloc(void* ptr, bool b_is_root);
/** Frees the slot and bumps its generation, so the old handle never resolves again. An invalid id is ignored. */
void zrn_handle_free(Zid id);
/**
 * The pointer of a live handle; NULL when the handle is invalid or stale, or of the other kind (`b_is_root` says whether a root or an element
 * is asked for).
 */
void* zrn_handle_get(Zid id, bool b_is_root);
/** The slots in index order, for the shutdown: the pointer of slot `index` (NULL when it is free) and whether it is a root. */
uint32_t zrn_handle_count(void);
void* zrn_handle_at(uint32_t index, bool* b_is_root);
/** Frees the table: every handle becomes invalid. For Zshutdown. */
void zrn_handle_reset(void);
