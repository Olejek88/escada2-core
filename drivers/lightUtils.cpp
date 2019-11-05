#include <mysql/mysql.h>
#include "lightUtils.h"
#include "dbase.h"
#include <map>
#include "suninfo.h"
#include <iostream>
#include <ctime>
#include "kernel.h"
#include "MtmZigbee.h"
#include "function.h"
#include "DeviceProgram.h"

extern Kernel *kernel;
extern std::string coordinatorUuid;
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

std::map<std::string, std::map<int, time_t>> *getDefaultAstroEvents(time_t currentTime, double lon, double lat) {
    // получаем три даты для выборки
    char *tmpStr = new char[11];

    std::tm tmpTm = {0};
    std::map<int, std::string> actualDates;
    localtime_r(&currentTime, &tmpTm);
    // вчера
    tmpTm.tm_mday--;
    std::strftime(tmpStr, 11, "%F", &tmpTm);
    actualDates.insert({0, tmpStr});
    // сегодня
    tmpTm.tm_mday++;
    std::strftime(tmpStr, 11, "%F", &tmpTm);
    actualDates.insert({1, tmpStr});
    // завтра
    tmpTm.tm_mday++;
    std::strftime(tmpStr, 11, "%F", &tmpTm);
    actualDates.insert({2, tmpStr});

    auto *defaultAstroEvents = new std::map<std::string, std::map<int, time_t>>;

    // зарание строим массив с расчитанными значениями, которые далее будем переопределять данными полученными из базы
    localtime_r(&currentTime, &tmpTm);
    // вчерашняя дата
    tmpTm.tm_mday--;
    time_t tmpTime = std::mktime(&tmpTm);

    auto it = actualDates.begin();
    while (it != actualDates.end()) {
        auto second = it->second;
        double r, s;
        double twb, twe;

        localtime_r(&tmpTime, &tmpTm);

        sun_rise_set(tmpTm.tm_year + 1900, tmpTm.tm_mon + 1, tmpTm.tm_mday, lon, lat, &r, &s);
        fillTimeStruct(r, &tmpTm);
        (*defaultAstroEvents)[second][0] = mktime(&tmpTm);
        fillTimeStruct(s, &tmpTm);
        (*defaultAstroEvents)[second][1] = mktime(&tmpTm);

        civil_twilight(tmpTm.tm_year + 1900, tmpTm.tm_mon + 1, tmpTm.tm_mday, lon, lat, &twb, &twe);
        fillTimeStruct(twb, &tmpTm);
        (*defaultAstroEvents)[second][2] = mktime(&tmpTm);
        fillTimeStruct(twe, &tmpTm);
        (*defaultAstroEvents)[second][3] = mktime(&tmpTm);

        tmpTm.tm_mday++;
        tmpTime = std::mktime(&tmpTm);
        it++;
    }

    it = actualDates.begin();
    auto next = actualDates.begin();
    next++;
    while (next != actualDates.end()) {
        auto prevDay = it->second;
        auto currDay = next->second;
        uint16_t nightLenght;
        uint16_t twilightLength;

        // длительность ночи восход сегодня - закат вчера
        nightLenght = (*defaultAstroEvents)[currDay][0] - (*defaultAstroEvents)[prevDay][1];
        // длительность ночи со вчера на сегодня
        (*defaultAstroEvents)[prevDay][4] = nightLenght;
        // длительность вчерашних сумерек конец сумерек - закат
        twilightLength = (*defaultAstroEvents)[prevDay][3] - (*defaultAstroEvents)[prevDay][1];
        // длительность вчерашних сумерек
        (*defaultAstroEvents)[prevDay][5] = twilightLength;

        it++;
        next++;
    }

    return defaultAstroEvents;
}

