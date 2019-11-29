#ifndef ESCADA_CORE_SENSOR_CHANNEL_H
#define ESCADA_CORE_SENSOR_CHANNEL_H

#include <iostream>

class SensorChannel {
public:
    int32_t _id;
    std::string uuid;
    std::string title;
    int32_t reg;
    std::string deviceUuid;
    std::string measureTypeUuid;
    std::string createdAt;
    std::string changedAt;
};


#endif //ESCADA_CORE_SENSOR_CHANNEL_H
