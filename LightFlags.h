#include<cstdint>

#ifndef ESCADA_CORE_LIGHTFLAGS_H
#define ESCADA_CORE_LIGHTFLAGS_H


class LightFlags {
private:
    uint8_t flags;
public :
    LightFlags();

    void setPeriod1Active();

    bool isPeriod1();

    void setPeriod2Active();

    bool isPeriod2();

    void setPeriod3Active();

    bool isPeriod3();

    void setPeriod4Active();

    bool isPeriod4();

    void setPeriod5Active();

    bool isPeriod5();

    void setDayActive();

    bool isDay();
};

#endif //ESCADA_CORE_LIGHTFLAGS_H
