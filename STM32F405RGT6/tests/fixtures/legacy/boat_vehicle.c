#include "boat_vehicle.h"

static BoatVehicle s_vehicle;

void BoatVehicle_Set(const BoatVehicle *v)
{
    if (v == 0) {
        return;
    }
    s_vehicle = *v;
}

const BoatVehicle *BoatVehicle_Get(void)
{
    return &s_vehicle;
}
