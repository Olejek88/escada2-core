#include <zigbeemtm.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>

// расчитывает frame checksum
uint8_t compute_fcs(uint8_t const *buffer, size_t len) {
    uint16_t i;
    uint16_t sum;

    for (i = 1, sum = 0; i < len - 1; i++) {
        sum = sum ^ buffer[i];
    }

    return (uint8_t) sum;
}

// складывает данные команды в массив
int64_t zigbeemtm_get_cmd_data(uint16_t cmd, void *data, uint8_t *buffer) {
    uint8_t i = 0, j;
    int64_t size = -1;
    void *mt_cmd;
    mtm_cmd_header *header;
    zigbee_mt_cmd_af_data_request *af_data_request;
    zigbee_mt_cmd_af_data_request_ext *af_data_request_ext;
    zigbee_mt_af_register *af_register;
    zigbee_mt_cmd_nwk_addr_req *nwk_addr_req;

    switch (cmd) {
        case ZDO_NWK_ADDR_REQ:
            nwk_addr_req = (zigbee_mt_cmd_nwk_addr_req *) data;
            for (i = 0; i < 8; i++) {
                buffer[i] = nwk_addr_req->ieee_address[i];
            }

            buffer[i++] = nwk_addr_req->req_type;
            buffer[i++] = nwk_addr_req->start_index;
            size = i;
            break;

        case ZB_SYSTEM_RESET:
            size = i;
            break;

        case AF_DATA_REQUEST:
            af_data_request = (zigbee_mt_cmd_af_data_request *) data;
            *(uint16_t *) (&buffer[i]) = af_data_request->dst_addr;
            i += 2;
            buffer[i++] = af_data_request->dep;
            buffer[i++] = af_data_request->sep;
            *(uint16_t *) (&buffer[i]) = af_data_request->cid;
            i += 2;
            buffer[i++] = af_data_request->tid;
            buffer[i++] = af_data_request->opt;
            buffer[i++] = af_data_request->rad;
            buffer[i++] = af_data_request->adl;

            mt_cmd = ((zigbee_mt_cmd_af_data_request *) data)->mt_cmd;
            if (mt_cmd != nullptr) {
                header = (mtm_cmd_header *) mt_cmd;
                size = zigbeemtm_get_mtm_cmd_data(header->type, mt_cmd, &buffer[i]);
                size += i;
            } else {
                size = i;
            }

            break;

        case AF_DATA_REQUEST_EXT:
            af_data_request_ext = (zigbee_mt_cmd_af_data_request_ext *) data;
            buffer[i++] = af_data_request_ext->dst_addr_mode;
            *(uint64_t *) (&buffer[i]) = af_data_request_ext->dst_addr;
            i += 8;
            buffer[i++] = af_data_request_ext->dep;
            *(uint16_t *) (&buffer[i]) = af_data_request_ext->dst_pan_id;
            i += 2;
            buffer[i++] = af_data_request_ext->sep;
            *(uint16_t *) (&buffer[i]) = af_data_request_ext->cid;
            i += 2;
            buffer[i++] = af_data_request_ext->tid;
            buffer[i++] = af_data_request_ext->opt;
            buffer[i++] = af_data_request_ext->rad;
            *(uint16_t *) (&buffer[i]) = af_data_request_ext->adl;
            i += 2;

            mt_cmd = ((zigbee_mt_cmd_af_data_request_ext *) data)->mt_cmd;
            if (mt_cmd != NULL) {
                header = (mtm_cmd_header *) mt_cmd;
                size = zigbeemtm_get_mtm_cmd_data(header->type, mt_cmd, &buffer[i]);
                size += i;
            } else {
                size = i;
            }

            break;

        case AF_REGISTER:
            af_register = (zigbee_mt_af_register *) data;
            buffer[i++] = af_register->ep;
            *(uint16_t *) (&buffer[i]) = af_register->app_prof_id;
            i += 2;
            *(uint16_t *) (&buffer[i]) = af_register->app_device_id;
            i += 2;
            buffer[i++] = af_register->app_device_version;
            buffer[i++] = af_register->latency_req;
            buffer[i++] = af_register->app_num_in_clusters;
            for (j = 0; j < af_register->app_num_in_clusters; j++) {
                *(uint16_t *) (&buffer[i]) = af_register->app_in_cluster_list[j];
                i += 2;
            }

            buffer[i++] = af_register->app_num_out_clusters;
            for (j = 0; j < af_register->app_num_out_clusters; j++) {
                *(uint16_t *) (&buffer[i]) = af_register->app_out_cluster_list[j];
                i += 2;
            }

            size = i;
            break;

        default:
            break;
    }

    return size;
}

