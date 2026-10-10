
#include "boat_attitude.h"
#include "boat_vehicle.h"
#include "boat_config.h"
__declspec(dllexport) void YawReset(void) { BoatAttitude_Init(); }
__declspec(dllexport) float YawStep(float error_deg, float rate_deg, float dt, unsigned int now_ms) {
    BoatVehicle v = {0};
    v.heading_deg = -error_deg;
    v.yaw_rate_rad = rate_deg * BOAT_DEG2RAD;
    v.pos_valid = v.heading_valid = v.speed_valid = 1;
    v.now_ms = now_ms;
    BoatVehicle_Set(&v);
    return BoatAttitude_SteerFromHeading(0.0f, 0.0f, dt);
}
