#define FDS_IMPL
#include "fds.h"
#include "fds_hash.h"

int main(void) {
    SV input_str = sv_from_cstr("user_session_token_993182");

    // ------------------------------------------------------------------------
    // WyHash64: Максимальна швидкість (>10 Гб/с)
    // Ідеально для: внутрішніх хеш-таблиць, розпарсингу AST, комбінаторного пошуку
    // ------------------------------------------------------------------------
    uint64_t seed = 0x1337BEEF;
    uint64_t wy_hash = fds_hash_sv(input_str, seed);
    fds_log(FINFO, "[WyHash64] Хеш: 0x%016llX", (unsigned long long)wy_hash);

    // ------------------------------------------------------------------------
    // SipHash-2-4: Захищений від колізій та HashDoS атак (DoD рівень)
    // Ідеально для: обробки зовнішніх HTTP-запитів, JSON від невідомих джерел
    // ------------------------------------------------------------------------
    uint64_t key0 = 0x0123456789ABCDEFULL; // 128-бітний секретний ключ
    uint64_t key1 = 0xFEDCBA9876543210ULL;
    
    uint64_t sip_hash = fds_siphash64(input_str.data, input_str.count, key0, key1);
    fds_log(FINFO, "[SipHash-2-4] Хеш: 0x%016llX", (unsigned long long)sip_hash);

    // Властивість хешування однакових даних
    SV duplicate_str = sv_from_cstr("user_session_token_993182");
    FDS_ASSERT(fds_hash_sv(duplicate_str, seed) == wy_hash, "Хеші повинні збігатися!");

    return 0;
}