#define FDS_IMPL
#include "fds.h"
#include "fds_hash.h"

// Допоміжна функція виводу SHA-256 у форматуванні Hex
static void print_sha256(const uint8_t hash[32]) {
    char hex_str[65];
    for (int i = 0; i < 32; i++) {
        snprintf(hex_str + (i * 2), 3, "%02x", hash[i]);
    }
    hex_str[64] = '\0';
    fds_log(FINFO, "SHA-256: %s", hex_str);
}

int main(void) {
    // --- Сценарій 1: Одноразовий розрахунок для готового буфера ---
    const char *payload = "Hello, World! Security via FDS.";
    uint8_t single_out[32];
    fds_sha256(payload, strlen(payload), single_out);
    
    fds_log(FINFO, "Одноразовий хеш:");
    print_sha256(single_out);

    // --- Сценарій 2: Потокове обчислення (Stream/Chunk Processing) ---
    // Корисно при читанні великих файлів або мережевих пакетів по частинах
    FdsSHA256 sha_ctx;
    fds_sha256_init(&sha_ctx);

    const char *chunk1 = "Hello, ";
    const char *chunk2 = "World! ";
    const char *chunk3 = "Security via FDS.";

    fds_sha256_update(&sha_ctx, chunk1, strlen(chunk1));
    fds_sha256_update(&sha_ctx, chunk2, strlen(chunk2));
    fds_sha256_update(&sha_ctx, chunk3, strlen(chunk3));

    uint8_t stream_out[32];
    fds_sha256_final(&sha_ctx, stream_out);

    fds_log(FINFO, "Потоковий хеш (має збігатися):");
    print_sha256(stream_out);

    FDS_ASSERT(memcmp(single_out, stream_out, 32) == 0, "SHA256 хеші не збігаються!");
    return 0;
}