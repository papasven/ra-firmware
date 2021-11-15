
#include <math.h>
#include <stdlib.h>
#if !defined(M_PI)
#  define M_PI 3.14159265358979323846
#endif
#include <string.h>
#include <stdio.h>

#include "lpclib.h"
#include "m20.h"
#include "m20private.h"
#include "observer.h"
#include "rinex.h"


static const double _m20_coordinateFactor = M_PI / (180.0 * 1e6);


/* Read 24-bit little-endian signed integer from memory */
static int32_t _M20_read24_BigEndian (const uint8_t *p24)
{
    int32_t value = p24[2] + 256 * p24[1] + 65536 * p24[0];
    if (value & (1u << 23)) {
        value |= 0xFF000000;
    }

    return value;
}



LPCLIB_Result _M20_processPayloadInner (
        const struct _M20_PayloadInner *payload,
        M20_CookedGps *cookedGps,
        M20_CookedMetrology *cookedMetro)
{
    LLA_Coordinate lla;

    lla.lat = NAN;
    lla.lon = NAN;
    lla.alt = NAN;
    lla.climbRate = NAN;
    lla.velocity = NAN;
    lla.direction = NAN;

    float altitude = _M20_read24_BigEndian(&payload->altitude[0]) / 100.0f;
    if (altitude > -100.0) {
        lla.alt = altitude;
        float ve = payload->speedE * 0.01f;
        float vn = payload->speedN * 0.01f;
        lla.velocity = sqrtf(ve * ve + vn * vn);
        lla.direction = atan2f(ve, vn);
        if (lla.direction <= 0) {
            lla.direction += 2.0f * M_PI;
        }
        GPS_applyGeoidHeightCorrection(&lla);
    }

    cookedGps->observerLLA = lla;

    /* Sensors */
    cookedMetro->temperature = NAN;
    cookedMetro->humidity = NAN;

    return LPCLIB_SUCCESS;
}



LPCLIB_Result _M20_processPayload (
        const struct _M20_Payload *payload,
        _Bool valid,
        M20_CookedGps *cookedGps,
        M20_CookedMetrology *cookedMetro)
{
    LLA_Coordinate lla;

    lla = cookedGps->observerLLA;

    if (valid) {
        lla.lat = (double)payload->latitude * _m20_coordinateFactor;
        lla.lon = (double)payload->longitude * _m20_coordinateFactor;
        lla.climbRate = 0.01f * (float)payload->climbRate;

        cookedMetro->batteryVoltage = payload->vbat * (3.0f / 228.0f);  //TODO use ADC VDD and resistor divider
        cookedMetro->cpuTemperature = payload->cpuTemperature * 0.4f;
    }

    cookedGps->observerLLA = lla;

    return LPCLIB_SUCCESS;
}


