#ifndef ESCADA_CORE_DEVICE_H
#define ESCADA_CORE_DEVICE_H

#include <iostream>

class Device {
public:
    int32_t _id;
    std::string uuid;
    std::string address;
    std::string name;
    std::string deviceTypeUuid;
    std::string serial;
    std::string port;
    int32_t interface;
    std::string deviceStatusUuid;
    std::string last_date;
    std::string nodeUuid;
    int32_t linkTimeout;
    int32_t q_att;
    uint32_t q_errors;
    std::string dev_time;
    uint32_t protocol;
    std::string number;
    std::string createdAt;
    std::string changedAt;
};


#endif //ESCADA_CORE_DEVICE_H
