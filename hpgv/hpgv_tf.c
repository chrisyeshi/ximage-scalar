#include "hpgv_tf.h"
#include "assert.h"

//
//
// Local Declarations
//
//

#define nPerColor 4

//
//
// Local Variables
//
//

static float stepBase = 10.f;

//
//
// Forward Declarations
//
//

tf_color_t hpgv_tf_sample(const float* tf, int tfsize, float val);

//
//
// Local Functions
//
//

int getBinId(const float* binTicks, float value)
{
    int binId = -1;
    for (binId = 0; binId < hpgv_raf_bin_num; ++binId)
    {
        float lower = binTicks[binId];
        float upper = binTicks[binId+1];
        if (lower <= value && value < upper)
        {
            return binId;
        }
    }
    return -1;
}

tf_color_t tf_seg_integrate(const float* tf, int tfsize,
        float valbeg, float valend, float length)
{
//    // Default Simplified Transfer Function
//    //========================================================
//    // error checking
//    float small = valbeg < valend ? valbeg : valend;
//    float big = valbeg < valend ? valend : valbeg;
//    int binId = getBinId(small);
//    // here we go
//    float binVal = binVals[binId];
//    if (small <= binVal && binVal <= big)
//    { // across the beginning value
//        tf_color_t color = hpgv_tf_sample(tf, tfsize, binVal);
//        color.a = 1.f / 16.f;
//        return color;
//    }
//    // not crossing the avg value
//    tf_color_t color;
//    color.r = 0.f;
//    color.g = 0.f;
//    color.b = 0.f;
//    color.a = 0.f;
//    return color;

    // Integrate over transfer function
    //=========================================================
    tf_color_t colorbeg = hpgv_tf_sample(tf, tfsize, valbeg);
    tf_color_t colorend = hpgv_tf_sample(tf, tfsize, valend);
    tf_color_t color;
    color.r = (colorbeg.r + colorend.r) * 0.5f;
    color.g = (colorbeg.g + colorend.g) * 0.5f;
    color.b = (colorbeg.b + colorend.b) * 0.5f;
    color.a = (colorbeg.a + colorend.a) * 0.5f;
    // adjust to step size
    color.a = 1.0 - pow(1.0 - color.a, length / stepBase);
    color.r *= color.a;
    color.g *= color.a;
    color.b *= color.a;
    return color;
}

//
//
// Public Functions
//
//

tf_color_t hpgv_tf_sample(const float* tf, int tfsize, float val)
{
    assert(val >= 0.f && val <= 1.f);
    // sample position in tf space
    float x = val * (float)tfsize - 0.5f;
    // clamp to edge
    if (x < 0.f) {
        tf_color_t color;
        memcpy(&color, &tf[nPerColor * 0], sizeof(tf_color_t));
        return color;
    }
    if (x >= (float)(tfsize - 1)) {
        tf_color_t color;
        memcpy(&color, &tf[nPerColor * (tfsize - 1)], sizeof(tf_color_t));
        return color;
    }
    // linear interpolate
    int head = (int)(x + 0);
    int tail = (int)(x + 1);
    float ratio = x - (float)head;
    assert(tail <= tfsize - 1);
    tf_color_t color_head, color_tail;
    memcpy(&color_head, &tf[nPerColor * head], sizeof(tf_color_t));
    memcpy(&color_tail, &tf[nPerColor * tail], sizeof(tf_color_t));
    tf_color_t color;
    color.r = color_head.r * (1.f - ratio) + color_tail.r * ratio;
    color.g = color_head.g * (1.f - ratio) + color_tail.g * ratio;
    color.b = color_head.b * (1.f - ratio) + color_tail.b * ratio;
    color.a = color_head.a * (1.f - ratio) + color_tail.a * ratio;
    return color;
}

