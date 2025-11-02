#ifndef __COMM_H
#define __COMM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif



#define safe_free(p) do { free(p); (p) = NULL; } while(0)

char *get_process_name(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif
