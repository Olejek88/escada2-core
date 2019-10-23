#ifndef ESCADA_CORE_LIGHTUTILS_H
#define ESCADA_CORE_LIGHTUTILS_H

#include <map>
#include "DeviceProgram.h"
#include "dbase.h"

#define groupsMap std::map<int/*gid*/, std::map<std::string/*date ?*/, std::map<int/*type*/, std::map<std::string/*prog, time*/, std::string>>>>

// массив в который складируем текущие уровни яркости
std::map<uint8_t, uint8_t> lightGroupBright = {
        {0,  0},
        {1,  0},
        {2,  0},
        {3,  0},
        {4,  0},
        {5,  0},
        {6,  0},
        {7,  0},
        {8,  0},
        {9,  0},
        {10, 0},
        {11, 0},
        {12, 0},
        {13, 0},
        {14, 0},
        {15, 0},
};

std::map<std::string, std::map<int, time_t>> *getDefaultAstroEvents(time_t currentTime, double lon, double lat);

std::map<std::string, DeviceProgram> *getUsedPrograms(DBase *dBase, std::map<std::string, std::string> *programUuids);

void getNodeAstroEvents(DBase *dBase, std::map<std::string, std::map<int, time_t>> *defAstroEvents);

void recalcAstroEvents(std::map<std::string, std::map<int, time_t>> *defAstroEvents);

void fillGroupsDefValues(groupsMap *groups, std::map<std::string, std::map<int, time_t>> *defAstroEvents);

void checkLightProgram(DBase *dBase, time_t currentTime, double lon, double lat, int32_t threadId);

#endif //ESCADA_CORE_LIGHTUTILS_H
