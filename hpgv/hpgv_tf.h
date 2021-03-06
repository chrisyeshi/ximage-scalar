#ifndef HPGV_TF_H
#define HPGV_TF_H

#ifdef __cplusplus
extern "C" 
{
#endif

#include "assert.h"
#include "hpgv_gl.h"
#include "hpgv_utilmath.h"
#include "hpgv_util.h"

typedef struct tf_color_t {
    float r, g, b, a;
} tf_color_t;

tf_color_t hpgv_tf_sample(const float* tf, int tfsize, float val);

void hpgv_tf_raf_integrate(const float* tf, int tfsize, const float binTicks[],
        float left_value, float rite_value, float left_depth, float rite_depth,
        float sampling_spacing, hpgv_raf_t* histogram);

void hpgv_tf_raf_seg_integrate(const float* tf, int tfsize, const float binTicks[],
        float left_value, float rite_value, float left_depth, float rite_depth,
        float sampling_spacing, hpgv_raf_seg_t* seg, int segid);

tf_color_t hpgv_tf_rgba_integrate(const float* tf, int tfsize,
        float left_value, float rite_value, float sampling_spacing);

#ifdef __cplusplus
}
#endif

#endif
