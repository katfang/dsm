/* from http://www.mikeash.com/pyblog/friday-qa-2009-08-21-writing-vararg-macros-and-functions.html */

#define DEBUG_LOG(fmt, ...) do { \
  if(DEBUG) \
    fprintf(stderr, "[%s:%d] " fmt "\n", __FILE__, __LINE__, ## __VA_ARGS__); \
  } while(0)
