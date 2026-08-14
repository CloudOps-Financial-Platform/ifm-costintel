#ifndef IFM_STREAM_ADAPTER_H
#define IFM_STREAM_ADAPTER_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IFM_STREAM_BUFFER_SIZE 65536
#define IFM_MAX_LINE_LENGTH    131072

typedef struct {
    FILE *fp;
    char *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    uint64_t line_number;
    bool eof_reached;
    bool is_stdin;
} ifm_stream_reader_t;

/* Initialize stream reader from FILE pointer */
bool ifm_stream_reader_init(ifm_stream_reader_t *reader, FILE *fp, size_t buffer_size);

/* Read next newline-terminated line. Returns pointer to null-terminated line within line_buf. */
bool ifm_stream_reader_next_line(ifm_stream_reader_t *reader, char **out_line, size_t *out_length);

/* Get current line number */
uint64_t ifm_stream_reader_get_line_number(const ifm_stream_reader_t *reader);

/* Cleanup stream reader */
void ifm_stream_reader_cleanup(ifm_stream_reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif /* IFM_STREAM_ADAPTER_H */
