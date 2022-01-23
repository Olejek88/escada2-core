#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <kernel.h>

#ifndef ZIGBEEMTM_H
#define ZIGBEEMTM_H

// начало фрейма
#define SOF 0xFE

#define MTM_VERSION_0 0x00
#define MTM_VERSION_1 0x01

#define ZDO_NWK_ADDR_REQ 0x0025
#define AF_DATA_REQUEST 0x0124
#define AF_DATA_REQUEST_EXT 0x0224
#define ZB_SYSTEM_RESET 0x0946
#define AF_INCOMING_MSG 0x8144
#define AF_INCOMING_MSG_EXT 0x8244
#define AF_REGISTER 0x0024
#define AF_DATA_RESPONSE 0x8148

#define MBEE_API_END_POINT 0xE8
#define MTM_API_END_POINT 0xE9
#define MBEE_API_GET_TEMP_CLUSTER 0x0209
#define MBEE_API_UART_CLUSTER 0x0107
#define MTM_API_CLUSTER 0xFC00
#define MTM_PROFILE_ID 0xC000

#define MBEE_API_LOCAL_IOSTATUS_CLUSTER 0x0103
#define MBEE_API_LOCAL_MODULE_VERSION_CLUSTER 0x0100

#define MAX_LIGHT_CONFIG 5

#define MAX_PACKET_HOPS 50

#define ZB_O_SKIP_ROUTING           0b10000000
#define ZB_O_APS_SECURITY           0b01000000
#define ZB_O_APS_DISCOVERY_ROUTE    0b00100000
#define ZB_O_APS_ACKNOWLEDGE        0b00010000

#define NO_LATENCY 0x00
#define FAST_BEACONS 0x01
#define SLOW_BEACONS 0x02

// типы пакетов протокола MTM
enum {
    MTM_CMD_TYPE_STATUS = 0x01,
    MTM_CMD_TYPE_CONFIG,
    MTM_CMD_TYPE_CONFIG_LIGHT,
    MTM_CMD_TYPE_CURRENT_TIME,
    MTM_CMD_TYPE_ACTION,
    MTM_CMD_TYPE_CONTACTOR,
    MTM_CMD_TYPE_RESET_COORDINATOR,
    MTM_CMD_TYPE_CLEAR_NETWORK,
    MTM_CMD_TYPE_GET_STATUS,
    MTM_CMD_TYPE_BEACON,
};

#define MBEE_API_DIGITAL_LINE7 0x0007

#define MTM_DEVICE_LIGHT 0x0000
#define MTM_DEVICE_RSSI 0x0001
#define MTM_SENSOR_02 0x0002
#define MTM_SENSOR_CO2 MTM_SENSOR_02

// команда mt
//typedef struct _zigbee_mt_cmd {
//    uint8_t len;
//    uint16_t cmd;
//    uint8_t *data;
//} zigbee_mt_cmd;

// фрейм zigbee
//typedef struct _zigbee_frame {
//    uint8_t sof;
//    zigbee_mt_cmd mt_cmd;
//    uint8_t fcs;
//} zigbee_frame;

typedef struct _zigbee_frame {
    uint8_t sof;
    uint8_t len;
    uint16_t cmd;
    void *zb_cmd; // указатель на структуру типа zigbee_mt_cmd_*
    uint8_t fcs;
} zigbee_frame;

typedef struct _zigbee_mt_cmd_nwk_addr_req {
    uint8_t ieee_address[8];
    uint8_t req_type;
    uint8_t start_index;
} zigbee_mt_cmd_nwk_addr_req;

typedef struct _zigbee_mt_cmd_af_data_request {
    uint16_t dst_addr;
    uint8_t dep;
    uint8_t sep;
    uint16_t cid;
    uint8_t tid;
    uint8_t opt;
    uint8_t rad;
    uint8_t adl;
    void *mt_cmd; // указатель на структуру типа mtm_cmd_*
} zigbee_mt_cmd_af_data_request;

typedef struct _zigbee_mt_cmd_af_data_request_ext {
    uint8_t dst_addr_mode;
    uint64_t dst_addr;
    uint8_t dep;
    uint16_t dst_pan_id;
    uint8_t sep;
    uint16_t cid;
    uint8_t tid;
    uint8_t opt;
    uint8_t rad;
    uint16_t adl;
    void *mt_cmd; // указатель на структуру типа mtm_cmd_*
} zigbee_mt_cmd_af_data_request_ext;

typedef struct _zigbee_mt_af_incoming_msg {
    uint16_t group_id;
    uint16_t cid;
    uint16_t src_addr;
    uint8_t sep;
    uint8_t dep;
    uint8_t broadcast;
    uint8_t link_quality;
    uint8_t sequrity;
    uint32_t timestamp;
    uint8_t tid;
    uint8_t adl;
    void *mt_cmd; // указатель на структуру типа mtm_cmd_*
} zigbee_mt_af_incoming_msg;

