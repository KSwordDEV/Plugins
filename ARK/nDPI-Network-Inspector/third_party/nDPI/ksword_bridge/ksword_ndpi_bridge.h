#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KSWORD_NDPI_TEXT_CAPACITY 128

typedef struct KswordNdpiResult
{
    uint16_t master_protocol_id;
    uint16_t application_protocol_id;
    uint16_t category_id;
    uint8_t classification_state;
    uint8_t classified;
    char protocol_name[KSWORD_NDPI_TEXT_CAPACITY];
    char category_name[KSWORD_NDPI_TEXT_CAPACITY];
    char breed_name[KSWORD_NDPI_TEXT_CAPACITY];
} KswordNdpiResult;

void* ksword_ndpi_engine_create(void);
void ksword_ndpi_engine_destroy(void* engine_handle);

void* ksword_ndpi_flow_create(void);
void ksword_ndpi_flow_destroy(void* flow_handle);

int ksword_ndpi_process_packet(
    void* engine_handle,
    void* flow_handle,
    const uint8_t* layer3_packet,
    size_t packet_length,
    uint64_t timestamp_ms,
    KswordNdpiResult* result_out);

#ifdef __cplusplus
}
#endif
