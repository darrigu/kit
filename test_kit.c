#define KIT_IMPLEMENTATION
#include "kit.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name(void)

#define RUN_TEST(name)                                                         \
  do {                                                                         \
    printf("  %s ... ", #name);                                                \
    tests_run++;                                                               \
    test_##name();                                                             \
    printf("PASS\n");                                                          \
    tests_passed++;                                                            \
  } while (0)

#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL at line %d: %s\n", __LINE__, #cond);                        \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(cond) ASSERT(cond)
#define ASSERT_FALSE(cond) ASSERT(!(cond))
#define ASSERT_STREQ(a, b) ASSERT(strcmp(a, b) == 0)
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NONNULL(p) ASSERT((p) != NULL)

static int log_capture_count = 0;
static char log_capture_msg[1024];
static Kit_Log_Level log_capture_level;

static void test_log_handler(void *ctx, Kit_Log_Level level, const char *msg) {
  (void)ctx;
  log_capture_count++;
  log_capture_level = level;
  strncpy(log_capture_msg, msg, sizeof(log_capture_msg) - 1);
  log_capture_msg[sizeof(log_capture_msg) - 1] = '\0';
}

TEST(log_set_level) {
  kit_log_set_level(KIT_LOG_DEBUG);
}

TEST(log_custom_handler) {
  log_capture_count = 0;

  kit_log_set_custom(test_log_handler, NULL);
  kit_log_set_level(KIT_LOG_DEBUG);

  kit__log(KIT_LOG_DEBUG, "test message %d", 42);

  ASSERT_EQ(log_capture_count, 1);
  ASSERT_EQ(log_capture_level, KIT_LOG_DEBUG);
  ASSERT(strstr(log_capture_msg, "test message") != NULL);

  kit_log_set_sink(KIT_LOG_STDERR, NULL);
}

TEST(log_file_sink) {
  const char *test_log = "/tmp/kit_test.log";
  unlink(test_log);

  kit_log_set_sink(KIT_LOG_FILE, test_log);
  kit_log_set_level(KIT_LOG_INFO);

  kit__log(KIT_LOG_INFO, "file test message");

  kit_log_set_sink(KIT_LOG_STDERR, NULL);

  FILE *f = fopen(test_log, "r");
  ASSERT_NONNULL(f);

  char buf[256];
  bool found = false;
  while (fgets(buf, sizeof(buf), f)) {
    if (strstr(buf, "file test message")) {
      found = true;
      break;
    }
  }
  fclose(f);
  unlink(test_log);

  ASSERT_TRUE(found);
}

TEST(log_level_filtering) {
  log_capture_count = 0;
  kit_log_set_custom(test_log_handler, NULL);

  kit_log_set_level(KIT_LOG_WARN);

  kit__log(KIT_LOG_DEBUG, "debug msg");
  kit__log(KIT_LOG_INFO, "info msg");
  kit__log(KIT_LOG_WARN, "warn msg");
  kit__log(KIT_LOG_ERROR, "error msg");

  ASSERT_EQ(log_capture_count, 2);

  log_capture_count = 0;
  kit_log_set_level(KIT_LOG_NONE);
  kit__log(KIT_LOG_ERROR, "should not appear");
  ASSERT_EQ(log_capture_count, 0);

  kit_log_set_sink(KIT_LOG_STDERR, NULL);
}

TEST(log_null_file_path) {
  kit_log_set_sink(KIT_LOG_FILE, NULL);
  kit_log_set_sink(KIT_LOG_STDERR, NULL);
}

TEST(str_from_null) {
  Kit_Str str = kit_str_from(NULL);
  ASSERT_EQ(str.len, 0);
  ASSERT_NULL(str.data);
}

TEST(str_from_string) {
  Kit_Str str = kit_str_from("hello");
  ASSERT_EQ(str.len, 5);
  ASSERT_STREQ(str.data, "hello");
}

TEST(str_buf) {
  Kit_Str str = kit_str_buf("world", 3);
  ASSERT_EQ(str.len, 3);
  ASSERT_STREQ(str.data, "world");
}

TEST(str_empty) {
  ASSERT_TRUE(kit_str_empty(kit_str_from("")));
  ASSERT_TRUE(kit_str_empty(kit_str_from(NULL)));
  ASSERT_FALSE(kit_str_empty(kit_str_from("x")));
  ASSERT_FALSE(kit_str_empty(kit_str_from("abc")));
}

TEST(str_eq) {
  Kit_Str a = kit_str_from("hello");
  Kit_Str b = kit_str_from("hello");
  Kit_Str c = kit_str_from("world");
  Kit_Str d = kit_str_from("hell");
  Kit_Str e = kit_str_from("helloo");
  Kit_Str f = kit_str_from("helLo");

  ASSERT_TRUE(kit_str_eq(a, b));
  ASSERT_TRUE(kit_str_eq(a, a));
  ASSERT_FALSE(kit_str_eq(a, c));
  ASSERT_FALSE(kit_str_eq(a, d));
  ASSERT_FALSE(kit_str_eq(a, e));
  ASSERT_FALSE(kit_str_eq(a, f));

  ASSERT_TRUE(kit_str_eq(kit_str_from(""), kit_str_from("")));
  ASSERT_TRUE(kit_str_eq(kit_str_from(""), kit_str_from(NULL)));
}

TEST(str_eq_cstr) {
  Kit_Str str = kit_str_from("test");

  ASSERT_TRUE(kit_str_eq_cstr(str, "test"));
  ASSERT_FALSE(kit_str_eq_cstr(str, "Test"));
  ASSERT_FALSE(kit_str_eq_cstr(str, "testing"));
  ASSERT_FALSE(kit_str_eq_cstr(str, ""));
  ASSERT_FALSE(kit_str_eq_cstr(str, "tes"));

  ASSERT_TRUE(kit_str_eq_cstr(kit_str_from(NULL), NULL));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_from(""), NULL));
}