typedef struct _zigbee_mt_af_incoming_msg_ext {
    uint16_t group_id;
    uint16_t cid;
    uint8_t src_addr_mode;
    uint8_t src_addr[8];
    uint8_t sep;
    uint16_t src_pan_id;
    uint8_t dep;
    uint8_t broadcast;
    uint8_t link_quality;
    uint8_t sequrity;
    uint32_t timestamp;
    uint8_t tid;
    uint8_t adl;
    void *mt_cmd; // указатель на структуру типа mtm_cmd_*
} zigbee_mt_af_incoming_msg_ext;

typedef struct _zigbee_mt_af_register {
    uint8_t ep;
    uint16_t app_prof_id;
    uint16_t app_device_id;
    uint8_t app_device_version;
    uint8_t latency_req;
    uint8_t app_num_in_clusters;
    uint16_t app_in_cluster_list[16];
    uint8_t app_num_out_clusters;
    uint16_t app_out_cluster_list[16];
} zigbee_mt_af_register;

// заголовок протокола МТМ
typedef struct _mtm_cmd_header {
    uint8_t type;
    uint8_t protoVersion;
} mtm_cmd_header;

// общая структура команды MTM
typedef struct _mtm_cmd {
    mtm_cmd_header header;
    void *data;
} mtm_cmd;

// флаги аварии для конечных устройств/датчиков
typedef union _mtm_device_alert {
    uint16_t devices;
    struct {
        bool device0:1;
        bool device1:1;
        bool device2:1;
        bool device3:1;
        bool device4:1;
        bool device5:1;
        bool device6:1;
        bool device7:1;
        bool device8:1;
        bool device9:1;
        bool device10:1;
        bool device11:1;
        bool device12:1;
        bool device13:1;
        bool device14:1;
        bool device15:1;
    };
} mtm_device_alert;

// структура пакета МТМ "Статус"
typedef struct _mtm_cmd_status {
    mtm_cmd_header header;
    uint8_t mac[8];
    mtm_device_alert alert;
    uint16_t data[16];
} mtm_cmd_status;

typedef struct _mtm_cmd_status_v1 {
    mtm_cmd_header header;
    uint8_t mac[8];
    uint8_t parentMac[8];
    mtm_device_alert alert;
    uint16_t data[16];
} mtm_cmd_status_v1;

// структура пакета МТМ "Конфигурация датчика"
typedef struct _mtm_cmd_config {
    mtm_cmd_header header;
    uint8_t device;
    uint16_t min;
    uint16_t max;
} mtm_cmd_config;

// конфигурация освещения светильника
typedef struct _mtm_config_light {
    uint16_t time;
    uint16_t value;
} mtm_config_light;

// структура пакета МТМ "Кофигурация режима освещения"
typedef struct _mtm_cmd_config_light {
    mtm_cmd_header header;
    uint8_t device;
    mtm_config_light config[MAX_LIGHT_CONFIG];
} mtm_cmd_config_light;

// структура пакета МТМ "Текущее время"
typedef struct _mtm_cmd_current_time {
    mtm_cmd_header header;
    uint16_t time;
    uint8_t brightLevel[16];
} mtm_cmd_current_time;

// структура пакета МТМ "Команда исполнительному устройству"
typedef struct _mtm_cmd_action {
    mtm_cmd_header header;
    uint8_t device;
    uint16_t data;
} mtm_cmd_action;

// структура пакета МТМ "Команда управления контактором"
typedef struct _mtm_cmd_contactor {
    mtm_cmd_header header;
} mtm_cmd_contactor;

// структура пакета МТМ "Команда сброса координатора"
typedef struct _mtm_cmd_reset_coordinator {
    mtm_cmd_header header;
} mtm_cmd_reset_coordinator;

// структура пакета МТМ "Команда сброса сети Zigbee"
typedef struct _mtm_cmd_clear_network {
    mtm_cmd_header header;
} mtm_cmd_clear_network;

// структура пакета МТМ "Запрос статуса модуля"
typedef struct _mtm_cmd_get_status {
    mtm_cmd_header header;
} mtm_cmd_get_status;

// структура пакета МТМ "Маяк"
typedef struct _mtm_cmd_beacon {
    mtm_cmd_header header;
    uint8_t mac[8];
    uint8_t parentMac[8];
    uint16_t shortAddr;
} mtm_cmd_beacon;

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t *TAG;

uint8_t compute_fcs(uint8_t const *buffer, size_t len);

uint8_t *zigbeemtm_frame2buffer(zigbee_frame *frame);

int64_t zigbeemtm_get_cmd_data(uint16_t cmd, void *data, uint8_t *buffer);

int64_t zigbeemtm_get_mtm_cmd_data(uint8_t cmd, void *data, uint8_t *buffer);

ssize_t send_cmd(int fd, uint8_t *buffer, size_t size, Kernel *kernel);

int8_t get_mtm_command_size(uint8_t type, uint8_t protoVersion);

int8_t get_mtm_status_data_start(uint8_t protoVersion);

ssize_t send_mtm_cmd(int fd, uint16_t short_addr, void *cmd, Kernel *kernel);

ssize_t send_zb_cmd(int fd, uint16_t cmd, void *data, Kernel *kernel);

ssize_t send_mtm_cmd_ext(int fd, uint64_t addr, void *mtm_cmd, Kernel *kernel);

#ifdef __cplusplus
}
#endif

#endif //ZIGBEEMTM_H
