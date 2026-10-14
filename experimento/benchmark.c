#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <openssl/x509.h>
#include <openssl/core_names.h>

/* Um lote eh uma observacao estatistica; iteracoes nao sao amostras independentes. */
#define TASKS 8
#define MAX_BATCH 4096
#define SECRET_BYTES 32
typedef struct {
    int kem;
    EVP_PKEY *private_key, *public_key;
    EVP_PKEY_CTX *generator, *sender, *receiver;
} Fixture;
typedef struct {
    unsigned char *input, *ciphertext, *output, *shared;
    size_t *cipher_len, *output_len, *shared_len;
    int *send_status, *receive_status;
    EVP_PKEY **keys;
    size_t n, cipher_capacity;
} Batch;
static LARGE_INTEGER frequency;
static FILE *raw_file, *sizes_file;
static const char *run_directory;
static int timed_mode;
static unsigned int order_state;
static const char *task_names[TASKS] = {
    "keygen", "keygen", "send", "send", "receive", "receive", "cycle", "cycle"
};

static __declspec(noreturn) void fail(const char *step) {
    unsigned long code;
    char message[256];
    FILE *failure = NULL;
    if (run_directory) {
        char name[2048];
        int length = snprintf(name, sizeof(name), "%s/FAILURE.txt", run_directory);
        if (length > 0 && (size_t)length < sizeof(name)) failure = fopen(name, "wx");
    }
    fprintf(stderr, "Falha: %s; Windows=%lu\n", step, GetLastError());
    if (failure) fprintf(failure, "Falha: %s\n", step);
    while ((code = ERR_get_error()) != 0) {
        ERR_error_string_n(code, message, sizeof(message));
        fprintf(stderr, "%s\n", message);
        if (failure) fprintf(failure, "%s\n", message);
    }
    if (failure) fclose(failure);
    if (raw_file) { fflush(raw_file); fclose(raw_file); }
    if (sizes_file) { fflush(sizes_file); fclose(sizes_file); }
    exit(EXIT_FAILURE);
}
#define CHECK(x, s) do { if (!(x)) fail(s); } while (0)
static void *allocate(size_t n, size_t width) {
    void *p;
    size_t offset, bytes;
    volatile unsigned char *pages;
    CHECK(width && n <= SIZE_MAX / width, "overflow de alocacao");
    p = OPENSSL_zalloc(n * width);
    CHECK(p, "alocar buffers");
    /* Primeiro acesso de escrita nas paginas fica fora da regiao cronometrada. */
    bytes = n * width; pages = (volatile unsigned char *)p;
    for (offset = 0; offset < bytes; offset += 4096) pages[offset] = 0;
    if (bytes) pages[bytes - 1] = 0;
    return p;
}
static void json_string(FILE *file, const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    fputc('"', file);
    for (; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', file); fputc(*p, file); }
        else if (*p < 32) fprintf(file, "\\u%04x", *p);
        else fputc(*p, file);
    }
    fputc('"', file);
}
static unsigned int integer(const char *text, unsigned int low, unsigned int high) {
    char *end;
    unsigned long n;
    errno = 0;
    CHECK(text[0] >= '0' && text[0] <= '9', "argumento numerico invalido");
    n = strtoul(text, &end, 10);
    CHECK(!errno && *end == '\0' && n >= low && n <= high, "argumento fora do intervalo");
    return (unsigned int)n;
}
static unsigned int next_order(void) {
    unsigned int x = order_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    order_state = x;
    return x;
}
static void shuffle(int *order) {
    int i;
    for (i = 0; i < TASKS; i++) order[i] = i;
    for (i = TASKS - 1; i > 0; i--) {
        unsigned int j = next_order() % ((unsigned int)i + 1);
        int temp = order[i]; order[i] = order[j]; order[j] = temp;
    }
}
static EVP_PKEY_CTX *generator(int kem) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, kem ? "ML-KEM-512" : "RSA", "provider=default");
    CHECK(ctx, "criar contexto de geracao");
    CHECK(EVP_PKEY_keygen_init(ctx) > 0, "inicializar geracao");
    if (!kem) CHECK(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 3072) > 0, "modulo RSA-3072");
    return ctx;
}
static EVP_PKEY *generate(EVP_PKEY_CTX *ctx) {
    EVP_PKEY *key = NULL;
    CHECK(EVP_PKEY_generate(ctx, &key) > 0 && key, "gerar chave");
    return key;
}
static EVP_PKEY *public_only(EVP_PKEY *key) {
    unsigned char *encoded = NULL;
    const unsigned char *cursor;
    EVP_PKEY *public_key;
    int length = i2d_PUBKEY(key, &encoded);
    CHECK(length > 0 && encoded, "codificar chave publica SPKI");
    cursor = encoded;
    public_key = d2i_PUBKEY(NULL, &cursor, length);
    CHECK(public_key && cursor == encoded + length, "importar chave exclusivamente publica");
    OPENSSL_free(encoded);
    return public_key;
}
static void oaep(EVP_PKEY_CTX *ctx) {
    CHECK(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) > 0, "OAEP");
    CHECK(EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) > 0, "SHA-256 OAEP");
    CHECK(EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) > 0, "SHA-256 MGF1");
}
static Fixture fixture(int kem, EVP_PKEY *borrowed) {
    Fixture f;
    memset(&f, 0, sizeof(f));
    f.kem = kem;
    f.generator = generator(kem);
    if (borrowed) {
        CHECK(EVP_PKEY_up_ref(borrowed) == 1, "referenciar chave");
        f.private_key = borrowed;
    } else f.private_key = generate(f.generator);
    f.public_key = public_only(f.private_key);
    f.sender = EVP_PKEY_CTX_new_from_pkey(NULL, f.public_key, "provider=default");
    f.receiver = EVP_PKEY_CTX_new_from_pkey(NULL, f.private_key, "provider=default");
    CHECK(f.sender && f.receiver, "contextos de transporte");
    if (kem) {
        CHECK(EVP_PKEY_encapsulate_init(f.sender, NULL) > 0, "init encapsulamento");
        CHECK(EVP_PKEY_decapsulate_init(f.receiver, NULL) > 0, "init decapsulacao");
    } else {
        CHECK(EVP_PKEY_encrypt_init(f.sender) > 0, "init cifracao"); oaep(f.sender);
        CHECK(EVP_PKEY_decrypt_init(f.receiver) > 0, "init decifracao"); oaep(f.receiver);
    }
    return f;
}
static void fixture_free(Fixture *f) {
    EVP_PKEY_CTX_free(f->generator); EVP_PKEY_CTX_free(f->sender); EVP_PKEY_CTX_free(f->receiver);
    EVP_PKEY_free(f->public_key); EVP_PKEY_free(f->private_key);
}
static Batch batch(size_t n, int kem) {
    Batch b;
    memset(&b, 0, sizeof(b));
    b.n = n; b.cipher_capacity = kem ? 768 : 384;
    b.input = allocate(n, SECRET_BYTES); b.ciphertext = allocate(n, b.cipher_capacity);
    /* RSA decrypt requer um buffer com capacidade de ciphertext, nao somente 32. */
    b.output = allocate(n, b.cipher_capacity); b.shared = allocate(n, SECRET_BYTES);
    b.cipher_len = allocate(n, sizeof(size_t)); b.output_len = allocate(n, sizeof(size_t));
    b.shared_len = allocate(n, sizeof(size_t)); b.send_status = allocate(n, sizeof(int));
    b.receive_status = allocate(n, sizeof(int)); b.keys = allocate(n, sizeof(EVP_PKEY *));
    if (!kem) CHECK(RAND_bytes(b.input, (int)(n * SECRET_BYTES)) == 1, "entradas RSA aleatorias");
    return b;
}
static void batch_free(Batch *b) {
    size_t i;
    for (i = 0; i < b->n; i++) EVP_PKEY_free(b->keys[i]);
    OPENSSL_clear_free(b->input, b->n * SECRET_BYTES);
    OPENSSL_clear_free(b->output, b->n * b->cipher_capacity);
    OPENSSL_clear_free(b->shared, b->n * SECRET_BYTES);
    OPENSSL_free(b->ciphertext); OPENSSL_free(b->cipher_len); OPENSSL_free(b->output_len);
    OPENSSL_free(b->shared_len); OPENSSL_free(b->send_status); OPENSSL_free(b->receive_status);
    OPENSSL_free(b->keys);
}
static void send_one(Fixture *f, Batch *b, size_t i) {
    unsigned char *ct = b->ciphertext + i * b->cipher_capacity;
    b->cipher_len[i] = b->cipher_capacity; b->shared_len[i] = SECRET_BYTES;
    if (f->kem)
        b->send_status[i] = EVP_PKEY_encapsulate(f->sender, ct, &b->cipher_len[i],
                                              b->shared + i * SECRET_BYTES, &b->shared_len[i]);
    else
        b->send_status[i] = EVP_PKEY_encrypt(f->sender, ct, &b->cipher_len[i],
                                           b->input + i * SECRET_BYTES, SECRET_BYTES);
}
static void receive_one(Fixture *f, Batch *b, size_t i) {
    b->output_len[i] = b->cipher_capacity;
    if (f->kem)
        b->receive_status[i] = EVP_PKEY_decapsulate(f->receiver, b->output + i * b->cipher_capacity,
                                                  &b->output_len[i], b->ciphertext + i * b->cipher_capacity,
                                                  b->cipher_len[i]);
    else
        b->receive_status[i] = EVP_PKEY_decrypt(f->receiver, b->output + i * b->cipher_capacity,
                                              &b->output_len[i], b->ciphertext + i * b->cipher_capacity,
                                              b->cipher_len[i]);
}
static void validate(Fixture *f, Batch *b) {
    size_t i;
    for (i = 0; i < b->n; i++) {
        const unsigned char *expected = f->kem ? b->shared + i * SECRET_BYTES : b->input + i * SECRET_BYTES;
        CHECK(b->send_status[i] > 0 && b->receive_status[i] > 0, "status das operacoes");
        CHECK(b->cipher_len[i] == b->cipher_capacity && b->output_len[i] == SECRET_BYTES, "tamanho das saidas");
        if (f->kem) CHECK(b->shared_len[i] == SECRET_BYTES, "segredo ML-KEM de 32 bytes");
        CHECK(CRYPTO_memcmp(expected, b->output + i * b->cipher_capacity, SECRET_BYTES) == 0,
              "comparacao byte a byte de todas as saidas do lote");
    }
}
static void validate_generated(int kem, EVP_PKEY *key) {
    Fixture f = fixture(kem, key);
    Batch b = batch(1, kem);
    if (!kem) CHECK(EVP_PKEY_get_bits(key) == 3072, "bits da chave gerada");
    send_one(&f, &b, 0); receive_one(&f, &b, 0); validate(&f, &b);
    batch_free(&b); fixture_free(&f);
}
static long long run_batch(Fixture *f, int op, size_t n, int measure) {
    Batch b = batch(n, f->kem);
    LARGE_INTEGER start = {0}, end = {0};
    size_t i;
    long long ticks = 0;
    /* Entradas de receive sao preparadas fora da regiao medida. */
    if (op == 2) for (i = 0; i < n; i++) send_one(f, &b, i);
    if (measure) CHECK(QueryPerformanceCounter(&start), "QPC inicial");
    for (i = 0; i < n; i++) {
        if (op == 0) b.send_status[i] = EVP_PKEY_generate(f->generator, &b.keys[i]);
        else if (op == 1) send_one(f, &b, i);
        else if (op == 2) receive_one(f, &b, i);
        else { send_one(f, &b, i); receive_one(f, &b, i); }
    }
    if (measure) {
        CHECK(QueryPerformanceCounter(&end), "QPC final");
        CHECK(end.QuadPart >= start.QuadPart, "QPC nao decrescente");
        ticks = end.QuadPart - start.QuadPart;
    }
    /* Status, tamanhos, validade e liberacao sao conferidos fora do cronometro. */
    if (op == 0) {
        for (i = 0; i < n; i++) {
            CHECK(b.send_status[i] > 0 && b.keys[i], "geracao de chaves do lote");
            validate_generated(f->kem, b.keys[i]);
        }
    } else {
        if (op == 1) for (i = 0; i < n; i++) receive_one(f, &b, i);
        validate(f, &b);
    }
    batch_free(&b);
    return ticks;
}
static void record(const char *phase, unsigned int block, unsigned int sequence, int task,
                   size_t n, long long ticks, int measure, const SYSTEMTIME *utc) {
    fprintf(raw_file, "%s,%04u-%02u-%02uT%02u:%02u:%02u.%03uZ,%u,%u,%s,%s,%zu,%d,",
            phase, utc->wYear, utc->wMonth, utc->wDay, utc->wHour, utc->wMinute,
            utc->wSecond, utc->wMilliseconds, block, sequence,
            task % 2 ? "ML-KEM-512" : "RSA-3072-OAEP-SHA256", task_names[task], n, measure);
    if (measure) fprintf(raw_file, "%lld,%.3f,%.3f", ticks,
                         (double)ticks * 1e9 / (double)frequency.QuadPart,
                         (double)ticks * 1e9 / (double)frequency.QuadPart / (double)n);
    else fprintf(raw_file, ",,");
    fprintf(raw_file, ",1\n");
    CHECK(!ferror(raw_file), "gravar dados brutos");
    fflush(raw_file);
}
static long long sample(Fixture *f, int task, size_t n, const char *phase,
                        unsigned int block, unsigned int sequence, int measure) {
    SYSTEMTIME utc;
    long long ticks;
    GetSystemTime(&utc);
    ticks = run_batch(f, task / 2, n, measure);
    record(phase, block, sequence, task, n, ticks, measure, &utc);
    return ticks;
}
static void sizes(Fixture *f, unsigned int block) {
    PKCS8_PRIV_KEY_INFO *p8 = EVP_PKEY2PKCS8(f->private_key);
    int pub_size = i2d_PUBKEY(f->public_key, NULL), private_size;
    CHECK(p8 && pub_size > 0, "medir DER");
    private_size = i2d_PKCS8_PRIV_KEY_INFO(p8, NULL);
    CHECK(private_size > 0, "medir PKCS8 DER");
    fprintf(sizes_file, "%u,%s,public_key,SubjectPublicKeyInfo-DER,%d\n", block, f->kem ? "ML-KEM-512" : "RSA-3072", pub_size);
    fprintf(sizes_file, "%u,%s,private_key,PKCS8-DER-default-provider,%d\n", block, f->kem ? "ML-KEM-512" : "RSA-3072", private_size);
    PKCS8_PRIV_KEY_INFO_free(p8);
    if (f->kem) {
        unsigned char pub[800], priv[1632];
        size_t publen = 0, privlen = 0;
        CHECK(EVP_PKEY_get_octet_string_param(f->private_key, OSSL_PKEY_PARAM_PUB_KEY, pub, sizeof(pub), &publen) == 1, "ML-KEM ek");
        CHECK(EVP_PKEY_get_octet_string_param(f->private_key, OSSL_PKEY_PARAM_PRIV_KEY, priv, sizeof(priv), &privlen) == 1, "ML-KEM dk");
        CHECK(publen == 800 && privlen == 1632, "tamanhos FIPS203 ek e dk");
        fprintf(sizes_file, "%u,ML-KEM-512,public_key,FIPS203-ek,%zu\n", block, publen);
        fprintf(sizes_file, "%u,ML-KEM-512,private_key,FIPS203-dk,%zu\n", block, privlen);
        OPENSSL_cleanse(priv, sizeof(priv));
    }
    fprintf(sizes_file, "%u,%s,ciphertext,binary,%d\n", block, f->kem ? "ML-KEM-512" : "RSA-3072", f->kem ? 768 : 384);
    fprintf(sizes_file, "%u,%s,secret,binary,32\n", block, f->kem ? "ML-KEM-512" : "RSA-3072");
    CHECK(!ferror(sizes_file), "gravar tamanhos"); fflush(sizes_file);
}
static void path(char *out, size_t capacity, const char *directory, const char *name) {
    CHECK(snprintf(out, capacity, "%s/%s", directory, name) > 0 && strlen(out) < capacity - 1, "caminho muito longo");
}
static void clock_probe(const char *directory) {
    char filename[2048];
    FILE *file;
    int i;
    LARGE_INTEGER a, b;
    path(filename, sizeof(filename), directory, "clock.csv");
    file = fopen(filename, "wx"); CHECK(file, "criar clock.csv exclusivo");
    fprintf(file, "index,delta_ticks,delta_ns\n");
    for (i = 0; i < 2000; i++) {
        CHECK(QueryPerformanceCounter(&a) && QueryPerformanceCounter(&b), "sondar QPC");
        CHECK(b.QuadPart >= a.QuadPart, "QPC do piloto");
        fprintf(file, "%d,%lld,%.3f\n", i, b.QuadPart - a.QuadPart,
                (double)(b.QuadPart - a.QuadPart) * 1e9 / (double)frequency.QuadPart);
    }
    CHECK(fclose(file) == 0, "fechar clock.csv");
}
static void metadata(const char *directory, const char *mode, unsigned int blocks,
                     unsigned int seed, unsigned int cpu, unsigned int warm_rsa,
                     unsigned int warm_fast, const size_t *counts, double target_ms) {
    FILE *file;
    int i;
    HMODULE library = GetModuleHandleA("libcrypto-3-x64.dll");
    char filename[2048], dll_path[2048];
    CHECK(library && GetModuleFileNameA(library, dll_path, sizeof(dll_path)), "identificar DLL carregada");
    path(filename, sizeof(filename), directory, "config.json");
    file = fopen(filename, "wx"); CHECK(file, "criar config.json exclusivo");
    fprintf(file, "{\n  \"schema_version\": 1,\n  \"mode\": "); json_string(file, mode);
    fprintf(file, ",\n  \"openssl_headers\": "); json_string(file, OPENSSL_VERSION_TEXT);
    fprintf(file, ",\n  \"openssl_runtime\": "); json_string(file, OpenSSL_version(OPENSSL_VERSION));
    fprintf(file, ",\n  \"loaded_libcrypto\": "); json_string(file, dll_path);
    fprintf(file, ",\n  \"provider\": \"default\",\n  \"msvc\": %d,\n  \"blocks\": %u,\n  \"seed_order\": %u,\n  \"logical_cpu\": %u,\n  \"qpc_frequency_hz\": %lld,\n  \"target_batch_ms\": %.3f,\n  \"warmup_rsa_keygen\": %u,\n  \"warmup_other_operations\": %u,\n  \"initial_batches\": [",
            _MSC_FULL_VER, blocks, seed, cpu, frequency.QuadPart, target_ms, warm_rsa, warm_fast);
    for (i = 0; i < TASKS; i++) fprintf(file, "%s%zu", i ? ", " : "", counts[i]);
    fprintf(file, "]\n}\n"); CHECK(fclose(file) == 0, "fechar config.json");
}
int main(int argc, char **argv) {
    const char *mode = "validate", *directory = NULL, *batch_list = NULL;
    unsigned int blocks = 0, seed = 20261012, cpu = 0, warm_rsa = 3, warm_fast = 1000, block;
    size_t counts[TASKS] = {1, 2, 2, 2, 2, 2, 2, 2};
    int i, order[TASKS], requested_batches = 0;
    double target_ms = 20.0;
    DWORD_PTR process_mask, system_mask, old_affinity;
    char filename[2048];
    FILE *completion;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            puts("benchmark --mode validate|pilot|collect --out NOVA_PASTA [--blocks N] [--cpu N] [--seed N] [--warm-rsa N] [--warm-fast N] [--target-ms N] [--batches n0,n1,...,n7]");
            puts("Ordem batches: RSA-keygen,ML-keygen,RSA-send,ML-send,RSA-receive,ML-receive,RSA-cycle,ML-cycle."); return 0;
        }
        CHECK(i + 1 < argc, "opcao sem valor");
        if (!strcmp(argv[i], "--mode")) mode = argv[++i];
        else if (!strcmp(argv[i], "--out")) directory = argv[++i];
        else if (!strcmp(argv[i], "--blocks")) blocks = integer(argv[++i], 1, 1000);
        else if (!strcmp(argv[i], "--cpu")) cpu = integer(argv[++i], 0, 63);
        else if (!strcmp(argv[i], "--seed")) seed = integer(argv[++i], 1, UINT_MAX);
        else if (!strcmp(argv[i], "--warm-rsa")) warm_rsa = integer(argv[++i], 1, 100);
        else if (!strcmp(argv[i], "--warm-fast")) warm_fast = integer(argv[++i], 1, MAX_BATCH);
        else if (!strcmp(argv[i], "--target-ms")) target_ms = (double)integer(argv[++i], 1, 1000);
        else if (!strcmp(argv[i], "--batches")) batch_list = argv[++i];
        else fail("opcao desconhecida");
    }
    CHECK(!strcmp(mode, "validate") || !strcmp(mode, "pilot") || !strcmp(mode, "collect"), "modo invalido");
    CHECK(directory, "informe --out");
    timed_mode = strcmp(mode, "validate") != 0;
    if (!timed_mode) { warm_rsa = 2; warm_fast = 2; }
    if (!blocks) blocks = !strcmp(mode, "collect") ? 30 : !strcmp(mode, "pilot") ? 7 : 2;
    if (batch_list) {
        const char *cursor = batch_list;
        for (i = 0; i < TASKS; i++) {
            char number[16]; size_t length = strcspn(cursor, ",");
            CHECK(length > 0 && length < sizeof(number), "lista de lotes invalida");
            memcpy(number, cursor, length); number[length] = '\0';
            counts[i] = integer(number, 1, MAX_BATCH);
            cursor += length;
            if (i < TASKS - 1) { CHECK(*cursor == ',', "faltam lotes"); cursor++; }
            else CHECK(*cursor == '\0', "lotes adicionais");
        }
        requested_batches = 1;
    }
    CHECK(strcmp(mode, "collect") || requested_batches, "coleta exige --batches fixados apos o piloto");
    CHECK(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask), "afinidade permitida");
    CHECK((process_mask & ((DWORD_PTR)1 << cpu)) != 0, "CPU nao permitida");
    old_affinity = SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << cpu);
    CHECK(old_affinity, "fixar thread em uma CPU logica");
    if (timed_mode) CHECK(SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED), "impedir suspensao durante medicao");
    CHECK(OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, NULL) == 1, "OpenSSL sem configuracao externa");
    CHECK(OpenSSL_version_num() == OPENSSL_VERSION_NUMBER && OPENSSL_version_major() == 3 && OPENSSL_version_minor() == 5, "versao da biblioteca");
    CHECK(QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0, "frequencia QPC");
    CHECK(CreateDirectoryA(directory, NULL), "pasta de saida deve ser nova");
    run_directory = directory;
    path(filename, sizeof(filename), directory, "raw.csv"); raw_file = fopen(filename, "wx"); CHECK(raw_file, "criar raw.csv");
    path(filename, sizeof(filename), directory, "sizes.csv"); sizes_file = fopen(filename, "wx"); CHECK(sizes_file, "criar sizes.csv");
    fprintf(raw_file, "phase,timestamp_utc,block,sequence,algorithm,operation,iterations,timed,total_ticks,total_ns,ns_per_operation,validated\n");
    fprintf(sizes_file, "block,algorithm,artifact,representation,bytes\n");
    order_state = seed;
    /* Salvar configuracao inicial tambem permite auditar execucoes interrompidas. */
    metadata(directory, mode, blocks, seed, cpu, warm_rsa, warm_fast, counts, target_ms);
    if (timed_mode) clock_probe(directory);
    {
        Fixture fixtures[2];
        fixtures[0] = fixture(0, NULL); fixtures[1] = fixture(1, NULL);
        shuffle(order);
        for (i = 0; i < TASKS; i++) {
            int task = order[i];
            size_t warm = timed_mode ? (task == 0 ? warm_rsa : warm_fast) : 2;
            sample(&fixtures[task % 2], task, warm, "warmup", 0, (unsigned int)i, 0);
        }
        if (timed_mode && !requested_batches) {
            shuffle(order);
            for (i = 0; i < TASKS; i++) {
                int task = order[i]; size_t n = 1;
                for (;;) {
                    long long ticks = sample(&fixtures[task % 2], task, n, "calibration", 0, (unsigned int)i, 1);
                    if ((double)ticks * 1000.0 / (double)frequency.QuadPart >= target_ms || n == MAX_BATCH) break;
                    n *= 2; if (n > MAX_BATCH) n = MAX_BATCH;
                }
                counts[task] = n;
            }
        }
        fixture_free(&fixtures[0]); fixture_free(&fixtures[1]);
    }
    /* Configuracao definitiva fica separada da inicial. */
    {
        FILE *selected;
        path(filename, sizeof(filename), directory, "selected_batches.txt");
        selected = fopen(filename, "wx"); CHECK(selected, "salvar lotes selecionados");
        for (i = 0; i < TASKS; i++) fprintf(selected, "%s%zu", i ? "," : "", counts[i]);
        fprintf(selected, "\n"); CHECK(fclose(selected) == 0, "fechar lotes");
    }
    for (block = 1; block <= blocks; block++) {
        Fixture fixtures[2];
        fixtures[0] = fixture(0, NULL); fixtures[1] = fixture(1, NULL);
        sizes(&fixtures[0], block); sizes(&fixtures[1], block);
        shuffle(order);
        for (i = 0; i < TASKS; i++) {
            int task = order[i];
            sample(&fixtures[task % 2], task, counts[task], "sample", block, (unsigned int)i, timed_mode);
        }
        fixture_free(&fixtures[0]); fixture_free(&fixtures[1]);
        printf("Bloco %u/%u validado (%s).\n", block, blocks, mode); fflush(stdout);
    }
    {
        int raw_status = fclose(raw_file), size_status = fclose(sizes_file);
        raw_file = NULL; sizes_file = NULL;
        CHECK(raw_status == 0 && size_status == 0, "fechar dados e tamanhos");
    }
    path(filename, sizeof(filename), directory, "SUCCESS.txt"); completion = fopen(filename, "wx"); CHECK(completion, "marcar sucesso");
    fprintf(completion, "mode=%s\nblocks=%u\nvalidated=true\n", mode, blocks); fclose(completion);
    CHECK(SetThreadAffinityMask(GetCurrentThread(), old_affinity), "restaurar afinidade da thread");
    if (timed_mode) SetThreadExecutionState(ES_CONTINUOUS);
    return 0;
}