// возвращает данные команды в виде массива
int64_t zigbeemtm_get_mtm_cmd_data(uint8_t cmd, void *data, uint8_t *buffer) {
    int16_t i = 0;
    uint8_t j;
    mtm_cmd_config *config;
    mtm_cmd_config_light *config_light;
    mtm_cmd_current_time *current_time;
    mtm_cmd_action *action;

    switch (cmd) {
        case MTM_CMD_TYPE_CONFIG:
            config = (mtm_cmd_config *) data;
            buffer[i++] = config->header.type;
            buffer[i++] = config->header.protoVersion;
            buffer[i++] = config->device;
            *(uint16_t *) (&buffer[i]) = config->min;
            i += 2;
            *(uint16_t *) (&buffer[i]) = config->max;
            i += 2;
            break;

        case MTM_CMD_TYPE_CONFIG_LIGHT:
            config_light = (mtm_cmd_config_light *) data;
            buffer[i++] = config_light->header.type;
            buffer[i++] = config_light->header.protoVersion;
            buffer[i++] = config_light->device;
            for (j = 0; j < MAX_LIGHT_CONFIG; j++) {
                *(uint16_t *) (&buffer[i]) = config_light->config[j].time;
                i += 2;
                *(uint16_t *) (&buffer[i]) = config_light->config[j].value;
                i += 2;
            }

            break;

        case MTM_CMD_TYPE_CURRENT_TIME:
            current_time = (mtm_cmd_current_time *) data;
            buffer[i++] = current_time->header.type;
            buffer[i++] = current_time->header.protoVersion;
            *(uint16_t *) (&buffer[i++]) = current_time->time;
            i++;
            for (int idx = 0; idx < 16; idx++) {
                buffer[i++] = current_time->brightLevel[idx];
            }

            break;

        case MTM_CMD_TYPE_ACTION:
            action = (mtm_cmd_action *) data;
            buffer[i++] = action->header.type;
            buffer[i++] = action->header.protoVersion;
            buffer[i++] = action->device;
            *(uint16_t *) (&buffer[i]) = action->data;
            i += 2;
            break;

        case MTM_CMD_TYPE_CONTACTOR:
            // ни какой полезной нагрузки в этой команде нет
            break;

        default:
            i = -1;
            break;
    }

    return i;
}

// преобразует структуру в массив
uint8_t *zigbeemtm_frame2buffer(zigbee_frame *frame) {
    uint16_t frame_len;
    uint8_t *buffer;
    uint8_t idx = 0;

    frame_len = (uint16_t) (5 + frame->len); // SOF(1), LEN(1), CMD(2), FCS(1), DATA(?)
    buffer = (uint8_t *) malloc(frame_len);

    buffer[idx++] = frame->sof;
    buffer[idx++] = frame->len;

    *(uint16_t *) (&buffer[idx]) = frame->cmd;
    idx += 2;

    // "декодируем" данные команды
    zigbeemtm_get_cmd_data(frame->cmd, frame->zb_cmd, &buffer[idx]);

//    // TODO: добавить флаг для отладки
//    if (true) {
//        for (int i = 0; i < frame_len; i++) {
//            printf("%02x ", buffer[i]);
//        }
//
//        printf("\n");
//    }

    return buffer;
}

// отправляем команду
ssize_t send_cmd(int fd, uint8_t *buffer, size_t size, Kernel *kernel) {
    ssize_t count = 0;
    ssize_t writen;

    if (kernel->isDebug) {
        char pktStr[2048] = {0};
        for (int i = 0; i < size; i++) {
            sprintf(&pktStr[i * 2], "%02X", buffer[i]);
        }

        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] RAW out packet", TAG);
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, pktStr);
    }

    while (count < size) {
        writen = write(fd, &buffer[count], size - count);
        if (writen >= 0) {
            count += writen;
        } else {
            // ошибка записи в порт
            return writen;
        }
    }

    return count;
}

