#include "rpm_scale.h"

int rpm_scale_lit(int rpm)
{
    if (rpm <= 0)
        return 0;
    if (rpm >= RPM_SCALE_MAX)
        return RPM_SCALE_SEGMENTS;
    return (rpm * RPM_SCALE_SEGMENTS + RPM_SCALE_MAX / 2) / RPM_SCALE_MAX;
}

int rpm_scale_redline_seg(void)
{
    // ceil(REDLINE * SEGMENTS / MAX): first segment whose start is >= redline.
    return (RPM_SCALE_REDLINE * RPM_SCALE_SEGMENTS + RPM_SCALE_MAX - 1) / RPM_SCALE_MAX;
}