TEST(str_starts_with) {
  Kit_Str str = kit_str_from("hello world");

  ASSERT_TRUE(kit_str_starts_with(str, kit_str_from("hello")));
  ASSERT_TRUE(kit_str_starts_with(str, kit_str_from("")));
  ASSERT_TRUE(kit_str_starts_with(str, kit_str_from("hello world")));
  ASSERT_TRUE(kit_str_starts_with(str, kit_str_from("h")));
  ASSERT_FALSE(kit_str_starts_with(str, kit_str_from("world")));
  ASSERT_FALSE(kit_str_starts_with(str, kit_str_from("hello world!")));
  ASSERT_FALSE(kit_str_starts_with(str, kit_str_from("helloo")));

  ASSERT_TRUE(kit_str_starts_with(kit_str_from(""), kit_str_from("")));
  ASSERT_TRUE(kit_str_starts_with(kit_str_from("abc"), kit_str_from("")));
}

TEST(str_ends_with) {
  Kit_Str str = kit_str_from("hello world");

  ASSERT_TRUE(kit_str_ends_with(str, kit_str_from("world")));
  ASSERT_TRUE(kit_str_ends_with(str, kit_str_from("")));
  ASSERT_TRUE(kit_str_ends_with(str, kit_str_from("hello world")));
  ASSERT_TRUE(kit_str_ends_with(str, kit_str_from("d")));
  ASSERT_FALSE(kit_str_ends_with(str, kit_str_from("hello")));
  ASSERT_FALSE(kit_str_ends_with(str, kit_str_from("hello world!")));
  ASSERT_FALSE(kit_str_ends_with(str, kit_str_from("worold")));

  ASSERT_TRUE(kit_str_ends_with(kit_str_from(""), kit_str_from("")));
  ASSERT_TRUE(kit_str_ends_with(kit_str_from("abc"), kit_str_from("")));
}

TEST(str_find) {
  Kit_Str str = kit_str_from("hello world");

  ASSERT_EQ(kit_str_find(str, 'h'), 0);
  ASSERT_EQ(kit_str_find(str, 'e'), 1);
  ASSERT_EQ(kit_str_find(str, 'l'), 2);
  ASSERT_EQ(kit_str_find(str, 'o'), 4);
  ASSERT_EQ(kit_str_find(str, ' '), 5);
  ASSERT_EQ(kit_str_find(str, 'w'), 6);
  ASSERT_EQ(kit_str_find(str, 'r'), 8);
  ASSERT_EQ(kit_str_find(str, 'd'), 10);
  ASSERT_EQ(kit_str_find(str, 'x'), 11);

  ASSERT_EQ(kit_str_find(kit_str_from(""), 'a'), 0);

  Kit_Str str2 = kit_str_from("aaa");
  ASSERT_EQ(kit_str_find(str2, 'a'), 0);
}

TEST(str_rfind) {
  Kit_Str str = kit_str_from("hello world");

  ASSERT_EQ(kit_str_rfind(str, 'h'), 0);
  ASSERT_EQ(kit_str_rfind(str, 'e'), 1);
  ASSERT_EQ(kit_str_rfind(str, 'l'), 9);
  ASSERT_EQ(kit_str_rfind(str, 'o'), 7);
  ASSERT_EQ(kit_str_rfind(str, ' '), 5);
  ASSERT_EQ(kit_str_rfind(str, 'w'), 6);
  ASSERT_EQ(kit_str_rfind(str, 'r'), 8);
  ASSERT_EQ(kit_str_rfind(str, 'd'), 10);
  ASSERT_EQ(kit_str_rfind(str, 'x'), 11);

  ASSERT_EQ(kit_str_rfind(kit_str_from(""), 'a'), 0);

  Kit_Str str2 = kit_str_from("aaa");
  ASSERT_EQ(kit_str_rfind(str2, 'a'), 2);
}

