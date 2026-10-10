
#include "boat_attitude.h"
#include "boat_vehicle.h"
#include "boat_config.h"
__declspec(dllexport) void YawReset(void) { BoatAttitude_Init(); }
static void sense(float forward, float error, float rate, unsigned int now) {
    BoatVehicle v = {0};
    v.heading_deg=-error;v.yaw_rate_rad=rate*BOAT_DEG2RAD;v.forward_mps=forward;
    v.speed_valid=v.heading_valid=v.pos_valid=1;v.now_ms=now;BoatVehicle_Set(&v);
}
__declspec(dllexport) float YawStep(float error,float rate,float dt,unsigned int now) {
    sense(0,error,rate,now);return BoatAttitude_SteerFromHeading(0,0,dt);
}
__declspec(dllexport) float SpeedStep(float target,float speed,float dt,unsigned int now) {
    sense(speed,0,0,now);
    return target<=0.000001f ? BoatAttitude_ThrottleStop(dt) : BoatAttitude_ThrottleFromSpeed(target,dt);
}
