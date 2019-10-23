#ifndef ESCADA_CORE_DEVICEPROGRAM_H
#define ESCADA_CORE_DEVICEPROGRAM_H

#include <iostream>

class DeviceProgram {
public:
    std::string uuid;
    std::string title;
    std::string period_title1;
    uint8_t value1;
    std::string period_title2;
    time_t time2;
    uint8_t value2;
    std::string period_title3;
    time_t time3;
    uint8_t value3;
    std::string period_title4;
    time_t time4;
    uint8_t value4;
    std::string period_title5;
    uint8_t value5;
};


#endif //ESCADA_CORE_DEVICEPROGRAM_H
