// Stub libpinyin implementation for Windows build
// All functions return failure/empty results - Chinese IME will not work
#include "pinyin.h"
#include <stdlib.h>

struct _pinyin_context_t { int dummy; };
struct _pinyin_instance_t { int dummy; };

pinyin_context_t *pinyin_init(const char *systemdir, const char *userdir)
{
    (void)systemdir; (void)userdir;
    return (pinyin_context_t *)calloc(1, sizeof(pinyin_context_t));
}

void pinyin_fini(pinyin_context_t *context) { free(context); }
void pinyin_set_options(pinyin_context_t *context, guint32 options) { (void)context; (void)options; }

pinyin_instance_t *pinyin_alloc_instance(pinyin_context_t *context)
{
    (void)context;
    return (pinyin_instance_t *)calloc(1, sizeof(pinyin_instance_t));
}

void pinyin_free_instance(pinyin_instance_t *instance) { free(instance); }
void pinyin_reset(pinyin_instance_t *instance) { (void)instance; }
void pinyin_save(pinyin_context_t *context) { (void)context; }
void pinyin_mask_out(pinyin_context_t *context, guint32 mask, guint32 value) { (void)context; (void)mask; (void)value; }

int   pinyin_parse_more_full_pinyins(pinyin_instance_t *instance, const char *str) { (void)instance; (void)str; return 0; }
int   pinyin_guess_sentence_with_prefix(pinyin_instance_t *instance, const char *prefix) { (void)instance; (void)prefix; return 0; }
void  pinyin_guess_candidates(pinyin_instance_t *instance, size_t offset, int sort_option) { (void)instance; (void)offset; (void)sort_option; }
void  pinyin_get_n_candidate(pinyin_instance_t *instance, guint *num) { (void)instance; if(num) *num = 0; }
int   pinyin_get_candidate(pinyin_instance_t *instance, guint index, lookup_candidate_t **candidate) { (void)instance; (void)index; if(candidate) *candidate = NULL; return 0; }
int   pinyin_get_candidate_string(pinyin_instance_t *instance, lookup_candidate_t *candidate, const char **word) { (void)instance; (void)candidate; if(word) *word = ""; return 0; }
int   pinyin_get_candidate_type(pinyin_instance_t *instance, lookup_candidate_t *candidate, lookup_candidate_type_t *type) { (void)instance; (void)candidate; if(type) *type = 0; return 0; }
int   pinyin_choose_candidate(pinyin_instance_t *instance, size_t offset, lookup_candidate_t *candidate) { (void)instance; (void)offset; (void)candidate; return 0; }

size_t pinyin_get_parsed_input_length(pinyin_instance_t *instance) { (void)instance; return 0; }
int    pinyin_get_pinyin_key(pinyin_instance_t *instance, size_t index, ChewingKey **key) { (void)instance; (void)index; if(key) *key = NULL; return 0; }
int    pinyin_get_pinyin_is_incomplete(pinyin_instance_t *instance, ChewingKey *key) { (void)instance; (void)key; return 1; }
