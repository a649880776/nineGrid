#ifndef NINEGRID_ENGINE_H
#define NINEGRID_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NG_MAX_CANDIDATES 12
#define NG_WORD_MAX 64
#define NG_PINYIN_MAX 128
#define NG_DIGITS_MAX 64
#define NG_COMPOSITION_MAX 160

typedef struct NgEngine NgEngine;

typedef struct NgCandidate {
  char word[NG_WORD_MAX];
  char pinyin[NG_PINYIN_MAX];
  float score;
  int32_t flags; /* 1=sentence, 2=word, 4=learned, 8=single char */
} NgCandidate;

typedef struct NgSnapshot {
  char digits[NG_DIGITS_MAX];
  char composition[NG_COMPOSITION_MAX]; /* "ni'hao" or "ni…" */
  int32_t path_count;
  int32_t path_index;
  int32_t candidate_count;
  NgCandidate candidates[NG_MAX_CANDIDATES];
} NgSnapshot;

/* Create/destroy. dict_path is a TSV: word<TAB>pinyin<TAB>freq.
   user_path may be empty/NULL to disable persistence. */
NgEngine* ng_create(const char* dict_path, const char* user_path);
void ng_destroy(NgEngine* engine);

/* Input. Return 0 on success, -1 on error (see ng_last_error). */
int32_t ng_press_digit(NgEngine* engine, int32_t digit); /* 2..9 */
int32_t ng_backspace(NgEngine* engine);
int32_t ng_cycle_path(NgEngine* engine); /* switch segmentation */
int32_t ng_reset(NgEngine* engine);

/* Commit candidate index. Writes UTF-8 word to out (may be NULL). */
int32_t ng_select(NgEngine* engine, int32_t index, char* out, int32_t out_cap);

/* Read current state. */
int32_t ng_snapshot(NgEngine* engine, NgSnapshot* out);

/* Learning / data. */
int32_t ng_set_learning(NgEngine* engine, int32_t enabled);
int32_t ng_clear_user(NgEngine* engine);
int32_t ng_save_user(NgEngine* engine);

/* Human-readable last error, never NULL. */
const char* ng_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NINEGRID_ENGINE_H */