// отправляем zigbee команду
ssize_t send_zb_cmd(int fd, uint16_t cmd, void *data, Kernel *kernel) {
    uint8_t *buffer;
    size_t buffer_size;
    zigbee_frame frame;
    ssize_t rc;
    uint8_t cin, cout;
    zigbee_mt_cmd_af_data_request *request;

    // заполняем данными фрейм zigbee
    frame.sof = SOF;
    frame.fcs = 0;

    switch (cmd) {
        case ZDO_NWK_ADDR_REQ :
            frame.len = 10;
            frame.cmd = ZDO_NWK_ADDR_REQ;
            frame.zb_cmd = data;
            break;

        case ZB_SYSTEM_RESET :
            frame.len = 0;
            frame.cmd = ZB_SYSTEM_RESET;
            frame.zb_cmd = NULL;
            break;

        case AF_REGISTER :
            cin = (uint8_t) (((zigbee_mt_af_register *) data)->app_num_in_clusters * 2);
            cout = (uint8_t) (((zigbee_mt_af_register *) data)->app_num_out_clusters * 2);
            frame.len = (uint8_t) (1 + 2 + 2 + 1 + 1 + 1 + 1 + cin + cout);
            frame.cmd = AF_REGISTER;
            frame.zb_cmd = data;
            break;

        case AF_DATA_REQUEST :
            request = (zigbee_mt_cmd_af_data_request *) data;
            frame.len = 10 + request->adl;
            frame.cmd = AF_DATA_REQUEST;
            frame.zb_cmd = data;
            break;

        case AF_DATA_REQUEST_EXT :
            frame.len = 20;
            frame.cmd = AF_DATA_REQUEST_EXT;
            frame.zb_cmd = data;
            break;

        default:
            break;

    }

    // преобразуем структуры в массив байт
    buffer = zigbeemtm_frame2buffer(&frame);
    buffer_size = frame.len + 5;
    // считаем контрольную сумму
    buffer[buffer_size - 1] = compute_fcs(buffer, buffer_size);
    // пишем полученный буфер в порт
    rc = send_cmd(fd, buffer, buffer_size, kernel);
    free(buffer);

    return rc;
}

// отправляем mtm команду светильнику
ssize_t send_mtm_cmd(int fd, uint16_t short_addr, void *mtm_cmd, Kernel *kernel) {
    uint8_t *buffer;
    size_t buffer_size;
    zigbee_frame frame;
    zigbee_mt_cmd_af_data_request zb_cmd;
    ssize_t rc;
    auto *cmdHeader = (mtm_cmd_header *) mtm_cmd;

    // заполняем данными команду mt_api
    zb_cmd.dst_addr = short_addr;
    zb_cmd.dep = MTM_API_END_POINT;
    zb_cmd.sep = MTM_API_END_POINT;
    zb_cmd.cid = MTM_API_CLUSTER;
    zb_cmd.tid = 0;
    zb_cmd.opt = ZB_O_APS_ACKNOWLEDGE; // флаг для получения подтверждения с конечного устройства а не с первого хопа.
    zb_cmd.rad = MAX_PACKET_HOPS;
    zb_cmd.adl = (uint8_t) get_mtm_command_size(cmdHeader->type, cmdHeader->protoVersion);
    zb_cmd.mt_cmd = mtm_cmd;

    // заполняем данными фрейм zigbee
    frame.sof = SOF;
    frame.len = (uint8_t) 10 + zb_cmd.adl; // длина zigbee пакета + длина mtm пакета
    frame.cmd = AF_DATA_REQUEST;
    frame.zb_cmd = &zb_cmd;
    frame.fcs = 0;

    buffer = zigbeemtm_frame2buffer(&frame);
    buffer_size = frame.len + 5;
    buffer[buffer_size - 1] = compute_fcs(buffer, buffer_size);

    rc = send_cmd(fd, buffer, buffer_size, kernel);
    free(buffer);

    return rc;
}

