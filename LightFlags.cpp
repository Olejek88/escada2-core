#include "LightFlags.h"

LightFlags::LightFlags() {
    flags = 0;
}

void LightFlags::setPeriod1Active() {
    flags = 0b000001;
}

bool LightFlags::isPeriod1() {
    return flags & 0b000001u;
}

void LightFlags::setPeriod2Active() {
    flags = 0b000010;
}

bool LightFlags::isPeriod2() {
    return flags & 0b000010u;
}

void LightFlags::setPeriod3Active() {
    flags = 0b000100;
}

bool LightFlags::isPeriod3() {
    return flags & 0b000100u;
}

void LightFlags::setPeriod4Active() {
    flags = 0b001000;
}

bool LightFlags::isPeriod4() {
    return flags & 0b001000u;
}

void LightFlags::setPeriod5Active() {
    flags = 0b010000;
}

bool LightFlags::isPeriod5() {
    return flags & 0b010000u;
}

void LightFlags::setDayActive() {
    flags = 0b100000;
}

bool LightFlags::isDay() {
    return flags & 0b100000u;
}
