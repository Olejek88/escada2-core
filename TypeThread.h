//
// Created by Mac on 27/04/2019.
//

#ifndef ESCADA_CORE_TYPETHREAD_H
#define ESCADA_CORE_TYPETHREAD_H


#include <ctime>

class TypeThread {
public:
    char title[100];
    char port[15];
    char device_thread[15];
    uint16_t speed;
    char deviceType[15];
    std::time_t lastDate;
    int status;
    int work;

    int id;

    static uint32_t getAllThreads(TypeThread **dstPtr);
};

#endif //ESCADA_CORE_TYPETHREAD_H