void hpgv_tf_raf_integrate(const float* tf, int tfsize, const float binTicks[],
        float left_value, float rite_value, float left_depth, float rite_depth,
        float sampling_spacing, hpgv_raf_t* histogram)
{
//    // Sample with Manual Bin Values
//    int i;
//#pragma omp parallel for private(i)
//    for (i = 0; i < 16; ++i) {
//        if (left_value <= binVals[i] && binVals[i] <= rite_value)
//        {
//            float alpha = 1.f / 16.f;
//            float attenuation = (1.f - histogram->attenuation) * alpha;
//            histogram->attenuation += attenuation;
//            histogram->raf[i] += attenuation;
//            float depth = left_depth;
//            if (fabs(rite_value - left_value) > 0.0001)
//                depth = (binVals[i] - left_value) / (rite_value - left_value) * (rite_depth - left_depth) + left_depth;
//            if (depth < histogram->depths[i])
//                histogram->depths[i] = depth;
//        }
//    }

//     // Sample at Ray Segment End Points
//     int binId = CLAMP((int)(rite_value * HPGV_RAF_BIN_NUM), 0, HPGV_RAF_BIN_NUM - 1);
//     tf_color_t color = hpgv_tf_sample(tf, tfsize, rite_value);
//     float attenuation = (1.f - histogram->attenuation) * color.a;
//     histogram->attenuation += attenuation;
//     histogram->raf[binId] += attenuation;
//     if (rite_depth < histogram->depths[binId])
//         histogram->depths[binId] = rite_depth;

    // Integrate Over Ray Segments
//    int binBeg = CLAMP((int)(left_value * HPGV_RAF_BIN_NUM), 0, HPGV_RAF_BIN_NUM - 1);
//    int binEnd = CLAMP((int)(rite_value * HPGV_RAF_BIN_NUM), 0, HPGV_RAF_BIN_NUM - 1);
    int binBeg = getBinId(binTicks, left_value);
    int binEnd = getBinId(binTicks, rite_value);
    // if binBeg == binEnd
    if (binBeg == binEnd) {
        float valBeg = left_value;
        float valEnd = rite_value;
        tf_color_t color = tf_seg_integrate(tf, tfsize, valBeg, valEnd, sampling_spacing);
        float attenuation = (1.f - histogram->attenuation) * color.a;
        histogram->attenuation += attenuation;
        histogram->raf[binBeg] += attenuation;
        return;
    }
    int dir = (binEnd - binBeg) / abs(binEnd - binBeg);
    // the beginning bin, which is partially included
    {
        float valBeg = left_value;
        float valEnd;
        if (dir > 0)
            valEnd = binTicks[binBeg+1]; //(float)(binBeg + 1) / (float)(HPGV_RAF_BIN_NUM);
        else
            valEnd = binTicks[binBeg]; //(float)(binBeg) / (float)(HPGV_RAF_BIN_NUM);
        float length = 0.f;
        if (fabs(rite_value - left_value) > 0.0001)
            length = (valEnd - valBeg) / (rite_value - left_value) * sampling_spacing;
        tf_color_t color  = tf_seg_integrate(tf, tfsize, valBeg, valEnd, length);
        float attenuation = (1.f - histogram->attenuation) * color.a;
        histogram->attenuation += attenuation;
        histogram->raf[binBeg] += attenuation;
        if (dir < 0)
        {
            float binVal = binTicks[binBeg];
            float depth = (binVal - valBeg) / (rite_value - valBeg) * (rite_depth - left_depth) + left_depth;
            if (depth < histogram->depths[binBeg])
                histogram->depths[binBeg] = depth;
        }
    }
    // the bins in between
    int binId;
    for (binId = binBeg + dir; binId != binEnd; binId += dir)
    {
        float valBeg, valEnd;
        if (dir > 0) {
            valBeg = binTicks[binId]; //(float)(binId + 0) / (float)(HPGV_RAF_BIN_NUM);
            valEnd = binTicks[binId+1]; //(float)(binId + 1) / (float)(HPGV_RAF_BIN_NUM);
        } else {
            valBeg = binTicks[binId+1]; //(float)(binId + 1) / (float)(HPGV_RAF_BIN_NUM);
            valEnd = binTicks[binId]; //(float)(binId + 0) / (float)(HPGV_RAF_BIN_NUM);
        }
        float length = 0.f;
        if (fabs(rite_value - left_value) > 0.0001)
            length = (valEnd - valBeg) / (rite_value - left_value) * sampling_spacing;
        tf_color_t color = tf_seg_integrate(tf, tfsize, valBeg, valEnd, length);
        float attenuation = (1.f - histogram->attenuation) * color.a;
        histogram->attenuation += attenuation;
        histogram->raf[binId] += attenuation;
        float binVal = binTicks[binId];
        float depth = (binVal - left_value) / (rite_value - left_value) * (rite_depth - left_depth) + left_depth;
        if (depth < histogram->depths[binId])
            histogram->depths[binId] = depth;
    }
    // the end bin, which is also partially included
    {
        float valBeg;
        if (dir > 0)
            valBeg = binTicks[binEnd]; //(float)(binEnd + 0) / (float)(HPGV_RAF_BIN_NUM);
        else
            valBeg = binTicks[binEnd+1]; //(float)(binEnd + 1) / (float)(HPGV_RAF_BIN_NUM);
        float valEnd = rite_value;
        float length = 0.f;
        if (fabs(rite_value - left_value) > 0.0001)
            length = (valEnd - valBeg) / (rite_value - left_value) * sampling_spacing;
        tf_color_t color  = tf_seg_integrate(tf, tfsize, valBeg, valEnd, length);
        float attenuation = (1.f - histogram->attenuation) * color.a;
        histogram->attenuation += attenuation;
        histogram->raf[binEnd] += attenuation;
        if (dir > 0)
        {
            float binVal = binTicks[binEnd];
            float depth = (binVal - valBeg) / (rite_value - valBeg) * (rite_depth - left_depth) + left_depth;
            if (depth < histogram->depths[binEnd])
                histogram->depths[binEnd] = depth;
        }
    }
}

void hpgv_tf_raf_seg_integrate(const float* tf, int tfsize, const float* binTicks,
        float left_value, float rite_value, float left_depth, float rite_depth,
        float sampling_spacing, hpgv_raf_seg_t* seg, int segid)
{
    float* data = (float*)malloc(hpgv_formatsize(HPGV_RAF) * sizeof(float));
    hpgv_raf_t* histogram = (hpgv_raf_t*)data;
    histogram->raf = &data[hpgv_raf_offset()];
    histogram->depths = &data[hpgv_raf_offset() + hpgv_raf_bin_num];

    hpgv_raf_reset(histogram);
    histogram->attenuation = seg->attenuation;
    hpgv_tf_raf_integrate(tf, tfsize, binTicks,
            left_value, rite_value, 1.f - left_depth, 1.f - rite_depth,
            sampling_spacing, histogram);
    // composite
    seg->attenuation = histogram->attenuation;
    int segBin;
    for (segBin = 0; segBin < HPGV_RAF_SEG_NUM; ++segBin)
    {
        int rafBin = segBin + segid * HPGV_RAF_SEG_NUM;
        seg->raf[segBin] += histogram->raf[rafBin];
        if (histogram->depths[rafBin] < 1.f - seg->depths[segBin])
            seg->depths[segBin] = 1.f - histogram->depths[rafBin];
    }

    free(data);
}

tf_color_t hpgv_tf_rgba_integrate(const float *tf, int tfsize, float left_value, float rite_value, float sampling_spacing)
{
    return tf_seg_integrate(tf, tfsize, left_value, rite_value, sampling_spacing);
}
