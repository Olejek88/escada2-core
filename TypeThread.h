//
// Created by Mac on 27/04/2019.
//

#ifndef ESCADA_CORE_TYPETHREAD_H
#define ESCADA_CORE_TYPETHREAD_H


#include <ctime>

class TypeThread {
public:
    std::string title;
    std::string port;
    std::string device_thread;
    std::string speed;
    std::string deviceType;
    std::time_t lastDate;

    int id;

    static TypeThread* getAllThreads ();
};

#endif //ESCADA_CORE_TYPETHREAD_H
