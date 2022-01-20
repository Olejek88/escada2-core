//
// Created by koputo on 05.01.22.
//

#ifndef ESCADA2_CORE_E18_ENTITYPARAMETER_H
#define ESCADA2_CORE_E18_ENTITYPARAMETER_H

#include <iostream>
#include <dbase.h>

class EntityParameter {
public:
    EntityParameter(DBase *dBase) {
        this->dBase = dBase;
    }

    bool loadParameter(const std::string &uuid, const std::string &parameter);

    bool save();

public:
    DBase *dBase;
    std::int64_t _id;
    std::string uuid;
    std::string entityUuid;
    std::string parameter;
    std::string value;
    std::string createdAt;
    std::string changedAt;
    bool isNew = true;
};


#endif //ESCADA2_CORE_E18_ENTITYPARAMETER_H
