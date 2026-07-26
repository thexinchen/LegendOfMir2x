// Stub pinyin.h for Windows build without libpinyin
// Provides the API surface used by ime.cpp with no-op implementations
#pragma once

#include <cstddef>
#include <cstdint>

// GLib type stubs
typedef unsigned int guint;
typedef uint32_t guint32;

// Opaque types
typedef struct _pinyin_context_t pinyin_context_t;
typedef struct _pinyin_instance_t pinyin_instance_t;
typedef struct _lookup_candidate_t lookup_candidate_t;
typedef struct _ChewingKey ChewingKey;

// Constants
enum {
    PINYIN_INCOMPLETE  = 1 << 0,
    PINYIN_CORRECT_ALL = 1 << 1,
    USE_DIVIDED_TABLE  = 1 << 2,
    USE_RESPLIT_TABLE  = 1 << 3,
    DYNAMIC_ADJUST     = 1 << 4,
};

enum {
    NBEST_MATCH_CANDIDATE = 0,
};

enum {
    SORT_BY_PHRASE_LENGTH_AND_PINYIN_LENGTH_AND_FREQUENCY = 0,
};

typedef int lookup_candidate_type_t;

// API stubs - implemented in pinyin_stub.c
#ifdef __cplusplus
extern "C" {
#endif

pinyin_context_t  * pinyin_init(const char *systemdir, const char *userdir);
void                pinyin_fini(pinyin_context_t *context);
void                pinyin_set_options(pinyin_context_t *context, guint32 options);
pinyin_instance_t * pinyin_alloc_instance(pinyin_context_t *context);
void                pinyin_free_instance(pinyin_instance_t *instance);
void                pinyin_reset(pinyin_instance_t *instance);
void                pinyin_save(pinyin_context_t *context);
void                pinyin_mask_out(pinyin_context_t *context, guint32 mask, guint32 value);

int   pinyin_parse_more_full_pinyins(pinyin_instance_t *instance, const char *str);
int   pinyin_guess_sentence_with_prefix(pinyin_instance_t *instance, const char *prefix);
void  pinyin_guess_candidates(pinyin_instance_t *instance, size_t offset, int sort_option);
void  pinyin_get_n_candidate(pinyin_instance_t *instance, guint *num);
int   pinyin_get_candidate(pinyin_instance_t *instance, guint index, lookup_candidate_t **candidate);
int   pinyin_get_candidate_string(pinyin_instance_t *instance, lookup_candidate_t *candidate, const char **word);
int   pinyin_get_candidate_type(pinyin_instance_t *instance, lookup_candidate_t *candidate, lookup_candidate_type_t *type);
int   pinyin_choose_candidate(pinyin_instance_t *instance, size_t offset, lookup_candidate_t *candidate);

size_t pinyin_get_parsed_input_length(pinyin_instance_t *instance);
int    pinyin_get_pinyin_key(pinyin_instance_t *instance, size_t index, ChewingKey **key);
int    pinyin_get_pinyin_is_incomplete(pinyin_instance_t *instance, ChewingKey *key);

#ifdef __cplusplus
}
#endif
