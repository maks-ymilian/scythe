#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
extern const char* __asan_default_options(void) { return "detect_leaks=0"; }
#pragma GCC diagnostic pop
