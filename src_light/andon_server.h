

#define DEF_ANDON_GATT 

#ifdef DEF_ANDON_GATT

#define UUID_ANDON_SERVICE                    'c','o','m','.','j','i','u','a','n','.','S','L','B','V','0','1'
#define UUID_ANDON_CHARACTERISTIC_NOTIFY      'r','e','c','.','j','i','u','a','n','.','S','L','B','V','0','1'
#define UUID_ANDON_CHARACTERISTIC_WRITE       's','e','n','.','j','i','u','a','n','.','S','L','B','V','0','1'
#define HANDLE_ANDON_SERVICE                  0x200
#define HANDLE_ANDON_SERVICE_CHAR_NOTIFY      0x201
#define HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL  0x202
#define HANDLE_ANDON_SERVICE_CHAR_CFG_DESC    0x203
#define HANDLE_ANDON_SERVICE_WRITE            0x204
#define HANDLE_ANDON_SERVICE_WRITE_VAL        0x205

#define DEF_ANDON_OTA
#ifdef DEF_ANDON_OTA

#define UUID_ANDON_OTA_FW_UPGRADE_SERVICE                            'c','o','m','.','j','i','u','a','n','.','U','P','G','R','A','D'
#define UUID_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT       'c','t','r','.','j','i','u','a','n','.','U','P','G','R','A','D'
#define UUID_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_DATA                'd','a','t','.','j','i','u','a','n','.','U','P','G','R','A','D'
/* GATT handles used for by the FW upgrade service  此handle值的最低8bit必须与HANDLE_OTA_FW_UPGRADE_SERVICE中对应的handle值的最低8bit保持一致*/
#define HANDLE_ANDON_OTA_FW_UPGRADE_SERVICE                           0x210 /* OTA firmware upgrade service handle */
#define HANDLE_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT      0x211 /* OTA firmware upgrade characteristic control point handle */
#define HANDLE_ANDON_OTA_FW_UPGRADE_CONTROL_POINT                     0x212 /* OTA firmware upgrade control point value handle */
#define HANDLE_ANDON_OTA_FW_UPGRADE_CLIENT_CONFIGURATION_DESCRIPTOR   0x213 /* OTA firmware upgrade client configuration description handle */
#define HANDLE_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_DATA               0x214 /* OTA firmware upgrade data characteristic handle */
#define HANDLE_ANDON_OTA_FW_UPGRADE_DATA                              0x215 /* OTA firmware upgrade data value handle */
#define HANDLE_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_APP_INFO           0x216 /* OTA firmware upgrade app info characteristic handle */
#define HANDLE_ANDON_OTA_FW_UPGRADE_APP_INFO                          0x217 /* OTA firmware upgrade app info value handle */

#endif

void andonServerSendLightStatus(void);
void andonServerSendResetStatus(void);
void andonServerSendLightCountDownStatus(void);
void AndonServiceGattDisConnect(void);
wiced_bt_gatt_status_t AndonServerReadHandle(uint16_t conn_id, wiced_bt_gatt_read_t * p_read_data);
wiced_bt_gatt_status_t AndonServerWriteHandle(uint16_t conn_id, wiced_bt_gatt_write_t * p_data);

#ifdef DEF_ANDON_OTA
void* AndonServiceUpgradeEncrypt(uint8_t *indata, uint16_t inlen, uint16_t *outlen);
void* AndonServiceUpgradeDecrypt(uint8_t *indata, uint16_t inlen, uint16_t *outlen);

#endif

#endif