TEST(str_slice) {
  Kit_Str str = kit_str_from("hello world");

  Kit_Str s1 = kit_str_slice(str, 0, 5);
  ASSERT_TRUE(kit_str_eq_cstr(s1, "hello"));

  Kit_Str s2 = kit_str_slice(str, 6, 11);
  ASSERT_TRUE(kit_str_eq_cstr(s2, "world"));

  Kit_Str s3 = kit_str_slice(str, 3, 8);
  ASSERT_TRUE(kit_str_eq_cstr(s3, "lo wo"));

  Kit_Str s4 = kit_str_slice(str, 0, 0);
  ASSERT_TRUE(kit_str_empty(s4));

  Kit_Str s5 = kit_str_slice(str, 5, 5);
  ASSERT_TRUE(kit_str_empty(s5));

  Kit_Str s6 = kit_str_slice(str, 0, 100);
  ASSERT_TRUE(kit_str_eq_cstr(s6, "hello world"));
  ASSERT_EQ(s6.len, 11);

  Kit_Str s7 = kit_str_slice(str, 100, 200);
  ASSERT_TRUE(kit_str_empty(s7));

  Kit_Str s8 = kit_str_slice(str, 8, 3);
  ASSERT_EQ(s8.len, 0);
}

TEST(str_trim_left) {
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_left(kit_str_from("  hello")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_left(kit_str_from("\t\nhello")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_left(kit_str_from(" \t\n hello")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_left(kit_str_from("hello")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_left(kit_str_from("")), ""));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_left(kit_str_from("   ")), ""));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_left(kit_str_from("\t\n\r ")), ""));
}

TEST(str_trim_right) {
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_right(kit_str_from("hello  ")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_right(kit_str_from("hello\t\n")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_right(kit_str_from("hello \t\n\r")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_right(kit_str_from("hello")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_right(kit_str_from("")), ""));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_right(kit_str_from("   ")), ""));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim_right(kit_str_from("\t\n\r ")), ""));
}

