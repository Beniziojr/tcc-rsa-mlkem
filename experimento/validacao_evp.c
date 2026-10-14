#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

static void fail(const char *step) {
    unsigned long code;
    char message[256];
    fprintf(stderr, "Falha: %s\n", step);
    while ((code = ERR_get_error()) != 0) {
        ERR_error_string_n(code, message, sizeof(message));
        fprintf(stderr, "%s\n", message);
    }
    exit(EXIT_FAILURE);
}
#define CHECK(condition, step) do { if (!(condition)) fail(step); } while (0)

static EVP_PKEY *keygen(const char *name) {
    EVP_PKEY *key = NULL;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, name, NULL);
    CHECK(ctx != NULL, "criar contexto de geracao");
    CHECK(EVP_PKEY_keygen_init(ctx) > 0, "inicializar geracao");
    if (strcmp(name, "RSA") == 0)
        CHECK(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 3072) > 0, "modulo RSA-3072");
    CHECK(EVP_PKEY_generate(ctx, &key) > 0, "gerar chaves");
    EVP_PKEY_CTX_free(ctx);
    return key;
}

static void oaep_config(EVP_PKEY_CTX *ctx) {
    CHECK(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) > 0, "OAEP");
    CHECK(EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) > 0, "hash OAEP SHA-256");
    CHECK(EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) > 0, "hash MGF1 SHA-256");
    /* O rotulo OAEP permanece vazio, conforme a configuracao padrao. */
}

static void rsa_roundtrip(EVP_PKEY *key) {
    unsigned char input[32], ciphertext[384], recovered[384];
    size_t clen = sizeof(ciphertext), rlen = sizeof(recovered);
    EVP_PKEY_CTX *enc = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    EVP_PKEY_CTX *dec = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    CHECK(enc && dec, "contextos RSA");
    CHECK(RAND_bytes(input, sizeof(input)) == 1, "segredo aleatorio RSA");
    CHECK(EVP_PKEY_encrypt_init(enc) > 0, "inicializar cifracao");
    oaep_config(enc);
    CHECK(EVP_PKEY_encrypt(enc, ciphertext, &clen, input, sizeof(input)) > 0, "cifrar");
    CHECK(clen == 384, "ciphertext RSA de 384 bytes");
    CHECK(EVP_PKEY_decrypt_init(dec) > 0, "inicializar decifracao");
    oaep_config(dec);
    CHECK(EVP_PKEY_decrypt(dec, recovered, &rlen, ciphertext, clen) > 0, "decifrar");
    CHECK(rlen == 32 && CRYPTO_memcmp(input, recovered, 32) == 0, "segredo RSA identico");
    OPENSSL_cleanse(input, sizeof(input));
    OPENSSL_cleanse(recovered, sizeof(recovered));
    EVP_PKEY_CTX_free(enc);
    EVP_PKEY_CTX_free(dec);
}

static void kem_roundtrip(EVP_PKEY *key) {
    unsigned char ciphertext[768], secret[32], recovered[32];
    size_t clen = sizeof(ciphertext), slen = sizeof(secret), rlen = sizeof(recovered);
    EVP_PKEY_CTX *enc = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    EVP_PKEY_CTX *dec = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    CHECK(enc && dec, "contextos ML-KEM");
    CHECK(EVP_PKEY_encapsulate_init(enc, NULL) > 0, "inicializar encapsulamento");
    CHECK(EVP_PKEY_encapsulate(enc, ciphertext, &clen, secret, &slen) > 0, "encapsular");
    CHECK(clen == 768 && slen == 32, "tamanhos ML-KEM");
    CHECK(EVP_PKEY_decapsulate_init(dec, NULL) > 0, "inicializar decapsulacao");
    CHECK(EVP_PKEY_decapsulate(dec, recovered, &rlen, ciphertext, clen) > 0, "decapsular");
    CHECK(rlen == 32 && CRYPTO_memcmp(secret, recovered, 32) == 0, "segredo ML-KEM identico");
    OPENSSL_cleanse(secret, sizeof(secret));
    OPENSSL_cleanse(recovered, sizeof(recovered));
    EVP_PKEY_CTX_free(enc);
    EVP_PKEY_CTX_free(dec);
}

int main(void) {
    int pair, iteration;
    LARGE_INTEGER frequency, previous, current;
    CHECK(QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0, "frequencia QPC");
    CHECK(QueryPerformanceCounter(&previous), "ler QPC");
    for (iteration = 0; iteration < 1000; iteration++) {
        CHECK(QueryPerformanceCounter(&current), "ler QPC");
        CHECK(current.QuadPart >= previous.QuadPart, "QPC nao retrocede");
        previous = current;
    }
    CHECK(OPENSSL_version_major() == 3 && OPENSSL_version_minor() == 5, "versao runtime OpenSSL 3.5");
    CHECK(OpenSSL_version_num() == OPENSSL_VERSION_NUMBER, "cabecalhos e biblioteca de mesma versao");
    for (pair = 0; pair < 3; pair++) {
        EVP_PKEY *rsa = keygen("RSA");
        EVP_PKEY *kem = keygen("ML-KEM-512");
        CHECK(EVP_PKEY_get_bits(rsa) == 3072, "confirmar modulo RSA");
        for (iteration = 0; iteration < 10; iteration++) {
            rsa_roundtrip(rsa);
            kem_roundtrip(kem);
        }
        EVP_PKEY_free(rsa);
        EVP_PKEY_free(kem);
    }
    printf("{\n");
    printf("  \"tipo\": \"validacao_funcional_C_EVP_sem_latencias\",\n");
    printf("  \"openssl_headers\": \"%s\",\n", OPENSSL_VERSION_TEXT);
    printf("  \"openssl_runtime\": \"%s\",\n", OpenSSL_version(OPENSSL_VERSION));
    printf("  \"msvc\": %d,\n", _MSC_FULL_VER);
    printf("  \"arquitetura_bits\": %zu,\n", sizeof(void *) * 8);
    printf("  \"qpc_frequencia_hz\": %lld,\n", frequency.QuadPart);
    printf("  \"qpc_nao_decrescente_1000_leituras\": true,\n");
    printf("  \"pares_de_chaves_por_mecanismo\": 3,\n");
    printf("  \"ciclos_validados_por_mecanismo\": 30,\n");
    printf("  \"rsa_ciphertext_bytes\": 384,\n");
    printf("  \"mlkem_ciphertext_bytes\": 768,\n");
    printf("  \"segredo_bytes\": 32,\n");
    printf("  \"sucesso\": true\n}\n");
    return EXIT_SUCCESS;
}
