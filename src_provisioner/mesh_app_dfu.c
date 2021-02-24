/*
* $ Copyright 2016-YEAR Cypress Semiconductor $
*/

/** @file
 *
 * Mesh DFU related functionality
 *
 */

#include "bt_types.h"
#include "sha2.h"
#include "p_256_ecc_pp.h"
#include "wiced_bt_ble.h"
#include "wiced_firmware_upgrade.h"
#include "wiced_bt_mesh_model_utils.h"
#include "wiced_bt_mesh_dfu.h"
#include "wiced_bt_trace.h"
#include "wiced_hal_wdog.h"
#include "hci_control_api.h"


/******************************************************
 *          Constants
 ******************************************************/

//#define DONT_ALLOW_TO_DOWNLOAD_SAME_VERSION

/* Metadata is 2 bytes CID, 2 bytes PID, 2 bytes VID, 1 byte major version, 1 byte minor version, 1 byte revision, 2 bytes build */
#define MESH_DFU_METADATA_LEN                       11

#define FW_UPDATE_NVRAM_MAX_READ_SIZE               512

#define HASH_SIZE                                   32
#define SIGNATURE_SIZE                              64

#ifndef WICED_SDK_MAJOR_VER
#define WICED_SDK_BUILD_NUMBER  0
#define WICED_SDK_MAJOR_VER     0
#define WICED_SDK_MINOR_VER     0
#define WICED_SDK_REV_NUMBER    0
#endif

/******************************************************
 *          Function Prototypes
 ******************************************************/

#ifdef MESH_DFU_OOB_SUPPORTED
static uint8_t      mesh_app_fw_oob_upload_callback(mesh_dfu_uri_t *p_uri, mesh_dfu_fw_id_t *p_fw_id);
#endif
static wiced_bool_t mesh_get_active_fw_id(mesh_dfu_fw_id_t *p_fw_id);
static wiced_bool_t mesh_metadata_check(mesh_dfu_metadata_t *p_metadata);
static wiced_bool_t mesh_fw_verify(uint32_t fw_size, mesh_dfu_metadata_t *p_metadata);

extern wiced_result_t mesh_application_send_hci_event(uint16_t opcode, const uint8_t *p_data, uint16_t data_len);

/******************************************************
 *          Variables Definitions
 ******************************************************/

#ifdef MESH_DFU_OOB_SUPPORTED
uint8_t mesh_dfu_oob_supported_schemes[] = { WICED_BT_MESH_FW_UPDATE_URI_SCHEME_HTTP,
                                             WICED_BT_MESH_FW_UPDATE_URI_SCHEME_HTTPS };
#endif

// public key. Ecdsa256_pub.c should be generated and included in the project.
extern Point ecdsa256_public_key;

/******************************************************
 *               Function Definitions
 ******************************************************/
wiced_result_t mesh_app_dfu_init()
{
    WICED_BT_TRACE("Mesh DFU init\n");

    wiced_bt_mesh_set_dfu_callbacks(mesh_get_active_fw_id, mesh_metadata_check, mesh_fw_verify);

    return WICED_SUCCESS;
}

#ifdef MESH_DFU_OOB_SUPPORTED

void mesh_app_dfu_oob_init()
{
    wiced_bt_mesh_model_fw_upload_oob_init((uint8_t)sizeof(mesh_dfu_oob_supported_schemes),
            mesh_dfu_oob_supported_schemes, mesh_app_fw_oob_upload_callback);
}

/*
 * Out-Of-Band firmware upload uses HTTP to retrieve firmware image from a server. It construct the Request
 * URI with Update URI and FW ID input, send an HTTP GET request with the Request URI. The HTTP server
 * returns a firmware archive which contains FW image, FW ID, and metadata. (See mesh DFU spec section 8.2)
 *
 * The following code send Upload request through WICED HCI. The OOB Upload logic is implemented in PC (Client
 * Control or Python script.)
 */
wiced_timer_t oob_timer;

