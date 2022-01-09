//
// Created by koputo on 08.01.22.
//

#ifndef ESCADA2_CORE_E18_IZIGBEEMODULE_H
#define ESCADA2_CORE_E18_IZIGBEEMODULE_H


#include <dbase.h>
#include <kernel.h>
#include <TypeThread.h>

class IZigbeeModule {
public:
    virtual ~IZigbeeModule() = default;

    virtual void *moduleThread(TypeThread *pth) = 0;
};


#endif //ESCADA2_CORE_E18_IZIGBEEMODULE_H