TEST(str_trim) {
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim(kit_str_from("  hello  ")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim(kit_str_from("\t\nhello\t\n")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim(kit_str_from("  h e l l o  ")), "h e l l o"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim(kit_str_from("hello")), "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim(kit_str_from("")), ""));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim(kit_str_from("   ")), ""));
  ASSERT_TRUE(kit_str_eq_cstr(kit_str_trim(kit_str_from(" \t\n\r ")), ""));
}

TEST(str_split) {
  Kit_Str str = kit_str_from("hello,world,test");
  Kit_Str part = {0};

  ASSERT_TRUE(kit_str_split(&str, ',', &part));
  ASSERT_TRUE(kit_str_eq_cstr(part, "hello"));
  ASSERT_TRUE(kit_str_eq_cstr(str, "world,test"));

  ASSERT_TRUE(kit_str_split(&str, ',', &part));
  ASSERT_TRUE(kit_str_eq_cstr(part, "world"));
  ASSERT_TRUE(kit_str_eq_cstr(str, "test"));

  ASSERT_TRUE(kit_str_split(&str, ',', &part));
  ASSERT_TRUE(kit_str_eq_cstr(part, "test"));
  ASSERT_TRUE(kit_str_empty(str));

  ASSERT_FALSE(kit_str_split(&str, ',', &part));

  Kit_Str str2 = kit_str_from("hello");
  ASSERT_TRUE(kit_str_split(&str2, ',', &part));
  ASSERT_TRUE(kit_str_eq_cstr(part, "hello"));
  ASSERT_TRUE(kit_str_empty(str2));
  ASSERT_FALSE(kit_str_split(&str2, ',', &part));

  Kit_Str str3 = kit_str_from("");
  ASSERT_FALSE(kit_str_split(&str3, ',', &part));

  Kit_Str str4 = kit_str_from(",");
  ASSERT_TRUE(kit_str_split(&str4, ',', &part));
  ASSERT_TRUE(kit_str_empty(part));
  ASSERT_TRUE(kit_str_empty(str4));
}

TEST(str_macro) {
  Kit_Str str = KIT_STR("hello");
  ASSERT_EQ(str.len, 5);
  ASSERT_STREQ(str.data, "hello");

  Kit_Str str2 = KIT_STR("");
  ASSERT_EQ(str2.len, 0);
}

TEST(arr_basic) {
  Kit_Arr(int) arr = {0};

  ASSERT_EQ(arr.len, 0);
  ASSERT_EQ(arr.cap, 0);
  ASSERT_NULL(arr.data);

  kit_arr_push(&arr, 42);
  ASSERT_EQ(arr.len, 1);
  ASSERT_EQ(arr.data[0], 42);

  kit_arr_push(&arr, 100);
  kit_arr_push(&arr, 200);
  ASSERT_EQ(arr.len, 3);
  ASSERT_EQ(arr.data[0], 42);
  ASSERT_EQ(arr.data[1], 100);
  ASSERT_EQ(arr.data[2], 200);

  kit_arr_free(&arr);
  ASSERT_NULL(arr.data);
  ASSERT_EQ(arr.len, 0);
  ASSERT_EQ(arr.cap, 0);
}

TEST(arr_push_multiple) {
  Kit_Arr(int) arr = {0};

  for (int i = 0; i < 100; i++) {
    kit_arr_push(&arr, i);
  }

  ASSERT_EQ(arr.len, 100);
  ASSERT(arr.cap >= 100);

  for (int i = 0; i < 100; i++) {
    ASSERT_EQ(arr.data[i], i);
  }

  kit_arr_free(&arr);
}

TEST(arr_pop) {
  Kit_Arr(int) arr = {0};

  kit_arr_push(&arr, 10);
  kit_arr_push(&arr, 20);
  kit_arr_push(&arr, 30);

  ASSERT_EQ(kit_arr_pop(&arr), 30);
  ASSERT_EQ(arr.len, 2);

  ASSERT_EQ(kit_arr_pop(&arr), 20);
  ASSERT_EQ(arr.len, 1);

  ASSERT_EQ(kit_arr_pop(&arr), 10);
  ASSERT_EQ(arr.len, 0);

  kit_arr_free(&arr);
}

TEST(arr_get) {
  Kit_Arr(int) arr = {0};

  kit_arr_push(&arr, 5);
  kit_arr_push(&arr, 15);
  kit_arr_push(&arr, 25);

  ASSERT_EQ(kit_arr_get(&arr, 0), 5);
  ASSERT_EQ(kit_arr_get(&arr, 1), 15);
  ASSERT_EQ(kit_arr_get(&arr, 2), 25);

  kit_arr_free(&arr);
}

TEST(arr_reserve) {
  Kit_Arr(int) arr = {0};

  kit_arr_reserve(&arr, 50);
  ASSERT(arr.cap >= 50);
  ASSERT_EQ(arr.len, 0);

  for (int i = 0; i < 50; i++) {
    kit_arr_push(&arr, i);
  }
  ASSERT_EQ(arr.len, 50);

  kit_arr_free(&arr);
}

TEST(arr_strings) {
  Kit_Arr(char *) arr = {0};

  kit_arr_push(&arr, strdup("hello"));
  kit_arr_push(&arr, strdup("world"));
  kit_arr_push(&arr, strdup("test"));

  ASSERT_EQ(arr.len, 3);
  ASSERT_STREQ(arr.data[0], "hello");
  ASSERT_STREQ(arr.data[1], "world");
  ASSERT_STREQ(arr.data[2], "test");

  for (size_t i = 0; i < arr.len; i++) {
    free(arr.data[i]);
  }
  kit_arr_free(&arr);
}

TEST(arr_struct) {
  typedef struct {
    int x;
    int y;
  } Point;

  Kit_Arr(Point) arr = {0};

  kit_arr_push(&arr, ((Point){1, 2}));
  kit_arr_push(&arr, ((Point){3, 4}));
  kit_arr_push(&arr, ((Point){5, 6}));

  ASSERT_EQ(arr.data[0].x, 1);
  ASSERT_EQ(arr.data[0].y, 2);
  ASSERT_EQ(arr.data[1].x, 3);
  ASSERT_EQ(arr.data[1].y, 4);
  ASSERT_EQ(arr.data[2].x, 5);
  ASSERT_EQ(arr.data[2].y, 6);

  kit_arr_free(&arr);
}

TEST(temp_alloc_basic) {
  size_t initial_bytes_used = kit_temp_save();
  ASSERT_EQ(initial_bytes_used, 0);

  void *ptr1 = kit_temp_alloc(100);
  ASSERT_NONNULL(ptr1);
  size_t after_alloc1 = kit_temp_save();
  ASSERT_NE(after_alloc1, 0);
  ASSERT(after_alloc1 >= 100);

  void *ptr2 = kit_temp_alloc(50);
  ASSERT_NONNULL(ptr2);
  ASSERT_NE(ptr2, ptr1);
  size_t after_alloc2 = kit_temp_save();
  ASSERT(after_alloc2 > after_alloc1);

  kit_temp_reset();
  ASSERT_EQ(kit_temp_save(), 0);
}

TEST(temp_strdup) {
  const char *original = "a temporary string";
  char *temp_str = kit_temp_strdup(original);
  ASSERT_NONNULL(temp_str);
  ASSERT_STREQ(temp_str, original);

  size_t checkpoint = kit_temp_save();
  kit_temp_reset();
  ASSERT_EQ(kit_temp_save(), 0);
}

TEST(temp_strndup) {
  const char *original = "a temporary string to truncate";
  size_t len_to_dup = 10;
  char *temp_str = kit_temp_strndup(original, len_to_dup);
  ASSERT_NONNULL(temp_str);
  ASSERT_EQ(strlen(temp_str), len_to_dup);
  ASSERT(strncmp(temp_str, original, len_to_dup) == 0);
  ASSERT_EQ(temp_str[len_to_dup], '\0');

  char *temp_str_long = kit_temp_strndup(original, 100);
  ASSERT_NONNULL(temp_str_long);
  ASSERT_STREQ(temp_str_long, original);

  kit_temp_reset();
}

TEST(temp_sprintf) {
  const char *fmt = "Integer value: %d, String value: %s";
  int num = 123;
  const char *str = "test string";
  char *temp_str = kit_temp_sprintf(fmt, num, str);
  ASSERT_NONNULL(temp_str);

  char expected[256];
  snprintf(expected, sizeof(expected), fmt, num, str);
  ASSERT_STREQ(temp_str, expected);

  kit_temp_reset();
}

TEST(temp_vsprintf) {
  double val = 3.14159;

  char *temp_str_from_sprintf = kit_temp_sprintf("Test for vsprintf: %f", val);
  ASSERT_NONNULL(temp_str_from_sprintf);
  ASSERT(strstr(temp_str_from_sprintf, "3.14159") != NULL);

  kit_temp_reset();
}

TEST(temp_save_and_rewind) {
  size_t checkpoint_initial = kit_temp_save();
  ASSERT_EQ(checkpoint_initial, 0);

  void *ptr1 = kit_temp_alloc(100);
  ASSERT_NONNULL(ptr1);
  size_t checkpoint1 = kit_temp_save();
  ASSERT_NE(checkpoint1, 0);
  ASSERT(checkpoint1 >= 100);

  void *ptr2 = kit_temp_alloc(200);
  ASSERT_NONNULL(ptr2);
  ASSERT_NE(ptr2, ptr1);
  size_t checkpoint2 = kit_temp_save();
  ASSERT(checkpoint2 > checkpoint1);
  ASSERT(checkpoint2 >= checkpoint1 + 200);

  kit_temp_rewind(checkpoint1);
  size_t after_rewind1 = kit_temp_save();
  ASSERT_EQ(after_rewind1, checkpoint1);

  void *ptr3 = kit_temp_alloc(50);
  ASSERT_NONNULL(ptr3);
  size_t after_rewind_alloc = kit_temp_save();
  ASSERT(after_rewind_alloc > checkpoint1);
  ASSERT(after_rewind_alloc < checkpoint2);

  kit_temp_rewind(0);
  ASSERT_EQ(kit_temp_save(), 0);

  void *ptr4 = kit_temp_alloc(10);
  ASSERT_NONNULL(ptr4);
  size_t before_invalid_rewind = kit_temp_save();
  ASSERT_NE(before_invalid_rewind, 0);

  kit_temp_rewind(before_invalid_rewind + 100);
  ASSERT_EQ(kit_temp_save(), before_invalid_rewind);

  kit_temp_reset();
  ASSERT_EQ(kit_temp_save(), 0);
}

TEST(run_null_cmd) {
  Kit_Run_Result r = kit_run(NULL);
  ASSERT_EQ(r.status, KIT_ERR_ARGS);

  r = kit_run("");
  ASSERT_EQ(r.status, KIT_ERR_ARGS);
}

TEST(run_success) {
  Kit_Run_Result r = kit_run("echo hello");
  ASSERT_EQ(r.status, KIT_OK);
  ASSERT_EQ(r.exit_code, 0);
}

TEST(run_failure) {
  Kit_Run_Result r = kit_run("false");
  ASSERT_EQ(r.status, KIT_ERR_SPAWN);
  ASSERT_EQ(r.exit_code, 1);
}

TEST(run_nonexistent) {
  Kit_Run_Result r = kit_run("/nonexistent/path/to/command");
  ASSERT_EQ(r.status, KIT_ERR_SPAWN);
}

TEST(run_capture_null_cmd) {
  char buf[64];
  Kit_Run_Result r = kit_run_capture(NULL, buf, sizeof(buf));
  ASSERT_EQ(r.status, KIT_ERR_ARGS);

  r = kit_run_capture("", buf, sizeof(buf));
  ASSERT_EQ(r.status, KIT_ERR_ARGS);
}

TEST(run_capture_null_buf) {
  Kit_Run_Result r = kit_run_capture("echo test", NULL, 0);
  ASSERT_EQ(r.status, KIT_ERR_ARGS);

  Kit_Run_Result r2 = kit_run_capture("echo test", NULL, 100);
  ASSERT_EQ(r2.status, KIT_ERR_ARGS);
}

TEST(run_capture_success) {
  char buf[256] = {0};
  Kit_Run_Result r = kit_run_capture("echo hello world", buf, sizeof(buf));

  ASSERT_EQ(r.status, KIT_OK);
  ASSERT_EQ(r.exit_code, 0);
  ASSERT(strstr(buf, "hello world") != NULL);
}

TEST(run_capture_truncated) {
  char buf[4] = {0};
  Kit_Run_Result r = kit_run_capture("echo hello", buf, sizeof(buf));

  ASSERT_EQ(r.status, KIT_ERR_BUF);
  ASSERT(buf[0] != '\0');
}

TEST(run_capture_large_output) {
  char buf[4096];
  memset(buf, 0, sizeof(buf));

  Kit_Run_Result r = kit_run_capture("dd if=/dev/zero bs=1k count=2 2>/dev/null | tr '\\0' 'x'", buf, sizeof(buf));

  ASSERT(r.status == KIT_OK || r.status == KIT_ERR_BUF);
}

TEST(run_argv_null) {
  Kit_Run_Result r = kit_run_argv(NULL);
  ASSERT_EQ(r.status, KIT_ERR_ARGS);

  const char *empty_argv[] = {NULL};
  r = kit_run_argv(empty_argv);
  ASSERT_EQ(r.status, KIT_ERR_ARGS);

  const char *empty_first[] = {"", NULL};
  r = kit_run_argv(empty_first);
  ASSERT_EQ(r.status, KIT_ERR_ARGS);
}

TEST(run_argv_success) {
  const char *argv[] = {"echo", "test", NULL};
  Kit_Run_Result r = kit_run_argv(argv);

  ASSERT_EQ(r.status, KIT_OK);
  ASSERT_EQ(r.exit_code, 0);
}

TEST(run_argv_failure) {
  const char *argv[] = {"false", NULL};
  Kit_Run_Result r = kit_run_argv(argv);

  ASSERT_EQ(r.status, KIT_ERR_SPAWN);
  ASSERT_EQ(r.exit_code, 1);
}

TEST(run_argv_nonexistent) {
  const char *argv[] = {"/nonexistent/command", NULL};
  Kit_Run_Result r = kit_run_argv(argv);

  ASSERT_EQ(r.status, KIT_ERR_SPAWN);
}

TEST(run_argv_multicmd) {
  const char *argv[] = {"sh", "-c", "echo hello && exit 0", NULL};
  Kit_Run_Result r = kit_run_argv(argv);

  ASSERT_EQ(r.status, KIT_OK);
  ASSERT_EQ(r.exit_code, 0);
}

TEST(needs_rebuild_null_args) {
  bool result;
  ASSERT_FALSE(kit_needs_rebuild(NULL, "binary", &result));
  ASSERT_FALSE(kit_needs_rebuild("source", NULL, &result));
  ASSERT_FALSE(kit_needs_rebuild("source", "binary", NULL));
  ASSERT_FALSE(kit_needs_rebuild(NULL, NULL, NULL));
}

TEST(needs_rebuild_missing_source) {
  bool result;
  ASSERT_FALSE(kit_needs_rebuild("/nonexistent/source.c", "/tmp/fake_binary", &result));
}

TEST(needs_rebuild_missing_binary) {
  bool result;
  ASSERT_TRUE(kit_needs_rebuild(__FILE__, "/tmp/nonexistent_binary_12345", &result));
  ASSERT_TRUE(result);
}

TEST(needs_rebuild_up_to_date) {
  bool result;
  const char *src = "/tmp/kit_test_rebuild_src.c";
  const char *bin = "/tmp/kit_test_rebuild_bin";

  FILE *f = fopen(src, "w");
  ASSERT_NONNULL(f);
  fprintf(f, "int main(void) { return 0; }\n");
  fclose(f);

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "cc -o %s %s", bin, src);
  Kit_Run_Result r = kit_run(cmd);
  ASSERT_EQ(r.status, KIT_OK);

  ASSERT_TRUE(kit_needs_rebuild(src, bin, &result));
  ASSERT_FALSE(result);

  unlink(src);
  unlink(bin);
}

TEST(needs_rebuild_source_newer) {
  bool result;
  const char *src = "/tmp/kit_test_rebuild_src2.c";
  const char *bin = "/tmp/kit_test_rebuild_bin2";

  FILE *f = fopen(src, "w");
  ASSERT_NONNULL(f);
  fprintf(f, "int main(void) { return 0; }\n");
  fclose(f);

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "cc -o %s %s", bin, src);
  Kit_Run_Result r = kit_run(cmd);
  ASSERT_EQ(r.status, KIT_OK);

  struct timespec ts = {1, 0};
  nanosleep(&ts, NULL);

  f = fopen(src, "a");
  ASSERT_NONNULL(f);
  fprintf(f, "// modified\n");
  fclose(f);

  ASSERT_TRUE(kit_needs_rebuild(src, bin, &result));
  ASSERT_TRUE(result);

  unlink(src);
  unlink(bin);
}

