#ifndef TREE_SITTER_PARSER_H_
#define TREE_SITTER_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define ts_builtin_sym_error ((TSSymbol)-1)
#define ts_builtin_sym_end 0
#define TREE_SITTER_SERIALIZATION_BUFFER_SIZE 1024

#ifndef TREE_SITTER_API_H_
typedef uint16_t TSStateId;
typedef uint16_t TSSymbol;
typedef uint16_t TSFieldId;
typedef struct TSLanguage TSLanguage;
#endif

typedef struct {
  TSFieldId field_id;
  uint8_t child_index;
  bool inherited;
} TSFieldMapEntry;

typedef struct {
  uint16_t index;
  uint16_t length;
} TSFieldMapSlice;

typedef struct {
  bool visible;
  bool named;
  bool supertype;
} TSSymbolMetadata;

typedef struct TSLexer TSLexer;

struct TSLexer {
  int32_t lookahead;
  TSSymbol result_symbol;
  void (*advance)(TSLexer *, bool);
  void (*mark_end)(TSLexer *);
  uint32_t (*get_column)(TSLexer *);
  bool (*is_at_included_range_start)(const TSLexer *);
  bool (*eof)(const TSLexer *);
};

typedef enum {
  TSParseActionTypeShift,
  TSParseActionTypeReduce,
  TSParseActionTypeAccept,
  TSParseActionTypeRecover,
} TSParseActionType;

typedef union {
  struct {
    uint8_t type;
    TSStateId state;
    bool extra;
    bool repetition;
  } shift;
  struct {
    uint8_t type;
    uint8_t child_count;
    TSSymbol symbol;
    int16_t dynamic_precedence;
    uint16_t production_id;
  } reduce;
  uint8_t type;
} TSParseAction;

typedef struct {
  uint16_t lex_state;
  uint16_t external_lex_state;
} TSLexMode;

typedef union {
  TSParseAction action;
  struct {
    uint8_t count;
    bool reusable;
  } entry;
} TSParseActionEntry;

struct TSLanguage {
  uint32_t version;
  uint32_t symbol_count;
  uint32_t alias_count;
  uint32_t token_count;
  uint32_t external_token_count;
  uint32_t state_count;
  uint32_t large_state_count;
  uint32_t production_id_count;
  uint32_t field_count;
  uint16_t max_alias_sequence_length;
  const uint16_t *parse_table;
  const uint16_t *small_parse_table;
  const uint32_t *small_parse_table_map;
  const TSParseActionEntry *parse_actions;
  const char * const *symbol_names;
  const char * const *field_names;
  const TSFieldMapSlice *field_map_slices;
  const TSFieldMapEntry *field_map_entries;
  const TSSymbolMetadata *symbol_metadata;
  const TSSymbol *public_symbol_map;
  const uint16_t *alias_map;
  const TSSymbol *alias_sequences;
  const TSLexMode *lex_modes;
  bool (*lex_fn)(TSLexer *, TSStateId);
  bool (*keyword_lex_fn)(TSLexer *, TSStateId);
  TSSymbol keyword_capture_token;
  struct {
    const bool *states;
    const TSSymbol *symbol_map;
    void *(*create)(void);
    void (*destroy)(void *);
    bool (*scan)(void *, TSLexer *, const bool *symbol_whitelist);
    unsigned (*serialize)(void *, char *);
    void (*deserialize)(void *, const char *, unsigned);
  } external_scanner;
  const TSStateId *primary_state_ids;
};

/*
 *  Lexer Macros
 */

#ifdef _MSC_VER
#define UNUSED __pragma(warning(suppress : 4101))
#else
#define UNUSED __attribute__((unused))
#endif

#define START_LEXER()           \
  bool result = false;          \
  bool skip = false;            \
  UNUSED                        \
  bool eof = false;             \
  int32_t lookahead;            \
  goto start;                   \
  next_state:                   \
  lexer->advance(lexer, skip);  \
  start:                        \
  skip = false;                 \
  lookahead = lexer->lookahead;

#define ADVANCE(state_value) \
  {                          \
    state = state_value;     \
    goto next_state;         \
  }

#define SKIP(state_value) \
  {                       \
    skip = true;          \
    state = state_value;  \
    goto next_state;      \
  }

#define ACCEPT_TOKEN(symbol_value)     \
  result = true;                       \
  lexer->result_symbol = symbol_value; \
  lexer->mark_end(lexer);

#define END_STATE() return result;

/*
 *  Parse Table Macros
 */

#define SMALL_STATE(id) ((id) - LARGE_STATE_COUNT)

#define STATE(id) id

#define ACTIONS(id) id

#define SHIFT(state_value)            \
  {{                                  \
    .shift = {                        \
      .type = TSParseActionTypeShift, \
      .state = (state_value)          \
    }                                 \
  }}

#define SHIFT_REPEAT(state_value)     \
  {{                                  \
    .shift = {                        \
      .type = TSParseActionTypeShift, \
      .state = (state_value),         \
      .repetition = true              \
    }                                 \
  }}