static void mesh_app_fw_upload_oob_time_out(TIMER_PARAM_TYPE arg)
{
    static int counter = 0;

    if (counter == 0)
    {
        wiced_bt_mesh_fw_upload_start_t upload_start;

        upload_start.fw_size = 4096;
        wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_START, &upload_start);
    }
    else if (counter == 1)
    {
        mesh_dfu_metadata_t metadata = { 11, { 0x31, 0x01, 0x16, 0x30, 0x02, 0x00, 0x06, 0x04, 0x00, 0x24, 0x00} };
        mesh_dfu_fw_id_t fw_id = { 4, { 0x11, 0x11, 0x11, 0x11 } };

        wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_METADATA, &metadata);
        wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_FW_ID, &fw_id);
    }
    else if (counter < 10)
    {
        wiced_bt_mesh_fw_upload_data_t upload_data;
        uint8_t buffer[512];

        memset(buffer, (uint8_t)counter, 512);
        upload_data.p_data = buffer;
        upload_data.data_len = 512;
        upload_data.offset = 512 * (counter - 2);
        wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_DATA, &upload_data);
    }
    else
    {
        wiced_bt_mesh_fw_upload_finish_t upload_finish;

        upload_finish.result = WICED_BT_MESH_UPLOAD_SUCCESS;
        wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_FINISH, &upload_finish);
        wiced_stop_timer(&oob_timer);
    }
    counter++;
}

wiced_bool_t check_uri_valid(char *uri)
{
    //char *p_scheme = NULL;
    char *p_authority = NULL;
    char *p_path = NULL;
    char *p;

    p = strchr(uri, ':');
    if (p)
    {
        //p_scheme = uri;
        if (memcmp(p + 1, "//", 2) == 0)
        {
            p_authority = p + 3;
            p = strchr(p_authority, '/');
            if (p)
                p_path = p + 1;
        }
        else
            p_path = p + 1;
    }

    return p_path ? WICED_TRUE : WICED_FALSE;
}

uint8_t mesh_app_fw_oob_upload_callback(mesh_dfu_uri_t *p_uri, mesh_dfu_fw_id_t *p_fw_id)
{
    uint8_t buf[512];
    uint8_t *p = buf;

    WICED_BT_TRACE("Mesh app FW OOB upload from %s\n", (char *)p_uri->uri);

    p_uri->uri[p_uri->len] = 0;
    if (!check_uri_valid((char *)p_uri->uri))
        return WICED_BT_MESH_FW_DISTR_STATUS_URI_MALFORMED;
    else if (memcmp(p_uri->uri, "http:", 5) != 0 && memcmp(p_uri->uri, "https:", 6) != 0)
        return WICED_BT_MESH_FW_DISTR_STATUS_URI_NOT_SUPPORTED;

#if 0
    UINT8_TO_STREAM(p, p_uri->len);
    memcpy(p, p_uri->uri, p_uri->len);
    p += p_uri->len;
    UINT8_TO_STREAM(p, p_fw_id->fw_id_len);
    memcpy(p, p_fw_id->fw_id, p_fw_id->fw_id_len);
    p += p_fw_id->fw_id_len;

    mesh_application_send_hci_event(HCI_CONTROL_MESH_EVENT_FW_DISTRIBUTION_UPLOAD_REQUEST, buf, p - buf);
#else
    wiced_init_timer(&oob_timer, &mesh_app_fw_upload_oob_time_out, 0, WICED_SECONDS_PERIODIC_TIMER);
    wiced_start_timer(&oob_timer, 5);
#endif

    return WICED_BT_MESH_FW_DISTR_STATUS_SUCCESS;
}
#endif

/*
 * Get FW ID of active firmware
 */
wiced_bool_t mesh_get_active_fw_id(mesh_dfu_fw_id_t *p_fw_id)
{
    uint8_t *p = p_fw_id->fw_id;

    UINT16_TO_STREAM(p, mesh_config.company_id);
    UINT16_TO_STREAM(p, mesh_config.product_id);
    UINT16_TO_STREAM(p, mesh_config.vendor_id);
    UINT8_TO_STREAM(p, WICED_SDK_MAJOR_VER);
    UINT8_TO_STREAM(p, WICED_SDK_MINOR_VER);
    UINT8_TO_STREAM(p, WICED_SDK_REV_NUMBER);
    UINT16_TO_STREAM(p, WICED_SDK_BUILD_NUMBER);
    p_fw_id->fw_id_len = p - p_fw_id->fw_id;

    return WICED_TRUE;
}

