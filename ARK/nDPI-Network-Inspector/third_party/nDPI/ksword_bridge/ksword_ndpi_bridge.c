#include "ksword_ndpi_bridge.h"

#include "ndpi_api.h"

#include <stdlib.h>
#include <string.h>

typedef struct KswordNdpiEngine
{
    struct ndpi_detection_module_struct* module;
} KswordNdpiEngine;

typedef struct KswordNdpiFlow
{
    struct ndpi_flow_struct* flow;
    ndpi_protocol last_protocol;
    int classification_complete;
} KswordNdpiFlow;

static void ksword_ndpi_copy_text(char* destination, size_t capacity, const char* source)
{
    if (destination == NULL || capacity == 0)
    {
        return;
    }

    if (source == NULL)
    {
        destination[0] = '\0';
        return;
    }

    strncpy_s(destination, capacity, source, _TRUNCATE);
}

static void ksword_ndpi_write_result(
    KswordNdpiEngine* engine,
    const ndpi_protocol protocol,
    KswordNdpiResult* result_out)
{
    char protocol_name[KSWORD_NDPI_TEXT_CAPACITY] = { 0 };
    const char* category_name;
    const char* breed_name;

    memset(result_out, 0, sizeof(*result_out));
    result_out->master_protocol_id = protocol.proto.master_protocol;
    result_out->application_protocol_id = protocol.proto.app_protocol;
    result_out->category_id = (uint16_t)protocol.category;
    result_out->classification_state = (uint8_t)protocol.state;
    result_out->classified = (uint8_t)(
        protocol.proto.master_protocol != NDPI_PROTOCOL_UNKNOWN ||
        protocol.proto.app_protocol != NDPI_PROTOCOL_UNKNOWN);

    ndpi_protocol2name(engine->module, protocol.proto, protocol_name, sizeof(protocol_name));
    category_name = ndpi_category_get_name(engine->module, protocol.category);
    breed_name = ndpi_get_proto_breed_name(protocol.breed);

    ksword_ndpi_copy_text(result_out->protocol_name, sizeof(result_out->protocol_name), protocol_name);
    ksword_ndpi_copy_text(result_out->category_name, sizeof(result_out->category_name), category_name);
    ksword_ndpi_copy_text(result_out->breed_name, sizeof(result_out->breed_name), breed_name);
}

void* ksword_ndpi_engine_create(void)
{
    KswordNdpiEngine* engine = (KswordNdpiEngine*)calloc(1, sizeof(*engine));
    if (engine == NULL)
    {
        return NULL;
    }

    engine->module = ndpi_init_detection_module(NULL);
    if (engine->module == NULL || ndpi_finalize_initialization(engine->module) != 0)
    {
        if (engine->module != NULL)
        {
            ndpi_exit_detection_module(engine->module);
        }
        free(engine);
        return NULL;
    }

    return engine;
}

void ksword_ndpi_engine_destroy(void* engine_handle)
{
    KswordNdpiEngine* engine = (KswordNdpiEngine*)engine_handle;
    if (engine == NULL)
    {
        return;
    }

    if (engine->module != NULL)
    {
        ndpi_exit_detection_module(engine->module);
    }
    free(engine);
}

void* ksword_ndpi_flow_create(void)
{
    KswordNdpiFlow* flow = (KswordNdpiFlow*)calloc(1, sizeof(*flow));
    if (flow == NULL)
    {
        return NULL;
    }

    flow->flow = (struct ndpi_flow_struct*)ndpi_flow_malloc(SIZEOF_FLOW_STRUCT);
    if (flow->flow == NULL)
    {
        free(flow);
        return NULL;
    }

    memset(flow->flow, 0, SIZEOF_FLOW_STRUCT);
    return flow;
}

void ksword_ndpi_flow_destroy(void* flow_handle)
{
    KswordNdpiFlow* flow = (KswordNdpiFlow*)flow_handle;
    if (flow == NULL)
    {
        return;
    }

    if (flow->flow != NULL)
    {
        ndpi_free_flow(flow->flow);
    }
    free(flow);
}

int ksword_ndpi_process_packet(
    void* engine_handle,
    void* flow_handle,
    const uint8_t* layer3_packet,
    const size_t packet_length,
    const uint64_t timestamp_ms,
    KswordNdpiResult* result_out)
{
    KswordNdpiEngine* engine = (KswordNdpiEngine*)engine_handle;
    KswordNdpiFlow* flow = (KswordNdpiFlow*)flow_handle;
    struct ndpi_flow_input_info input_info;

    if (engine == NULL || engine->module == NULL || flow == NULL || flow->flow == NULL ||
        layer3_packet == NULL || packet_length == 0 || packet_length > UINT16_MAX || result_out == NULL)
    {
        return 0;
    }

    if (!flow->classification_complete)
    {
        memset(&input_info, 0, sizeof(input_info));
        input_info.in_pkt_dir = NDPI_IN_PKT_DIR_UNKNOWN;
        input_info.seen_flow_beginning = NDPI_FLOW_BEGINNING_UNKNOWN;
        flow->last_protocol = ndpi_detection_process_packet(
            engine->module,
            flow->flow,
            layer3_packet,
            (unsigned short)packet_length,
            timestamp_ms,
            &input_info);
        flow->classification_complete = flow->last_protocol.state == NDPI_STATE_CLASSIFIED;
    }

    ksword_ndpi_write_result(engine, flow->last_protocol, result_out);
    return 1;
}
