//
// Created by koputo on 10.01.22.
//

#ifndef ESCADA2_CORE_E18_E18CMDITEM_H
#define ESCADA2_CORE_E18_E18CMDITEM_H


#include <stdint.h>
#include <string>

class E18CmdItem {
public:
    void *data;
    uint32_t dataLen;
    uint8_t cmd;
    uint8_t pin;
    std::string mac;
};


#endif //ESCADA2_CORE_E18_E18CMDITEM_H