std::map<std::string, DeviceProgram> *getUsedPrograms(DBase *dBase, std::map<std::string, std::string> *programUuids) {
    auto *result = new std::map<std::string, DeviceProgram>;
    // выбираем программы из базы и складываем их в список в виде объектов
    std::string progList;
    auto prgUuidsIt = programUuids->begin();
    bool firstTime = true;
    while (prgUuidsIt != programUuids->end()) {
        if (firstTime) {
            firstTime = false;
            progList += "'" + prgUuidsIt->first + "'";
        } else {
            progList += ", '" + prgUuidsIt->first + "'";
        }

        prgUuidsIt++;
    }

    auto query = std::string("SELECT * FROM device_program WHERE uuid IN (" + progList + ")");
    MYSQL_ROW row;
    auto res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        char *tmpData;
        char *tmpDataEnd;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            DeviceProgram program;
            program.uuid = dBase->getFieldValue(row, "uuid");
            program.title = dBase->getFieldValue(row, "title");

            program.period_title1 = dBase->getFieldValue(row, "period_title1");
            program.value1 = std::stoi(dBase->getFieldValue(row, "value1"));

            program.period_title2 = dBase->getFieldValue(row, "period_title2");
            tmpData = dBase->getFieldValue(row, "time2");
            tmpDataEnd = &tmpData[strlen(tmpData)];
            program.time2 = std::strtoll(tmpData, &tmpDataEnd, 10);
            program.value2 = std::stoi(dBase->getFieldValue(row, "value2"));

            program.period_title3 = dBase->getFieldValue(row, "period_title3");
            tmpData = dBase->getFieldValue(row, "time3");
            tmpDataEnd = &tmpData[strlen(tmpData)];
            program.time3 = std::strtoll(tmpData, &tmpDataEnd, 10);
            program.value3 = std::stoi(dBase->getFieldValue(row, "value3"));

            program.period_title4 = dBase->getFieldValue(row, "period_title4");
            tmpData = dBase->getFieldValue(row, "time4");
            tmpDataEnd = &tmpData[strlen(tmpData)];
            program.time4 = std::strtoll(tmpData, &tmpDataEnd, 10);
            program.value4 = std::stoi(dBase->getFieldValue(row, "value4"));

            program.period_title5 = dBase->getFieldValue(row, "period_title5");
            program.value5 = std::stoi(dBase->getFieldValue(row, "value5"));

            (*result)[program.uuid] = program;
        }

        mysql_free_result(res);
    }

    return result;
}

void getNodeAstroEvents(DBase *dBase, std::map<std::string, std::map<int, time_t>> *defAstroEvents) {
    MYSQL_RES *res;
    MYSQL_ROW row;
    auto daeIt = defAstroEvents->begin();
    std::string query;
    query =
            "SELECT DATE(nct.date) AS date, TIME(nct.date) AS time, type, UNIX_TIMESTAMP(nct.date) AS datetime FROM node_control AS nct "
            "WHERE DATE(nct.date) IN ('" + (daeIt++)->first + "', '" + (daeIt++)->first + "', '" + (daeIt++)->first +
            "')";
    res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        // обновляем данные в массиве
        while ((row = mysql_fetch_row(res)) != nullptr) {
            char *tmpData;
            std::string date;
            time_t time = 0;
            int type;

            tmpData = row[dBase->getFieldIndex("date")];
            if (tmpData == nullptr) {
                date.clear();
            } else {
                date = tmpData;
            }

            tmpData = row[dBase->getFieldIndex("datetime")];
            if (tmpData == nullptr) {
                time = -1;
            } else {
                char *endPtr = &tmpData[strlen(tmpData)];
                time = std::strtoll(tmpData, &endPtr, 10);
            }

            tmpData = row[dBase->getFieldIndex("type")];
            if (tmpData == nullptr) {
                type = -1;
            } else {
                type = std::stoi(tmpData);
            }

            if (!date.empty() && (type == 0 || type == 1) && time > -1) {
                (*defAstroEvents)[date][type] = time;
            }
        }

        mysql_free_result(res);
    }
}