TEST(rebuild_null_args) {
  ASSERT_FALSE(kit_rebuild(NULL, "binary", "cc -o %s %s"));
  ASSERT_FALSE(kit_rebuild("source", NULL, "cc -o %s %s"));
  ASSERT_FALSE(kit_rebuild("source", "binary", NULL));
  ASSERT_FALSE(kit_rebuild(NULL, NULL, NULL));
}

TEST(rebuild_success) {
  const char *src = "/tmp/kit_test_compile_src.c";
  const char *bin = "/tmp/kit_test_compile_bin";

  FILE *f = fopen(src, "w");
  ASSERT_NONNULL(f);
  fprintf(f, "int main(void) { return 42; }\n");
  fclose(f);

  ASSERT_TRUE(kit_rebuild(src, bin, "cc -o %s %s"));

  struct stat st;
  ASSERT_EQ(stat(bin, &st), 0);
  ASSERT_TRUE(st.st_mode & S_IXUSR);

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "%s", bin);
  Kit_Run_Result r = kit_run(cmd);
  ASSERT_EQ(r.exit_code, 42);

  unlink(src);
  unlink(bin);
}

TEST(rebuild_failure) {
  const char *src = "/tmp/kit_test_bad_src.c";
  const char *bin = "/tmp/kit_test_bad_bin";

  FILE *f = fopen(src, "w");
  ASSERT_NONNULL(f);
  fprintf(f, "this is not valid C code !!!\n");
  fclose(f);

  ASSERT_FALSE(kit_rebuild(src, bin, "cc -o %s %s"));

  unlink(src);
  unlink(bin);
}

