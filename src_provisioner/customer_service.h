#ifndef __CUSTOMER_SERVICE_H__
#define __CUSTOMER_SERVICE_H__

#include "wiced_bt_gatt.h"

// Customer 128 bit uuid service
#define UUID_CUSTOMER_SERVICE                              0x08, 0x08, 0x08, 0x41, 0x50, 0x02, 0x80, 0xb6, 0xe8, 0x11, 0xf5, 0xcc, 0x22, 0x29, 0x02, 0x10
#define UUID_CUSTOMER_SERVICE_CHARACTERISTIC_NOTIFY        0x09, 0x09, 0x09, 0x08, 0x0a, 0x57, 0x8e, 0x83, 0x99, 0x4e, 0xa7, 0xf7, 0xbf, 0x50, 0xdd, 0xa3
#define UUID_CUSTOMER_SERVICE_CHARACTERISTIC_WRITE         0x10, 0x10, 0x10, 0x41, 0x50, 0x02, 0x81, 0x9b, 0xe8, 0x11, 0xf5, 0xcc, 0xc8, 0x63, 0x71, 0x10

#define HANDLE_CUSTOMER_SERVICE                                   0x8000
#define HANDLE_CUSTOMER_SERVICE_CHARACTERISTIC_NOTIFY             0x8001
#define HANDLE_CUSTOMER_SERVICE_CHARACTERISTIC_NOTIFY_VALUE       0x8002
#define HANDLE_CUSTOMER_SERVICE_CONFIGURATION_DESCRIPTOR          0x8003
#define HANDLE_CUSTOMER_SERVICE_CHARACTERISTIC_WRITE              0x8004
#define HANDLE_CUSTOMER_SERVICE_CHARACTERISTIC_WRITE_VALUE        0x8005

void customer_service_set_client_configuration(uint16_t client_config);

wiced_bt_gatt_status_t wiced_customer_service_read_handler(uint16_t conn_id, wiced_bt_gatt_read_t * p_read_data);

wiced_bt_gatt_status_t wiced_customer_service_write_handler(uint16_t conn_id, wiced_bt_gatt_write_t *p_write_data);

wiced_bt_gatt_status_t wiced_customer_service_indication_cfm_handler(uint16_t conn_id, uint16_t handle);

wiced_bool_t wiced_customer_service_is_gatt_handle(uint16_t handle);


#endif // __CUSTOMER_SERVICE_H__
