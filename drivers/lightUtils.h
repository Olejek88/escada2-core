#ifndef ESCADA_CORE_LIGHTUTILS_H
#define ESCADA_CORE_LIGHTUTILS_H

#include <map>
#include <kernel.h>
#include "DeviceProgram.h"
#include "dbase.h"

#define groupsMap std::map<int/*gid*/, std::map<std::string/*date ?*/, std::map<int/*type*/, std::map<std::string/*prog, time*/, std::string>>>>

extern std::map<uint8_t, uint8_t> lightGroupBright;

std::map<std::string, std::map<int, time_t>> *getDefaultAstroEvents(time_t currentTime, double lon, double lat);

std::map<std::string, DeviceProgram> *getUsedPrograms(DBase *dBase, std::map<std::string, std::string> *programUuids);

void getNodeAstroEvents(DBase *dBase, std::map<std::string, std::map<int, time_t>> *defAstroEvents);

void recalcAstroEvents(std::map<std::string, std::map<int, time_t>> *defAstroEvents);

void fillGroupsDefValues(groupsMap *groups, std::map<std::string, std::map<int, time_t>> *defAstroEvents);

void checkLightProgram(DBase *dBase, time_t currentTime, double lon, double lat);

void makeLostLightList(DBase *dBase, Kernel *kernel);

#endif //ESCADA_CORE_LIGHTUTILS_H