void recalcAstroEvents(std::map<std::string, std::map<int, time_t>> *defAstroEvents) {
    auto prevDayIt = defAstroEvents->begin();
    auto currDayIt = defAstroEvents->begin();
    currDayIt++;
    while (currDayIt != defAstroEvents->end()) {
        auto prevDay = prevDayIt->first;
        auto currDay = currDayIt->first;
        uint32_t nightLenght;
        uint32_t twilightLength;

        // длительность ночи = восход сегодня - закат вчера
        nightLenght = (*defAstroEvents)[currDay][0] - (*defAstroEvents)[prevDay][1];
        double K = (double) nightLenght / (*defAstroEvents)[prevDay][4];
        // длительность ночи со вчера на сегодня
        (*defAstroEvents)[prevDay][4] = nightLenght;
        // длительность вчерашних сумерек = конец сумерек - закат
        twilightLength = (uint32_t) ((*defAstroEvents)[prevDay][5] * K);
        // длительность вчерашних сумерек
        (*defAstroEvents)[prevDay][5] = twilightLength;

        // пересчитываем время наступления сумерек
        // начало сумерек
        (*defAstroEvents)[prevDay][2] = (*defAstroEvents)[prevDay][0] - twilightLength;
        // конец сумерек
        (*defAstroEvents)[prevDay][3] = (*defAstroEvents)[prevDay][1] + twilightLength;

        prevDayIt++;
        currDayIt++;
    }
}

void fillGroupsDefValues(groupsMap *groups, std::map<std::string, std::map<int, time_t>> *defAstroEvents) {
    // т.к. у нас 16 групп, создаём для них данные по умолчанию, реальные астрособытия
    // (это не совсем так, реальные астрособытия уже изменены данными из календаря шкафа, так и должно быть)
    for (int i = 0; i < 16; i++) {
        auto daeIt = defAstroEvents->begin();
        while (daeIt != defAstroEvents->end()) {
            auto first = daeIt->first;
            auto second = daeIt->second;
            int type;
            time_t time;

            // восход
            type = second.find(0)->first;
            time = second.find(0)->second;
            (*groups)[i][first][type]["time"] = std::to_string(time);
            // закат
            type = second.find(1)->first;
            time = second.find(1)->second;
            (*groups)[i][first][type]["time"] = std::to_string(time);
//            // начало сумерек
//            type = second.find(2)->first;
//            time = second.find(2)->second;
//            (*groups)[i][first][type]["time"] = std::to_string(time);
//            // конец сумерек
//            type = second.find(3)->first;
//            time = second.find(3)->second;
//            (*groups)[i][first][type]["time"] = std::to_string(time);
            // длительность ночи
            type = second.find(4)->first;
            time = second.find(4)->second;
            (*groups)[i][first][type]["nLength"] = std::to_string(time);
            // длительность сумерек
            type = second.find(5)->first;
            time = second.find(5)->second;
            (*groups)[i][first][type]["twLength"] = std::to_string(time);

            daeIt++;
        }
    }
}

