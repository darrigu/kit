#ifndef KIT_H
#define KIT_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
  KIT_LOG_DEBUG = 0,
  KIT_LOG_INFO,
  KIT_LOG_WARN,
  KIT_LOG_ERROR,
  KIT_LOG_NONE
} Kit_Log_Level;

typedef enum {
  KIT_LOG_STDERR = 0,
  KIT_LOG_FILE,
  KIT_LOG_CUSTOM,
} Kit_Log_Sink;

typedef void (*Kit_Log_Fn)(void *ctx, Kit_Log_Level level, const char *msg);

void kit_log_set_level(Kit_Log_Level level);
void kit_log_set_sink(Kit_Log_Sink sink, const char *file_path);
void kit_log_set_custom(Kit_Log_Fn fn, void *ctx);

typedef struct {
  const char *data;
  size_t len;
} Kit_Str;

#define KIT_STR(lit) {.data = (lit), .len = sizeof(lit) - 1}

#define KIT_STR_FMT "%.*s"
#define KIT_STR_ARG(str) (int)(str).len, (str).data

static inline Kit_Str kit_str_from(const char *s);
static inline Kit_Str kit_str_buf(const char *s, size_t len);

static inline bool kit_str_empty(Kit_Str str);
static inline bool kit_str_eq(Kit_Str a, Kit_Str b);
static inline bool kit_str_eq_cstr(Kit_Str a, const char *b);
static inline bool kit_str_starts_with(Kit_Str str, Kit_Str prefix);
static inline bool kit_str_ends_with(Kit_Str str, Kit_Str suffix);
static inline size_t kit_str_find(Kit_Str str, char c);
static inline size_t kit_str_rfind(Kit_Str str, char c);

static inline Kit_Str kit_str_slice(Kit_Str str, size_t start, size_t end);

static inline Kit_Str kit_str_trim_left(Kit_Str str);
static inline Kit_Str kit_str_trim_right(Kit_Str str);
static inline Kit_Str kit_str_trim(Kit_Str str);

static inline bool kit_str_split(Kit_Str *str, char delim, Kit_Str *out);

#define Kit_Arr(T) struct { T *data; size_t len; size_t cap; }

void *kit__arr_grow(void *data, size_t *cap, size_t item_size);

char *kit_temp_strdup(const char *cstr);
char *kit_temp_strndup(const char *cstr, size_t size);
void *kit_temp_alloc(size_t size);
char *kit_temp_sprintf(const char *format, ...);
char *kit_temp_vsprintf(const char *format, va_list ap);
void kit_temp_reset(void);
size_t kit_temp_save(void);
void kit_temp_rewind(size_t checkpoint);

typedef enum {
  KIT_OK = 0,
  KIT_ERR_SPAWN,
  KIT_ERR_WAIT,
  KIT_ERR_SIGNAL,
  KIT_ERR_ARGS,
  KIT_ERR_BUF,
} Kit_Run_Status;

typedef struct {
  Kit_Run_Status status;
  int exit_code;
  int signal_no;
} Kit_Run_Result;

Kit_Run_Result kit_run(const char *cmd);
// TODO: add a way to capture stdout & stderr separately
Kit_Run_Result kit_run_capture(const char *cmd, char *buf, size_t len);
Kit_Run_Result kit_run_argv(const char *const argv[]);
const char *kit_strerror(Kit_Run_Status s);

bool kit_needs_rebuild(const char *source, const char *binary, bool *out_needs_rebuild);
bool kit_rebuild(const char *source, const char *binary, const char *cc_template);
bool kit_auto_rebuild(int argc, char **argv, const char *source_file, const char *cc_template);

#ifdef KIT_IMPLEMENTATION

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  Kit_Log_Level level;
  Kit_Log_Sink sink;
  FILE *file;
  Kit_Log_Fn custom_fn;
  void *custom_ctx;
} Kit__Logger;

static Kit__Logger kit__log_state = {
  .level = KIT_LOG_INFO,
  .sink = KIT_LOG_STDERR,
};

static const char *kit__level_str(Kit_Log_Level l) {
  switch (l) {
  case KIT_LOG_DEBUG: return "DEBUG";
  case KIT_LOG_INFO:  return "INFO";
  case KIT_LOG_WARN:  return "WARN";
  case KIT_LOG_ERROR: return "ERROR";
  default:            return "?";
  }
}

