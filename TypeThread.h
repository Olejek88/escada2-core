//
// Created by Mac on 27/04/2019.
//

#ifndef ESCADA_CORE_TYPETHREAD_H
#define ESCADA_CORE_TYPETHREAD_H


#include <ctime>
#include <cstdint>

class TypeThread {
public:
    char title[100];
    char port[15];
    char device_thread[15];
    uint16_t speed;
//    uint16_t deviceType;
    char deviceType[36]; // uuid
    std::time_t lastDate;
    int status;
    int work;

    int id;

    static uint32_t getAllThreads(TypeThread **dstPtr);
};

#endif //ESCADA_CORE_TYPETHREAD_H