// отправляем mtm команду светильнику
ssize_t send_mtm_cmd_ext(int fd, uint64_t addr, void *mtm_cmd, Kernel *kernel) {
    uint8_t *buffer;
    size_t buffer_size;
    zigbee_frame frame;
    zigbee_mt_cmd_af_data_request_ext zb_cmd;
    ssize_t rc;
    auto *cmdHeader = (mtm_cmd_header *) mtm_cmd;

    // заполняем данными команду mt_api
    zb_cmd.dst_addr_mode = 3; // 8 byte
    zb_cmd.dst_addr = addr;
    zb_cmd.dep = MTM_API_END_POINT;
    zb_cmd.dst_pan_id = 0x0000;
    zb_cmd.sep = MTM_API_END_POINT;
    zb_cmd.cid = MTM_API_CLUSTER;
    zb_cmd.tid = 0;
    zb_cmd.opt = ZB_O_APS_ACKNOWLEDGE; // флаг для получения подтверждения с конечного устройства а не с первого хопа.
    zb_cmd.rad = MAX_PACKET_HOPS;
    zb_cmd.adl = (uint16_t) get_mtm_command_size(cmdHeader->type, cmdHeader->protoVersion);
    zb_cmd.mt_cmd = mtm_cmd;

    // заполняем данными фрейм zigbee
    frame.sof = SOF;
    frame.len = (uint8_t) (20 + zb_cmd.adl); // длина zigbee пакета + длина mtm пакета
    frame.cmd = AF_DATA_REQUEST_EXT;
    frame.zb_cmd = &zb_cmd;
    frame.fcs = 0;

    buffer = zigbeemtm_frame2buffer(&frame);
    buffer_size = frame.len + 5;
    buffer[buffer_size - 1] = compute_fcs(buffer, buffer_size);

    rc = send_cmd(fd, buffer, buffer_size, kernel);
    free(buffer);

    return rc;
}

int8_t get_mtm_status_data_start(uint8_t protoVersion) {
    int8_t size = 0;
    mtm_cmd_header header;
    mtm_cmd_status status;
    mtm_cmd_status_v1 statusV1;

    size += sizeof(header.type);
    size += sizeof(header.protoVersion);

    size += sizeof(status.mac);
    if (protoVersion == MTM_VERSION_1) {
        size += sizeof(statusV1.parentMac);
    }

    size += sizeof(status.alert.devices);

    return size;
}

int8_t get_mtm_command_size(uint8_t type, uint8_t protoVersion) {
    int8_t size = 0;
    mtm_cmd_header header;
    mtm_cmd_status status;
    mtm_cmd_status_v1 statusV1;
    mtm_cmd_config config;
    mtm_cmd_config_light config_light;
    mtm_cmd_current_time current_time;
    mtm_cmd_action action;

    size += sizeof(header.type);
    size += sizeof(header.protoVersion);

    switch (type) {
        case MTM_CMD_TYPE_STATUS:
            size += sizeof(status.mac);
            if (protoVersion == MTM_VERSION_1) {
                size += sizeof(statusV1.parentMac);
            }

            size += sizeof(status.alert.devices);
            size += sizeof(status.data);
            break;
        case MTM_CMD_TYPE_CONFIG:
            size += sizeof(config.device);
            size += sizeof(config.min);
            size += sizeof(config.max);
            break;
        case MTM_CMD_TYPE_CONFIG_LIGHT:
            size += sizeof(config_light.device);
            size += (sizeof(config_light.config->time) + sizeof(config_light.config->value)) * MAX_LIGHT_CONFIG;
            break;
        case MTM_CMD_TYPE_CURRENT_TIME:
            size += sizeof(current_time.time);
            size += sizeof(current_time.brightLevel);
            break;
        case MTM_CMD_TYPE_ACTION:
            size += sizeof(action.device);
            size += sizeof(action.data);
            break;
        default:
            size = -1;
            break;
    }

    return size;
}


void print_buffer_hex(uint8_t *buffer, size_t buffer_size) {
    for (int i = 0; i < buffer_size; i++) {
        printf("0x%02x, ", buffer[i]);
    }
}