static void kit__log(Kit_Log_Level level, const char *fmt, ...) {
  if (level < kit__log_state.level || kit__log_state.level == KIT_LOG_NONE) return;

  char msg[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  if (kit__log_state.sink == KIT_LOG_CUSTOM && kit__log_state.custom_fn) {
    kit__log_state.custom_fn(kit__log_state.custom_ctx, level, msg);
    return;
  }

  bool to_file = kit__log_state.sink == KIT_LOG_FILE && kit__log_state.file;
  FILE *dest = to_file ? kit__log_state.file : stderr;

  if (to_file) {
    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(dest, "[%s] [%s] %s\n", ts, kit__level_str(level), msg);
  } else {
    fprintf(dest, "[%s] %s\n", kit__level_str(level), msg);
  }
  fflush(dest);
}

void kit_log_set_level(Kit_Log_Level level) {
  kit__log_state.level = level;
}

static void kit__log_cleanup(void) {
  if (kit__log_state.file) {
    fclose(kit__log_state.file);
    kit__log_state.file = NULL;
  }
}

void kit_log_set_sink(Kit_Log_Sink sink, const char *file_path) {
  static bool cleanup_registered = false;
  if (!cleanup_registered) {
    atexit(kit__log_cleanup);
    cleanup_registered = true;
  }

  if (kit__log_state.file) {
    fclose(kit__log_state.file);
    kit__log_state.file = NULL;
  }
  kit__log_state.sink = sink;
  if (sink == KIT_LOG_FILE) {
    if (!file_path) {
      kit__log(KIT_LOG_WARN, "kit_log_set_sink: file_path is NULL, falling back to stderr");
      kit__log_state.sink = KIT_LOG_STDERR;
      return;
    }
    kit__log_state.file = fopen(file_path, "a");
    if (!kit__log_state.file) {
      kit__log_state.sink = KIT_LOG_STDERR;
      kit__log(KIT_LOG_WARN, "kit_log_set_sink: cannot open '%s': %s — using stderr", file_path, strerror(errno));
    } else {
      kit__log(KIT_LOG_INFO, "logging to file: %s", file_path);
    }
  }
}

void kit_log_set_custom(Kit_Log_Fn fn, void *ctx) {
  kit__log_state.sink = KIT_LOG_CUSTOM;
  kit__log_state.custom_fn = fn;
  kit__log_state.custom_ctx = ctx;
}

static inline Kit_Str kit_str_from(const char *s) {
  return (Kit_Str){.data = s, .len = s ? strlen(s) : 0};
}

static inline Kit_Str kit_str_buf(const char *s, size_t len) {
  return (Kit_Str){.data = s, .len = len};
}

static inline bool kit_str_empty(Kit_Str str) {
  return str.len == 0;
}

static inline bool kit_str_eq(Kit_Str a, Kit_Str b) {
  return a.len == b.len && (a.data == b.data || memcmp(a.data, b.data, a.len) == 0);
}

static inline bool kit_str_eq_cstr(Kit_Str a, const char *b) {
  size_t b_len = b ? strlen(b) : 0;
  return a.len == b_len && memcmp(a.data, b, a.len) == 0;
}

static inline bool kit_str_starts_with(Kit_Str str, Kit_Str prefix) {
  return str.len >= prefix.len && memcmp(str.data, prefix.data, prefix.len) == 0;
}

static inline bool kit_str_ends_with(Kit_Str str, Kit_Str suffix) {
  return str.len >= suffix.len && memcmp(str.data + str.len - suffix.len, suffix.data, suffix.len) == 0;
}

static inline size_t kit_str_find(Kit_Str str, char c) {
  for (size_t i = 0; i < str.len; i++)
    if (str.data[i] == c) return i;
  return str.len;
}

static inline size_t kit_str_rfind(Kit_Str str, char c) {
  for (size_t i = str.len; i-- > 0;)
    if (str.data[i] == c) return i;
  return str.len;
}

static inline Kit_Str kit_str_slice(Kit_Str str, size_t start, size_t end) {
  if (start > str.len) start = str.len;
  if (end > str.len) end = str.len;
  if (end < start)  end = start;
  return (Kit_Str){.data = str.data + start, .len = end - start};
}

static inline Kit_Str kit_str_trim_left(Kit_Str str) {
  while (str.len && isspace(str.data[0])) {
    str.data++;
    str.len--;
  }
  return str;
}

static inline Kit_Str kit_str_trim_right(Kit_Str str) {
  while (str.len && isspace(str.data[str.len - 1]))
    str.len--;
  return str;
}

static inline Kit_Str kit_str_trim(Kit_Str str) {
  return kit_str_trim_right(kit_str_trim_left(str));
}

static inline bool kit_str_split(Kit_Str *str, char delim, Kit_Str *out) {
  if (!str || str->len == 0) return 0;
  size_t pos = kit_str_find(*str, delim);
  *out = kit_str_slice(*str, 0, pos);
  str->data += pos < str->len ? pos + 1 : str->len;
  str->len -= pos < str->len ? pos + 1 : str->len;
  return 1;
}

void *kit__arr_grow(void *data, size_t *cap, size_t item_size) {
  size_t new_cap = *cap == 0 ? 8 : *cap * 2;
  if (new_cap > SIZE_MAX/item_size) {
    fprintf(stderr, "[kit] allocation too large\n");
    exit(1);
  }
  void *new_data = realloc(data, new_cap*item_size);
  if (!new_data) {
    fprintf(stderr, "[kit] out of memory in kit__arr_grow\n");
    exit(1);
  }
  *cap = new_cap;
  return new_data;
}

// FIXME: should allocate all required memory at once
#define kit_arr_reserve(arr, n)                                                \
  do {                                                                         \
    while ((arr)->cap < (size_t)(n)) {                                         \
      (arr)->data =                                                            \
          kit__arr_grow((arr)->data, &(arr)->cap, sizeof(*(arr)->data));       \
    }                                                                          \
  } while (0)

#define kit_arr_push(arr, item)                                                \
  do {                                                                         \
    if ((arr)->len >= (arr)->cap)                                              \
      (arr)->data =                                                            \
          kit__arr_grow((arr)->data, &(arr)->cap, sizeof(*(arr)->data));       \
    (arr)->data[(arr)->len++] = (item);                                        \
  } while (0)

static inline void kit__arr_bounds_error(const char *op, size_t index, size_t len, const char *file, int line) {
  fprintf(stderr, "[kit] array bounds error: %s index=%zu len=%zu at %s:%d\n", op, index, len, file, line);
  fflush(stderr);
  abort();
}

#define kit_arr_get(arr, i)                                                    \
  ((i) < (arr)->len ? (arr)->data[(i)] :                                       \
   (kit__arr_bounds_error("get", (i), (arr)->len, __FILE__, __LINE__),         \
    (arr)->data[0]))

#define kit_arr_pop(arr)                                                       \
  ((arr)->len > 0 ? (arr)->data[--(arr)->len] :                                \
   (kit__arr_bounds_error("pop", 0, 0, __FILE__, __LINE__),                    \
    (arr)->data[0]))

#define kit_arr_try_get(arr, i, out_ptr)                                       \
  ((i) < (arr)->len ? (*(out_ptr) = (arr)->data[(i)], true) : false)

#define kit_arr_try_pop(arr, out_ptr)                                          \
  ((arr)->len > 0 ? (*(out_ptr) = (arr)->data[--(arr)->len], true) : false)

#define kit_arr_free(arr)                                                      \
  do {                                                                         \
    free((arr)->data);                                                         \
    (arr)->data = NULL;                                                        \
    (arr)->len = 0;                                                            \
    (arr)->cap = 0;                                                            \
  } while (0)

typedef struct Kit__Temp_Block {
  struct Kit__Temp_Block *next;
  size_t size;
  size_t used;
  char data[];
} Kit__Temp_Block;

typedef struct {
  Kit__Temp_Block *head;
  Kit__Temp_Block *current;
  size_t block_size;
} Kit__Temp_Allocator;

static Kit__Temp_Allocator kit__temp_allocator = {
  .head = NULL,
  .current = NULL,
  .block_size = 4096,
};

static bool temp_alloc_atexit_registered = false;

static void kit__temp_init(void) {
  if (!temp_alloc_atexit_registered) {
    atexit(kit_temp_reset);
    temp_alloc_atexit_registered = true;
  }
}

static Kit__Temp_Block *kit__temp_alloc_block(size_t size) {
  Kit__Temp_Block *new_block = malloc(sizeof(Kit__Temp_Block) + size);
  if (!new_block) {
    kit__log(KIT_LOG_ERROR, "kit__temp_alloc_block: out of memory");
    return NULL;
  }
  new_block->next = NULL;
  new_block->size = size;
  new_block->used = 0;
  return new_block;
}

static void *kit__temp_ensure_space(size_t size) {
  kit__temp_init();

  if (!kit__temp_allocator.current || kit__temp_allocator.current->used + size > kit__temp_allocator.current->size) {
    size_t new_block_size = kit__temp_allocator.block_size;
    if (size > new_block_size) {
      new_block_size = size;
    }
    Kit__Temp_Block *new_block = kit__temp_alloc_block(new_block_size);
    if (!new_block) {
      return NULL;
    }

    new_block->next = kit__temp_allocator.head;
    kit__temp_allocator.head = new_block;
    kit__temp_allocator.current = new_block;
  }
  return kit__temp_allocator.current->data + kit__temp_allocator.current->used;
}

char *kit_temp_strdup(const char *cstr) {
  if (!cstr) return NULL;
  size_t len = strlen(cstr) + 1;
  char *new_str = kit_temp_alloc(len);
  if (new_str) {
    memcpy(new_str, cstr, len);
  }
  return new_str;
}

char *kit_temp_strndup(const char *cstr, size_t size) {
  if (!cstr) return NULL;
  char *new_str = kit_temp_alloc(size + 1);
  if (new_str) {
    memcpy(new_str, cstr, size);
    new_str[size] = '\0';
  }
  return new_str;
}

void *kit_temp_alloc(size_t size) {
  if (size == 0) return NULL;

  void *ptr = kit__temp_ensure_space(size);
  if (!ptr) {
    return NULL;
  }

  kit__temp_allocator.current->used = ((uintptr_t)ptr - (uintptr_t)kit__temp_allocator.current->data) + size;

  return ptr;
}

char *kit_temp_sprintf(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *result = kit_temp_vsprintf(format, ap);
  va_end(ap);
  return result;
}

char *kit_temp_vsprintf(const char *format, va_list ap) {
  va_list ap_copy;

  va_copy(ap_copy, ap);
  int len = vsnprintf(NULL, 0, format, ap);
  va_end(ap_copy);

  if (len < 0) {
    kit__log(KIT_LOG_ERROR, "kit_temp_vsprintf: vsnprintf failed");
    return NULL;
  }

  size_t required_size = len + 1;
  char *buffer = kit_temp_alloc(required_size);
  if (!buffer) {
    return NULL;
  }

  va_copy(ap_copy, ap);
  vsnprintf(buffer, required_size, format, ap_copy);
  va_end(ap_copy);

  return buffer;
}

void kit_temp_reset(void) {
  Kit__Temp_Block *block = kit__temp_allocator.head;
  while (block) {
    Kit__Temp_Block *next = block->next;
    free(block);
    block = next;
  }
  kit__temp_allocator.head = NULL;
  kit__temp_allocator.current = NULL;
}

size_t kit_temp_save(void) {
  if (!kit__temp_allocator.current) {
    return 0;
  }
  return kit__temp_allocator.current->used;
}

void kit_temp_rewind(size_t checkpoint) {
  if (!kit__temp_allocator.current) {
    if (checkpoint == 0) {
      kit_temp_reset();
    }
    return;
  }

  if (checkpoint == 0) {
    kit_temp_reset();
    return;
  }

  if (checkpoint > kit__temp_allocator.current->used) {
    kit__log(KIT_LOG_WARN, "kit_temp_rewind: checkpoint %zu is beyond current usage %zu, ignoring", checkpoint, kit__temp_allocator.current->used);
    return;
  }

  kit__temp_allocator.current->used = checkpoint;
}

const char *kit_strerror(Kit_Run_Status s) {
  switch (s) {
  case KIT_OK:         return "success";
  case KIT_ERR_SPAWN:  return "failed to spawn child process";
  case KIT_ERR_WAIT:   return "failed to wait for child process";
  case KIT_ERR_SIGNAL: return "child killed by signal";
  case KIT_ERR_ARGS:   return "invalid arguments";
  case KIT_ERR_BUF:    return "output buffer too small (truncated)";
  default:             return "unknown error";
  }
}

static Kit_Run_Result kit__run_result(Kit_Run_Status status, int exit_code, int sig, const char *label) {
  Kit_Run_Result r = {.status = status, .exit_code = exit_code, .signal_no = sig};
  switch (status) {
  case KIT_OK:
    kit__log(KIT_LOG_INFO, "exited 0: %s", label);
    break;
  case KIT_ERR_SIGNAL:
    kit__log(KIT_LOG_ERROR, "killed by signal %d: %s", sig, label);
    break;
  default:
    if (exit_code != 0)
      kit__log(KIT_LOG_WARN, "exited %d: %s", exit_code, label);
    break;
  }
  return r;
}

Kit_Run_Result kit_run(const char *cmd) {
  Kit_Run_Result r = {.status = KIT_ERR_ARGS};

  if (!cmd || cmd[0] == '\0') {
    kit__log(KIT_LOG_ERROR, "kit_run: cmd is NULL or empty");
    return r;
  }

  kit__log(KIT_LOG_INFO, "kit_run: %s", cmd);

  int raw = system(cmd);
  if (raw == -1) {
    r.status = KIT_ERR_SPAWN;
    kit__log(KIT_LOG_ERROR, "kit_run: system() failed: %s", strerror(errno));
    return r;
  }

  if (WIFSIGNALED(raw)) {
    r.status = KIT_ERR_SIGNAL;
    r.signal_no = WTERMSIG(raw);
    kit__log(KIT_LOG_ERROR, "kit_run: child killed by signal %d", r.signal_no);
    return r;
  }

  r.exit_code = WEXITSTATUS(raw);
  return kit__run_result(r.exit_code == 0 ? KIT_OK : KIT_ERR_SPAWN, r.exit_code, 0, cmd);
}

Kit_Run_Result kit_run_capture(const char *cmd, char *buf, size_t len) {
  Kit_Run_Result r = {.status = KIT_ERR_ARGS};

  if (!cmd || cmd[0] == '\0') {
    kit__log(KIT_LOG_ERROR, "kit_run_capture: cmd is NULL or empty");
    return r;
  }
  if (!buf || len == 0) {
    kit__log(KIT_LOG_ERROR, "kit_run_capture: invalid output buffer");
    return r;
  }

  buf[0] = '\0';
  kit__log(KIT_LOG_INFO, "kit_run_capture: %s", cmd);

  FILE *fp = popen(cmd, "r");
  if (!fp) {
    r.status = KIT_ERR_SPAWN;
    kit__log(KIT_LOG_ERROR, "kit_run_capture: popen() failed: %s", strerror(errno));
    return r;
  }

  size_t total = 0;
  while (total < len - 1) {
    size_t n = fread(buf + total, 1, len - 1 - total, fp);
    if (n == 0) break;
    total += n;
  }
  buf[total] = '\0';

  bool truncated = total == len - 1 && !feof(fp);
  if (truncated) {
    char drain[256];
    while (fread(drain, 1, sizeof(drain), fp) > 0);
    kit__log(KIT_LOG_WARN, "kit_run_capture: output truncated at %zu bytes", len - 1);
  }

  int raw = pclose(fp);
  if (raw == -1) {
    r.status = KIT_ERR_WAIT;
    kit__log(KIT_LOG_ERROR, "kit_run_capture: pclose() failed: %s", strerror(errno));
    return r;
  }

  if (WIFSIGNALED(raw)) {
    r.status = KIT_ERR_SIGNAL;
    r.signal_no = WTERMSIG(raw);
    kit__log(KIT_LOG_ERROR, "kit_run_capture: child killed by signal %d", r.signal_no);
    return r;
  }

  r.exit_code = WEXITSTATUS(raw);
  kit__log(KIT_LOG_DEBUG, "kit_run_capture: %zu bytes, exit_code=%d", total, r.exit_code);

  if (truncated) {
    r.status = KIT_ERR_BUF;
    return r;
  }

  return kit__run_result(r.exit_code == 0 ? KIT_OK : KIT_ERR_SPAWN, r.exit_code, 0, cmd);
}

Kit_Run_Result kit_run_argv(const char *const argv[]) {
  Kit_Run_Result r = {.status = KIT_ERR_ARGS};

  if (!argv || !argv[0] || argv[0][0] == '\0') {
    kit__log(KIT_LOG_ERROR, "kit_run_argv: argv is NULL or argv[0] is empty");
    return r;
  }

  char label[256] = {0};
  size_t pos = 0;
  for (int i = 0; argv[i] && pos < sizeof(label) - 1; i++) {
    if (i && pos < sizeof(label) - 1) label[pos++] = ' ';
    size_t len = strlen(argv[i]);
    size_t copy_len = (pos + len < sizeof(label) - 1) ? len : sizeof(label) - 1 - pos;
    memcpy(label + pos, argv[i], copy_len);
    pos += copy_len;
  }
  label[pos] = '\0';

  kit__log(KIT_LOG_INFO, "kit_run_argv: %s", label);
  kit__log(KIT_LOG_DEBUG, "kit_run_argv: calling fork()");

  pid_t pid = fork();
  if (pid < 0) {
    r.status = KIT_ERR_SPAWN;
    kit__log(KIT_LOG_ERROR, "kit_run_argv: fork() failed: %s", strerror(errno));
    return r;
  }

  if (pid == 0) {
    execvp(argv[0], (char *const *)argv);
    fprintf(stderr, "[kit] execvp(%s): %s\n", argv[0], strerror(errno));
    exit(127);
  }

  int status;
  pid_t waited;
  kit__log(KIT_LOG_DEBUG, "kit_run_argv: waiting for pid %d", (int)pid);
  do {
    waited = waitpid(pid, &status, 0);
  } while (waited == -1 && errno == EINTR);

  if (waited == -1) {
    r.status = KIT_ERR_WAIT;
    kit__log(KIT_LOG_ERROR, "kit_run_argv: waitpid() failed: %s", strerror(errno));
    return r;
  }

  if (WIFSIGNALED(status)) {
    r.status = KIT_ERR_SIGNAL;
    r.signal_no = WTERMSIG(status);
    kit__log(KIT_LOG_ERROR, "kit_run_argv: child killed by signal %d", r.signal_no);
    return r;
  }

  r.exit_code = WEXITSTATUS(status);
  return kit__run_result(r.exit_code == 0 ? KIT_OK : KIT_ERR_SPAWN, r.exit_code, 0, label);
}

bool kit_needs_rebuild(const char *source, const char *binary, bool *out_needs_rebuild) {
  struct stat src_stat, bin_stat;

  if (!source || !binary || !out_needs_rebuild)
    return false;

  if (stat(source, &src_stat) != 0)
    return false;

  if (stat(binary, &bin_stat) != 0) {
    *out_needs_rebuild = true;
    return true;
  }

  *out_needs_rebuild = src_stat.st_mtime > bin_stat.st_mtime;
  return true;
}

bool kit_rebuild(const char *source, const char *binary, const char *cc_template) {
  char cmd[1024];

  if (!source || !binary || !cc_template) {
    kit__log(KIT_LOG_ERROR, "kit_rebuild: NULL argument");
    return false;
  }

  bool needs_rebuild;
  if (!kit_needs_rebuild(source, binary, &needs_rebuild))
    return false;

  if (!needs_rebuild) {
    kit__log(KIT_LOG_DEBUG, "kit_rebuild: %s is up to date", binary);
    return true;
  }

  kit__log(KIT_LOG_INFO, "kit_rebuild: recompiling %s -> %s", source, binary);

  int n = snprintf(cmd, sizeof(cmd), cc_template, binary, source);
  if (n < 0 || (size_t)n >= sizeof(cmd)) {
    kit__log(KIT_LOG_ERROR, "kit_rebuild: command too long");
    return false;
  }

  Kit_Run_Result r = kit_run(cmd);
  if (r.status != KIT_OK) {
    kit__log(KIT_LOG_ERROR, "kit_rebuild: compilation failed");
    return false;
  }

  kit__log(KIT_LOG_INFO, "kit_rebuild: compilation successful");
  return true;
}

bool kit_auto_rebuild(int argc, char **argv, const char *source_file, const char *cc_template) {
  const char *binary = (argc > 0 && argv && argv[0]) ? argv[0] : "a.out";

  char bin_path[512];
  if (binary[0] != '/' && binary[0] != '.') {
    snprintf(bin_path, sizeof(bin_path), "./%s", binary);
    binary = bin_path;
  }

  bool needs_rebuild;
  if (!kit_needs_rebuild(source_file, binary, &needs_rebuild)) {
    kit__log(KIT_LOG_ERROR, "kit_auto_rebuild: error checking files");
    return false;
  }

  if (!needs_rebuild) {
    kit__log(KIT_LOG_DEBUG, "kit_auto_rebuild: no rebuild needed");
    return true;
  }

  kit__log(KIT_LOG_INFO, "kit_auto_rebuild: source changed, recompiling...");

  if (!kit_rebuild(source_file, binary, cc_template)) {
    kit__log(KIT_LOG_ERROR, "kit_auto_rebuild: rebuild failed, continuing with old binary");
    return false;
  }

  kit__log(KIT_LOG_INFO, "kit_auto_rebuild: restarting...");

  execv(binary, argv);

  kit__log(KIT_LOG_ERROR, "kit_auto_rebuild: execv failed: %s", strerror(errno));
  return false;
}

#endif
#endif

// TODO: extract the arena allocator from temporary allocator
// TODO: command-line flags parsing