void checkLightProgram(DBase *dBase, time_t currentTime, double lon, double lat, int32_t threadId) {
//    struct tm ttm = {0};
//    ttm.tm_year = 119;
//    ttm.tm_mon = 7;
//    ttm.tm_mday = 28;
//    ttm.tm_hour = 00;
//    ttm.tm_min = 0;
//    ttm.tm_sec = 21133;
//    ttm.tm_gmtoff = 5 * 3600;
//    currentTime = std::mktime(&ttm);

    MYSQL_RES *res;
    MYSQL_ROW row;

    // асоциативный массив в котором будем хранить данные по датам, времени, действиям, программам групп
    groupsMap groups;

    // плучаем реальные данные по астро событиям за три дня ()
    auto defAstroEvents = getDefaultAstroEvents(currentTime, lon, lat);

    // получаем данные из календаря шкафа
    getNodeAstroEvents(dBase, defAstroEvents);

    // пересчитываем длительность сумерек, сохраняем новую длительность ночи
    // так как в календаре шкафа могут быть указаны другие значения заката/восхода
    recalcAstroEvents(defAstroEvents);

    // заполняем группы астрособытиями "по умолчанию", расчитаными и полученными из календаря шкафа
    fillGroupsDefValues(&groups, defAstroEvents);

    // вспомогательный массив для сохранения программ по умолчанию для групп
    std::map<int, std::string> grpProgram;
    // вспомогательный массив для списка используемых программ
    std::map<std::string, std::string> programUuids;

    // выбираем данные по группам
    auto daeIt = defAstroEvents->begin();
    std::string query;
    query =
            "SELECT gt.title, gt.groupId, gt.deviceProgramUuid AS gProgram, date(gct.date) as date, time(gct.date) as time, gct.type as type, gct.deviceProgramUuid AS gcProgram, UNIX_TIMESTAMP(gct.date) AS datetime "
            "FROM `group` AS gt "
            "LEFT JOIN group_control AS gct ON "
            "(gct.groupUuid=gt.uuid AND DATE(gct.date) IN('" + (daeIt++)->first + "', '" + (daeIt++)->first + "', '" +
            (daeIt++)->first + "'))";
//    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] QUERY GROUP: %s", TAG, query.data());
    res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        // обновляем данные в массиве
        while ((row = mysql_fetch_row(res)) != nullptr) {
            char *tmpData;
            int groupId;
            std::string gProgram;
            std::string date;
            std::string time;
            int type;
            std::string gcProgram;

            groupId = std::stoi(row[dBase->getFieldIndex("groupId")]);

            tmpData = row[dBase->getFieldIndex("gProgram")];
            if (tmpData == nullptr) {
                gProgram.clear();
            } else {
                gProgram = tmpData;
            }

            // запоминаем программу по умолчанию
            grpProgram[groupId] = gProgram;
            if (!gProgram.empty()) {
                programUuids[gProgram] = gProgram;
            }

            tmpData = row[dBase->getFieldIndex("date")];
            if (tmpData == nullptr) {
                date.clear();
            } else {
                date = tmpData;
            }

            tmpData = row[dBase->getFieldIndex("datetime")];
            if (tmpData == nullptr) {
                time.clear();
            } else {
                time = tmpData;
            }

            tmpData = row[dBase->getFieldIndex("type")];
            if (tmpData == nullptr) {
                type = -1;
            } else {
                type = std::stoi(tmpData);
            }

            tmpData = row[dBase->getFieldIndex("gcProgram")];
            if (tmpData == nullptr) {
                gcProgram.clear();
            } else {
                gcProgram = tmpData;
            }

            // вносим данные в массив
            if (type == 1) {
                if (!gcProgram.empty()) {
                    groups[groupId][date][type]["prog"] = gcProgram;
                    programUuids[gcProgram] = gcProgram;
                } else {
                    groups[groupId][date][type]["prog"] = gProgram;
                    if (!gProgram.empty()) {
                        programUuids[gProgram] = gProgram;
                    }
                }

                if (!time.empty()) {
                    groups[groupId][date][type]["time"] = time;
                }
            }
        }

        mysql_free_result(res);
    }

    // на текущий момент, в массиве не хватает данных по программам которые назначены группам
    // бежим по списку grpProgram, проставляем в массиве groups программы, для тех дат, для которых они не указанны
    for (int i = 0; i < 16; i++) {
        daeIt = defAstroEvents->begin();
        std::string defPrg;
        while (daeIt != defAstroEvents->end()) {
            for (int j = 0; j < 2; j++) {
                defPrg = groups[i][daeIt->first][j]["prog"];
                if (defPrg.empty()) {
                    defPrg = grpProgram[i];
                    groups[i][daeIt->first][j]["prog"] = defPrg;
                }
            }

            daeIt++;
        }
    }

    for (int i = 0; i < 16; i++) {
        auto prevDayIt = defAstroEvents->begin();
        auto currDayIt = defAstroEvents->begin();
        currDayIt++;

        while (currDayIt != defAstroEvents->end()) {
            auto prevDay = prevDayIt->first;
            auto currDay = currDayIt->first;

            uint32_t nightLenght;
            uint32_t twilightLength;

            // длительность ночи = восход сегодня - закат вчера
            nightLenght = std::stol(groups[i][currDay][0]["time"]) - std::stol(groups[i][prevDay][1]["time"]);
            double K = (double) nightLenght / std::stol(groups[i][prevDay][4]["nLength"]);
            // длительность ночи со вчера на сегодня
            groups[i][prevDay][4]["nLength"] = std::to_string(nightLenght);
            // длительность вчерашних сумерек = конец сумерек - закат
            twilightLength = (uint32_t) (std::stol(groups[i][prevDay][5]["twLength"]) * K);
            // длительность вчерашних сумерек
            groups[i][prevDay][5]["twLength"] = std::to_string(twilightLength);

            prevDayIt++;
            currDayIt++;
        }
    }

    // получаем список программ в виде объектов
    auto devPrograms = getUsedPrograms(dBase, &programUuids);

    // к этому моменту у меня есть список программ, список групп со всеми астрособытиями для них, астрособытия для шкафа (с учётом календарей)

    // массив в который складируем время-уровень освещённости (должно быть 16 значений)
    std::map<time_t, uint8_t> testData;

    // расчитываем временные метки за вчерашний день начиная с заката и сегодняшнего рассвета плюс с сегодняшнего заката до завтрашнего рассвета
    // для меток получаем значения яркости светильников в зависимости от программы управления
    // строим массив из 16 пар значений группа-яркость
    for (int i = 0; i < 16; i++) {
        testData.clear();

        auto prevDayIt = defAstroEvents->begin();
        auto currDayIt = defAstroEvents->begin();
        currDayIt++;

        DeviceProgram program;
        uint8_t brightValue = 99;
        std::string prog = groups[i][prevDayIt->first][1]["prog"];
        if (!prog.empty()) {
            program = devPrograms->find(prog)->second;
            while (currDayIt != defAstroEvents->end()) {

                long twLength = std::stol(groups[i][prevDayIt->first][5]["twLength"]);
                long nLength = std::stol(groups[i][prevDayIt->first][4]["nLength"]);
                nLength = nLength - twLength * 2;

                time_t sunrise = std::stol(groups[i][currDayIt->first][0]["time"]);
                time_t sunset = std::stol(groups[i][prevDayIt->first][1]["time"]);
                time_t twEnd = sunset + twLength;
                auto time2 = (time_t) ((double) twEnd + (nLength * (1.0 / (100.0 / program.time2))));
                auto time3 = (time_t) ((double) time2 + (nLength * (1.0 / (100.0 / program.time3))));
                time_t twBegin = sunrise - twLength;

                testData[sunset] = program.value1;
                testData[twEnd] = program.value2;
                testData[time2] = program.value3;
                testData[time3] = program.value4;
                testData[twBegin] = program.value5;
                testData[sunrise] = 0;

                prevDayIt++;
                currDayIt++;
            }

            auto testDataItCurr = testData.begin();
            auto testDataItNext = testData.begin();
            testDataItNext++;
            while (testDataItNext != testData.end()) {
                if (currentTime >= testDataItCurr->first && currentTime < testDataItNext->first) {
                    brightValue = testDataItCurr->second;
                    break;
                }

                testDataItCurr++;
                testDataItNext++;
            }
        }

        lightGroupBright[i] = brightValue;
    }

    // TODO: скопировать необходимые данные для отладки и протоколирования из старого варианта