TEST(rebuild_up_to_date) {
  const char *src = "/tmp/kit_test_uptodate_src.c";
  const char *bin = "/tmp/kit_test_uptodate_bin";

  FILE *f = fopen(src, "w");
  ASSERT_NONNULL(f);
  fprintf(f, "int main(void) { return 0; }\n");
  fclose(f);

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "cc -o %s %s", bin, src);
  Kit_Run_Result r = kit_run(cmd);
  ASSERT_EQ(r.status, KIT_OK);

  ASSERT_TRUE(kit_rebuild(src, bin, "cc -o %s %s"));

  unlink(src);
  unlink(bin);
}

TEST(auto_rebuild_null_args) {
  ASSERT_FALSE(kit_auto_rebuild(0, NULL, NULL, NULL));
}

TEST(auto_rebuild_missing_source) {
  char *fake_argv[] = {"./fake_binary", NULL};
  ASSERT_FALSE(kit_auto_rebuild(1, fake_argv, "/nonexistent/source.c", "cc -o %s %s"));
}

TEST(integration_str_and_arr) {
  Kit_Arr(Kit_Str) parts = {0};

  Kit_Str input = kit_str_from("one,two,three,four");
  Kit_Str remaining = input;
  Kit_Str part;

  while (kit_str_split(&remaining, ',', &part)) {
    Kit_Str trimmed = kit_str_trim(part);
    kit_arr_push(&parts, trimmed);
  }

  ASSERT_EQ(parts.len, 4);
  ASSERT_TRUE(kit_str_eq_cstr(parts.data[0], "one"));
  ASSERT_TRUE(kit_str_eq_cstr(parts.data[1], "two"));
  ASSERT_TRUE(kit_str_eq_cstr(parts.data[2], "three"));
  ASSERT_TRUE(kit_str_eq_cstr(parts.data[3], "four"));

  kit_arr_free(&parts);
}