#define SHIFT_EXTRA()                 \
  {{                                  \
    .shift = {                        \
      .type = TSParseActionTypeShift, \
      .extra = true                   \
    }                                 \
  }}

#define REDUCE(symbol_val, child_count_val, ...) \
  {{                                             \
    .reduce = {                                  \
      .type = TSParseActionTypeReduce,           \
      .symbol = symbol_val,                      \
      .child_count = child_count_val,            \
      __VA_ARGS__                                \
    },                                           \
  }}

#define RECOVER()                    \
  {{                                 \
    .type = TSParseActionTypeRecover \
  }}

#define ACCEPT_INPUT()              \
  {{                                \
    .type = TSParseActionTypeAccept \
  }}

#ifdef __cplusplus
}
#endif

/*
 * External scanner helpers
 */

#define SMALL_LEN 12

// A unicode string, encoded as UTF-8, and optimized for short lengths.
// Because this is designed for use in external scanners, the only
// mutation supported is appending characters.
typedef struct {
  union {
    struct {
      char *data;
      uint32_t capacity;
    } large;
    char small[SMALL_LEN];
  } content;
  uint32_t length;
} TSString;

static inline TSString ts_string_new() {
  TSString self;
  memset(&self, 0, sizeof(self));
  return self;
}

static inline TSString ts_string_from(const char *content, uint32_t length) {
  TSString self;
  self.length = length;
  if (length > SMALL_LEN) {
    self.content.large.data = malloc(length);
    self.content.large.capacity = length;
    memcpy(self.content.large.data, content, length);
  } else {
    memcpy(self.content.small, content, length);
  }
  return self;
}

static inline void ts_string_delete(TSString *self) {
  if (self->length > SMALL_LEN) free(self->content.large.data);
  memset(self, 0, sizeof(*self));
}

static inline char *ts_string_data(TSString *self) {
  if (self->length > SMALL_LEN) {
    return self->content.large.data;
  } else {
    return self->content.small;
  }
}

static inline bool ts_string_eq(const TSString *self, const TSString *other) {
  if (self->length != other->length) return false;
  if (self->length > SMALL_LEN) {
    return memcmp(self->content.large.data, other->content.large.data, self->length) == 0;
  } else {
    return memcmp(self->content.small, other->content.small, self->length) == 0;
  }
}

static inline void ts_string_push(TSString *self, int32_t c) {
  unsigned utf8_len = 0;
  if (c <= 0x7f) {
    utf8_len = 1;
  } else  if (c <= 0x7ff) {
    utf8_len = 2;
  } else  if (c <= 0xffff) {
    utf8_len = 3;
  } else {
    utf8_len = 4;
  }

  uint32_t old_capacity = self->length > SMALL_LEN
    ? self->content.large.capacity
    : SMALL_LEN;
  char *old_data = ts_string_data(self);

  char *data = old_data;
  if (self->length + utf8_len > old_capacity) {
    uint32_t capacity = old_capacity * 2;
    if (old_capacity > SMALL_LEN) {
      data = realloc(data, capacity);
      assert(data != NULL);
    } else {
      data = malloc(capacity);
      assert(data != NULL);
      memcpy(data, old_data, self->length);
    }
    self->content.large.data = data;
    self->content.large.capacity = capacity;
  }

  if (c <= 0x7f) {
      data[self->length++] = (char)c;
  } else {
      if (c <= 0x7ff) {
        data[self->length++] = (char)((c >> 6) | 0xc0);
      } else {
          if (c <= 0xffff) {
            data[self->length++] = (char)((c >> 12) | 0xe0);
          } else {
              data[self->length++] = (char)((c >> 18) | 0xf0);
              data[self->length++] = (char)(((c >> 12) & 0x3f) | 0x80);
          }
          data[self->length++] = (char)(((c >> 6) & 0x3f) | 0x80);
      }
      data[self->length++] = (char)((c & 0x3f) | 0x80);
  }
}

static inline int32_t ts_string_char(TSString *self, uint32_t *i) {
  char *data = ts_string_data(self);
  uint32_t ix = *i;

  if (ix >= self->length) return -1;

  int32_t c = data[ix];
  ix += 1;
  if ((c & 0x80) != 0) {
    if (c < 0xe0) {
      c = ((c & 0x1f) << 6) | (data[ix] & 0x3f);
      ix += 1;
    } else if (c < 0xf0) {
      c = (uint16_t)((c << 12) | ((data[ix] & 0x3f) << 6) | (data[ix + 1] & 0x3f));
      ix += 2;
    } else {
      c = (
        ((c & 7) << 18) |
        ((data[ix] & 0x3f) << 12) |
        ((data[ix + 1] & 0x3f) << 6) |
        (data[ix + 2] & 0x3f)
      );
      ix += 3;
    }
  }

  *i = ix;
  return c;
}

#undef SMALL_LEN

#endif  // TREE_SITTER_PARSER_H_