/*
if (kernel->isDebug) {
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %llu", TAG, checkTime);
    kernel->log.ulogw(LOG_LEVEL_INFO,
                      "[%s] twilightStartTime: %llu, sunRiseTime: %llu, sunSetTime: %llu, twilightEndTime: %llu",
                      TAG, twilightStartTime, sunRiseTime, sunSetTime, twilightEndTime);
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] dayLen: %llu, nightLen: %llu, twilightLen: %llu, sum: %llu",
                      TAG, dayLen, nightLen, twilightLen, dayLen + nightLen + twilightLen);
}

    res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        while ((row = mysql_fetch_row(res)) != nullptr) {
            int percent;
            if (currentProgram != row[dBase->getFieldIndex("title")]) {
                currentProgram = row[dBase->getFieldIndex("title")];
                // нужно пересчитать параметры программы
                percent = std::stoi(row[dBase->getFieldIndex("time2")]);
                time1raw = twilightEndTime + (uint64_t) (nightLen * (1.0 / (100.0 / percent)));
                percent = std::stoi(row[dBase->getFieldIndex("time3")]);
                time2raw = time1raw + (uint64_t) (nightLen * (1.0 / (100.0 / percent)));

                time1loc = time1raw > 86400 ? time1raw - 86400 : time1raw;
                time2loc = time2raw > 86400 ? time2raw - 86400 : time2raw;
if (kernel->isDebug) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] time1raw: %llu, time2raw: %llu", TAG, time1raw, time2raw);
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] time1loc: %llu, time2loc: %llu", TAG, time1loc, time2loc);
}
            }

            ssize_t rc;
            bool processed = false;
            std::string address = row[dBase->getFieldIndex("address")];
            // интервал от заката до конца сумерек
            if (!lightFlags[address].isPeriod1()) {
                if (twilightEndTime > 86400) {
                    if ((checkTime >= sunSetTime && checkTime < 86400) ||
                        (checkTime >= 0 && checkTime < twilightEndTimeLoc)) {
                        processed = true;
if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 1 overnight", TAG, address.data());
}
                    }
                } else {
                    if (checkTime >= sunSetTime && checkTime < twilightEndTime) {
                        processed = true;
if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 1", TAG, address.data());
}
                    }
                }

                if (processed) {
                    lightFlags[address].setPeriod1Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value1")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %ld", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
}
                }
            }

            // интервал от конца сумерек до длительности заданной time1
            processed = false;
            if (!lightFlags[address].isPeriod2()) {
                if (time1raw > 86400) {
                    // переход через полночь
                    if ((checkTime >= twilightEndTime && checkTime < 86400) ||
                        (checkTime >= 0 && checkTime < time1loc)) {
                        processed = true;
if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 2 overnight", TAG, address.data());
}
                    }
                } else {
                    if (checkTime >= twilightEndTime && checkTime < time1loc) {
                        processed = true;
if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 2", TAG, address.data());
}
                    }
                }

                if (processed) {
                    lightFlags[address].setPeriod2Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value2")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %ld", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
}
                }
            }

            // интервал от time1 до длительности заданной time2
            processed = false;
            if (!lightFlags[address].isPeriod3()) {
                if (time1loc > time2loc) {
                    // переход через полночь (time1 находится до полуночи, time2 после полуночи)
                    if ((checkTime >= time1raw && checkTime < 86400) || (checkTime >= 0 && checkTime < time2loc)) {
                        processed = true;
if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 3 overnight time2 after", TAG,
                                          address.data());
}
                    }
                } else {
                    if (checkTime >= time1loc && checkTime < time2loc) {
                        processed = true;
if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 3", TAG, address.data());
}
                    }
                }

                if (processed) {
                    lightFlags[address].setPeriod3Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value3")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %ld", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
}
                }
            }

            // интервал от time2 до начала сумерек
            processed = false;
            if (!lightFlags[address].isPeriod4()) {
                if (time2raw > 86400) {
                    // переход через полночь (time2 находится после полуночи)
                    if (checkTime >= time2loc && checkTime < twilightStartTime) {
                        processed = true;
if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 4 overnight", TAG, address.data());
}
                    }
                } else {
                    if ((checkTime >= time2loc && checkTime < 86400) ||
                        (checkTime >= 0 && checkTime < twilightStartTime)) {
                        processed = true;
if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 4", TAG, address.data());
}
                    }
                }

                if (processed) {
                    lightFlags[address].setPeriod4Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value4")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %ld", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
}
                }
            }

            // интервал от начала сумерек до восхода
            if (!lightFlags[address].isPeriod5()) {
                if (checkTime >= twilightStartTime && checkTime < sunRiseTime) {
                    lightFlags[address].setPeriod5Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value5")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 5", TAG, address.data());
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %llu", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
}
                }
            }

            // день
            if (!lightFlags[address].isDay()) {
                if (checkTime >= sunRiseTime && checkTime < sunSetTime) {
                    isDay = true;
                    lightFlags[address].setDayActive();
if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period day", TAG, address.data());
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %llu", TAG, checkTime);
                    // TODO: разобраться - должен я здесь отправлять какие-то команды?
//                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
}
                }
            }

            // TODO: пересмотреть алгоритм, для выявления подобного события
            // длина суммы периодов меньше длины ночи или равна 0
//            if (!processed) {
//                if (!lightFlags[addresses[i]].isNoEvents) {
//                    setNoEventsActive(&lightFlags[addresses[i]]);
//                    printf("[%s] no events by light program\n", TAG);
//                    printf("[%s] checkTime: %ld\n", TAG, checkTime);
//                }
//            }
        }

        if (isDay) {
            ssize_t rc = switchAllLight(0);
            if (rc == -1) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                mtmZigbeeStopThread(dBase, threadId);
                AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
                                  (char *) "Ошибка записи в порт координатора");
                return;
            }
if (kernel->isDebug) {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Switch all lights off by program", TAG);
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
}
        }

        mysql_free_result(res);
    }
*/
}