TEST(integration_path_parsing) {
  Kit_Str path = kit_str_from("/home/user/documents/file.txt");

  size_t last_slash = kit_str_rfind(path, '/');
  ASSERT_NE(last_slash, path.len);

  Kit_Str file_name = kit_str_slice(path, last_slash + 1, path.len);
  Kit_Str dir = kit_str_slice(path, 0, last_slash);

  ASSERT_TRUE(kit_str_eq_cstr(file_name, "file.txt"));
  ASSERT_TRUE(kit_str_eq_cstr(dir, "/home/user/documents"));

  size_t dot = kit_str_rfind(file_name, '.');
  Kit_Str ext = kit_str_slice(file_name, dot + 1, file_name.len);
  ASSERT_TRUE(kit_str_eq_cstr(ext, "txt"));
}

TEST(integration_command_builder) {
  Kit_Arr(char) cmd = {0};

  const char *parts[] = {"echo", "-n", "Hello", "World", NULL};

  for (int i = 0; parts[i]; i++) {
    if (i > 0) {
      kit_arr_push(&cmd, ' ');
    }
    for (const char *p = parts[i]; *p; p++) {
      kit_arr_push(&cmd, *p);
    }
  }
  kit_arr_push(&cmd, '\0');

  Kit_Run_Result r = kit_run_capture(cmd.data, (char[64]){0}, 64);
  ASSERT_EQ(r.status, KIT_OK);

  kit_arr_free(&cmd);
}

TEST(edge_case_empty_strings) {
  Kit_Str empty = kit_str_from("");
  Kit_Str null_str = kit_str_from(NULL);

  ASSERT_TRUE(kit_str_empty(empty));
  ASSERT_TRUE(kit_str_empty(null_str));
  ASSERT_TRUE(kit_str_eq(empty, null_str));
  ASSERT_TRUE(kit_str_eq_cstr(empty, ""));
  ASSERT_TRUE(kit_str_eq_cstr(null_str, ""));

  ASSERT_TRUE(kit_str_empty(kit_str_trim(empty)));
  ASSERT_TRUE(kit_str_empty(kit_str_trim(null_str)));

  Kit_Str sliced = kit_str_slice(empty, 0, 10);
  ASSERT_TRUE(kit_str_empty(sliced));
}

TEST(edge_case_single_char) {
  Kit_Str str = kit_str_from("x");

  ASSERT_EQ(str.len, 1);
  ASSERT_FALSE(kit_str_empty(str));
  ASSERT_TRUE(kit_str_eq(str, str));
  ASSERT_TRUE(kit_str_eq_cstr(str, "x"));
  ASSERT_TRUE(kit_str_starts_with(str, kit_str_from("x")));
  ASSERT_TRUE(kit_str_ends_with(str, kit_str_from("x")));
  ASSERT_TRUE(kit_str_starts_with(str, kit_str_from("")));
  ASSERT_TRUE(kit_str_ends_with(str, kit_str_from("")));

  ASSERT_EQ(kit_str_find(str, 'x'), 0);
  ASSERT_EQ(kit_str_rfind(str, 'x'), 0);
  ASSERT_EQ(kit_str_find(str, 'y'), 1);

  Kit_Str sliced = kit_str_slice(str, 0, 1);
  ASSERT_TRUE(kit_str_eq(sliced, str));
}

