// #define DEBUG_MEM
#define FDS_IMPL

#include "fds.h"



// ============================================================================
//                      FdsBitset (Bit Array / Bitset)
// ============================================================================

typedef struct {
	u64 *words;
	usize word_count;
	usize bit_count;
} FdsBitset;

FdsBitset fds_bitset_create(usize bit_count);
void fds_bitset_free(FdsBitset *bs);
void fds_bitset_set(FdsBitset *bs, usize bit);
void fds_bitset_clear(FdsBitset *bs, usize bit);
bool fds_bitset_test(const FdsBitset *bs, usize bit);
void fds_bitset_fill(FdsBitset *bs, bool value);


// ============================================================================
//                  FdsBitset Implementation
// ============================================================================

FdsBitset fds_bitset_create(usize bit_count) {
    FdsBitset bs = zero;
    bs.bit_count = bit_count;
    bs.word_count = (bit_count + 63) / 64;
    bs.words = (u64*)FDS_MALLOC(bs.word_count * sizeof(u64));
    FDS_ASSERT(bs.words != NULL, "FdsBitset: allocation failed");
    fds_bitset_fill(&bs, false);
    return bs;
}

void fds_bitset_free(FdsBitset *bs) {
    if (!bs) return;
    if (bs->words) {
        FDS_FREE(bs->words);
        bs->words = NULL;
    }
    bs->word_count = 0;
    bs->bit_count = 0;
}

void fds_bitset_set(FdsBitset *bs, usize bit) {
    FDS_ASSERT(bs && bit < bs->bit_count, "FdsBitset: index out of bounds");
    bs->words[bit / 64] |= ((u64)1 << (bit % 64));
}

void fds_bitset_clear(FdsBitset *bs, usize bit) {
    FDS_ASSERT(bs && bit < bs->bit_count, "FdsBitset: index out of bounds");
    bs->words[bit / 64] &= ~((u64)1 << (bit % 64));
}

bool fds_bitset_test(const FdsBitset *bs, usize bit) {
    FDS_ASSERT(bs && bit < bs->bit_count, "FdsBitset: index out of bounds");
    return (bs->words[bit / 64] & ((u64)1 << (bit % 64))) != 0;
}

void fds_bitset_fill(FdsBitset *bs, bool value) {
    if (!bs || !bs->words) return;
    u64 fill_val = value ? 0xFFFFFFFFFFFFFFFFULL : 0ULL;
    for (usize i = 0; i < bs->word_count; i++) {
        bs->words[i] = fill_val;
    }
}
void fds_begin(void)
{}

void fds_end(void)
{
	if (temp_arena_initialized) temp_arena_destroy();
	#ifdef DEBUG_MEM
    	fds_allocator_print_stats(fds_allocator_current());
	#endif
    fds_allocator_destroy(fds_allocator_current());
}
int main(void)
{
	
	console.info("Hello world!");

	fds_end();
    return 0;
}