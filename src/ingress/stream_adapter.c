#include "ifm_costintel/stream_adapter.h"
#include <stdlib.h>
#include <string.h>

bool ifm_stream_reader_init(ifm_stream_reader_t *reader, FILE *fp, size_t buffer_size) {
    if (!reader || !fp) return false;
    if (buffer_size < 1024) buffer_size = IFM_STREAM_BUFFER_SIZE;

    reader->fp = fp;
    reader->capacity = buffer_size;
    reader->buffer = (char *)malloc(buffer_size + 1);
    if (!reader->buffer) return false;

    reader->head = 0;
    reader->tail = 0;
    reader->line_number = 0;
    reader->eof_reached = false;
    reader->is_stdin = (fp == stdin);
    return true;
}

bool ifm_stream_reader_next_line(ifm_stream_reader_t *reader, char **out_line, size_t *out_length) {
    if (!reader || !out_line || !out_length) return false;

    while (1) {
        /* Look for newline in existing buffer between head and tail */
        for (size_t i = reader->head; i < reader->tail; ++i) {
            if (reader->buffer[i] == '\n' || reader->buffer[i] == '\r') {
                size_t line_start = reader->head;
                size_t line_len = i - line_start;

                /* Handle CRLF */
                size_t next_head = i + 1;
                if (reader->buffer[i] == '\r' && next_head < reader->tail && reader->buffer[next_head] == '\n') {
                    next_head++;
                }

                reader->buffer[i] = '\0';
                reader->head = next_head;
                reader->line_number++;

                *out_line = &reader->buffer[line_start];
                *out_length = line_len;
                return true;
            }
        }

        /* If EOF already reached and buffer has remaining characters without newline */
        if (reader->eof_reached) {
            if (reader->head < reader->tail) {
                size_t line_start = reader->head;
                size_t line_len = reader->tail - line_start;
                reader->buffer[reader->tail] = '\0';
                reader->head = reader->tail;
                reader->line_number++;
                *out_line = &reader->buffer[line_start];
                *out_length = line_len;
                return true;
            }
            return false; /* Clean EOF */
        }

        /* Shift unread bytes to start of buffer */
        size_t unread = reader->tail - reader->head;
        if (reader->head > 0) {
            if (unread > 0) {
                memmove(reader->buffer, &reader->buffer[reader->head], unread);
            }
            reader->head = 0;
            reader->tail = unread;
        }

        /* If buffer is full, expand */
        if (reader->tail == reader->capacity) {
            if (reader->capacity >= IFM_MAX_LINE_LENGTH) {
                return false; /* Line exceeds max allowed length */
            }
            size_t new_cap = reader->capacity * 2;
            if (new_cap > IFM_MAX_LINE_LENGTH) new_cap = IFM_MAX_LINE_LENGTH;
            char *new_buf = (char *)realloc(reader->buffer, new_cap + 1);
            if (!new_buf) return false;
            reader->buffer = new_buf;
            reader->capacity = new_cap;
        }

        /* Read more data from file */
        size_t bytes_to_read = reader->capacity - reader->tail;
        size_t bytes_read = fread(&reader->buffer[reader->tail], 1, bytes_to_read, reader->fp);
        if (bytes_read == 0) {
            reader->eof_reached = true;
            if (ferror(reader->fp)) {
                return false;
            }
        } else {
            reader->tail += bytes_read;
        }
    }
}

uint64_t ifm_stream_reader_get_line_number(const ifm_stream_reader_t *reader) {
    return reader ? reader->line_number : 0;
}

void ifm_stream_reader_cleanup(ifm_stream_reader_t *reader) {
    if (!reader) return;
    if (reader->buffer) {
        free(reader->buffer);
        reader->buffer = NULL;
    }
    reader->capacity = 0;
    reader->head = 0;
    reader->tail = 0;
}