TEST(edge_case_unicode_like) {
  Kit_Str str = kit_str_buf("hello\x01\x02world", 13);

  ASSERT_EQ(str.len, 13);
  ASSERT_FALSE(kit_str_empty(str));

  Kit_Str trimmed = kit_str_trim(str);
  ASSERT_EQ(trimmed.len, 13);

  Kit_Str str2 = kit_str_buf("  hello  ", 9);
  Kit_Str trimmed2 = kit_str_trim(str2);
  ASSERT_TRUE(kit_str_eq_cstr(trimmed2, "hello"));
}

TEST(performance_arr_growth) {
  Kit_Arr(int) arr = {0};

  for (int i = 0; i < 10000; i++) {
    kit_arr_push(&arr, i);
  }

  ASSERT_EQ(arr.len, 10000);
  ASSERT(arr.cap >= 10000);

  for (int i = 0; i < 10000; i++) {
    ASSERT_EQ(arr.data[i], i);
  }

  kit_arr_free(&arr);
}

int main(int argc, char **argv) {
  if (!kit_auto_rebuild(argc, argv, __FILE__, "cc -Wall -o %s %s"))
    return 1;

  printf("Kit Tests:\n");

  printf("\n--- Logging Tests ---\n");
  RUN_TEST(log_set_level);
  RUN_TEST(log_custom_handler);
  RUN_TEST(log_file_sink);
  RUN_TEST(log_level_filtering);
  RUN_TEST(log_null_file_path);

  printf("\n--- String Utilities Tests ---\n");
  RUN_TEST(str_from_null);
  RUN_TEST(str_from_string);
  RUN_TEST(str_buf);
  RUN_TEST(str_empty);
  RUN_TEST(str_eq);
  RUN_TEST(str_eq_cstr);
  RUN_TEST(str_starts_with);
  RUN_TEST(str_ends_with);
  RUN_TEST(str_find);
  RUN_TEST(str_rfind);
  RUN_TEST(str_slice);
  RUN_TEST(str_trim_left);
  RUN_TEST(str_trim_right);
  RUN_TEST(str_trim);
  RUN_TEST(str_split);
  RUN_TEST(str_macro);

  printf("\n--- Dynamic Arrays Tests ---\n");
  RUN_TEST(arr_basic);
  RUN_TEST(arr_push_multiple);
  RUN_TEST(arr_pop);
  RUN_TEST(arr_get);
  RUN_TEST(arr_reserve);
  RUN_TEST(arr_strings);
  RUN_TEST(arr_struct);

  printf("\n--- Temporary Memory Management Tests ---\n");
  RUN_TEST(temp_alloc_basic);
  RUN_TEST(temp_strdup);
  RUN_TEST(temp_strndup);
  RUN_TEST(temp_sprintf);
  RUN_TEST(temp_vsprintf);
  RUN_TEST(temp_save_and_rewind);

  printf("\n--- Process Execution Tests ---\n");
  RUN_TEST(run_null_cmd);
  RUN_TEST(run_success);
  RUN_TEST(run_failure);
  RUN_TEST(run_nonexistent);
  RUN_TEST(run_capture_null_cmd);
  RUN_TEST(run_capture_null_buf);
  RUN_TEST(run_capture_success);
  RUN_TEST(run_capture_truncated);
  RUN_TEST(run_capture_large_output);
  RUN_TEST(run_argv_null);
  RUN_TEST(run_argv_success);
  RUN_TEST(run_argv_failure);
  RUN_TEST(run_argv_nonexistent);
  RUN_TEST(run_argv_multicmd);

  printf("\n--- Build Utilities Tests ---\n");
  RUN_TEST(needs_rebuild_null_args);
  RUN_TEST(needs_rebuild_missing_source);
  RUN_TEST(needs_rebuild_missing_binary);
  RUN_TEST(needs_rebuild_up_to_date);
  RUN_TEST(needs_rebuild_source_newer);
  RUN_TEST(rebuild_null_args);
  RUN_TEST(rebuild_success);
  RUN_TEST(rebuild_failure);
  RUN_TEST(rebuild_up_to_date);
  RUN_TEST(auto_rebuild_null_args);
  RUN_TEST(auto_rebuild_missing_source);

  printf("\n--- Integration & Edge Case Tests ---\n");
  RUN_TEST(integration_str_and_arr);
  RUN_TEST(integration_path_parsing);
  RUN_TEST(integration_command_builder);
  RUN_TEST(edge_case_empty_strings);
  RUN_TEST(edge_case_single_char);
  RUN_TEST(edge_case_unicode_like);

  printf("\n--- Performance Tests ---\n");
  RUN_TEST(performance_arr_growth);

  printf("\n=========================\n");
  printf("Total Tests Run: %d\n", tests_run);
  printf("Tests Passed:    %d\n", tests_passed);
  printf("Tests Failed:    %d\n", tests_failed);
  printf("=========================\n");

  return tests_failed > 0 ? 1 : 0;
}
