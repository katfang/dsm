#include <stdint.h>

typedef uint64_t data_t;

int set_page_data(void *va, data_t id);
int get_page_data(void *va, data_t *id_out);