/*
 * Check if the Metadata is suitable for this device
 */
wiced_bool_t mesh_metadata_check(mesh_dfu_metadata_t *p_metadata)
{
    uint16_t cid, pid, vid;
    uint8_t ver_major, ver_minor, revision;
    uint16_t build;
    uint8_t *p = p_metadata->data;

    if (p_metadata->len != MESH_DFU_METADATA_LEN)
        return WICED_FALSE;

    STREAM_TO_UINT16(cid, p);
    STREAM_TO_UINT16(pid, p);
    STREAM_TO_UINT16(vid, p);
    STREAM_TO_UINT8(ver_major, p);
    STREAM_TO_UINT8(ver_minor, p);
    STREAM_TO_UINT8(revision, p);
    STREAM_TO_UINT16(build, p);

    if (cid != mesh_config.company_id || pid != mesh_config.product_id || vid != mesh_config.vendor_id)
    {
        WICED_BT_TRACE("FW ID not acceptable cid:%d/%d\n", cid, mesh_config.company_id);
        WICED_BT_TRACE("FW ID not acceptable pid:%d/%d\n", pid, mesh_config.product_id);
        WICED_BT_TRACE("FW ID not acceptable vid:%d/%d\n", vid, mesh_config.vendor_id);
        return WICED_FALSE;
    }
    if (ver_major > WICED_SDK_MAJOR_VER)
        return WICED_TRUE;
    if (ver_major < WICED_SDK_MAJOR_VER)
    {
        WICED_BT_TRACE("FW ID not acceptable ver_major:%d/%d\n", ver_major, WICED_SDK_MAJOR_VER);
        return WICED_FALSE;
    }
    // It is typically allowed to downgrade to earlier minor/revision
    if (ver_minor != WICED_SDK_MINOR_VER)
        return WICED_TRUE;
    if (revision != WICED_SDK_REV_NUMBER)
        return WICED_TRUE;
#ifdef DONT_ALLOW_TO_DOWNLOAD_SAME_VERSION
    if (build == WICED_SDK_BUILD_NUMBER)
        return WICED_FALSE;
#else
    UNUSED_VARIABLE(build);
#endif
    return WICED_TRUE;
}

/*
 * Verify the signature of received FW
 */
wiced_bool_t mesh_fw_verify(uint32_t fw_size, mesh_dfu_metadata_t *p_metadata)
{
    sha2_context sha2_ctx;
    uint8_t digest[HASH_SIZE];
    uint8_t signature[SIGNATURE_SIZE + 4];
    uint8_t buf[FW_UPDATE_NVRAM_MAX_READ_SIZE];
    uint32_t nv_read_size;
    uint32_t nv_read_offset = 0;
    uint32_t bytes_remain = fw_size - SIGNATURE_SIZE;
    uint32_t signature_offset;

    // initialize sha256 context
    sha2_starts(&sha2_ctx, 0);

    // generate digest
    sha2_update(&sha2_ctx, p_metadata->data, p_metadata->len);
    while (bytes_remain)
    {
        nv_read_size = bytes_remain < FW_UPDATE_NVRAM_MAX_READ_SIZE ? bytes_remain : FW_UPDATE_NVRAM_MAX_READ_SIZE;
        wiced_firmware_upgrade_retrieve_from_nv(nv_read_offset, buf, (nv_read_size + 3) & 0xFFFFFFFC);
        sha2_update(&sha2_ctx, buf, nv_read_size);
        nv_read_offset += nv_read_size;
        bytes_remain -= nv_read_size;
    }
    wiced_hal_wdog_restart();
    sha2_finish(&sha2_ctx, digest);

    // signature is at the end of FW image
    signature_offset = nv_read_offset - (nv_read_offset & 0xFFFFFFFC);
    wiced_firmware_upgrade_retrieve_from_nv((nv_read_offset & 0xFFFFFFFC), signature, SIGNATURE_SIZE + 4);

    return ecdsa_verify_(digest, signature + signature_offset, &ecdsa256_public_key);
}
