#include <utils/Log.h>

#define TOF_TEMP_NODE "/sys/devices/virtual/misc/tof_temp_dev/tof_temp_sensor"
#define TOF_EXPO_NODE "/sys/tof_control/tof_exposure"
#define TOF_FRAME_NODE "/sys/tof_control/tof_frame"
#define TOF_FREQUENCY_NODE "/sys/tof_control/tof_frequency"
#define TOF_EXPO_REG_NODE "/sys/tof_control/tof_expo_reg"
#define TOF_FREQ_1 80.32
#define TOF_FREQ_2 60.24

unsigned short Calc_Exposure_Value(unsigned int exposure, float freq);
float tof_get_temperature()
{
    float temp_val = 25.0;
    int val=0, size = 0;
    FILE *temp_fd = NULL;

    temp_fd = fopen(TOF_TEMP_NODE, "r");
    if(NULL == temp_fd)
    {
        ALOGE("tof_temp open file fail: %s", TOF_TEMP_NODE);
        return temp_val;
    }
    size = fscanf(temp_fd, "0x%04x", &val);
    fclose(temp_fd);
    temp_val = val/16 * 0.065;

    return temp_val;
}

int tof_set_mode(int freq_mode,int frame_rate,int expo_time)
{
    FILE *expo_fd = NULL;
    FILE *frame_fd = NULL;
    FILE *frequency_fd = NULL;
    int size = 0;
    unsigned int reg1, reg2, expo_reg_val = 0;

    reg1 = Calc_Exposure_Value(expo_time, TOF_FREQ_1);
    if(0 == reg1)
        return -1;
    reg2 = Calc_Exposure_Value(expo_time, TOF_FREQ_2);
    if(0 == reg2)
        return -1;
    expo_reg_val = (reg1<<16)|reg2;

    expo_fd = fopen(TOF_EXPO_NODE, "w");
    if(NULL == expo_fd)
    {
        ALOGE("tof_expo open file fail: %s", TOF_EXPO_NODE);
        return -1;
    }
    size = fprintf(expo_fd,"%08x", expo_reg_val);
    fclose(expo_fd);

    frame_fd = fopen(TOF_FRAME_NODE, "w");
    if(NULL == frame_fd)
    {
        ALOGE("tof_frame open file fail: %s", TOF_FRAME_NODE);
        return -1;
    }
    size = fprintf(frame_fd,"%d", frame_rate);
    fclose(frame_fd);

    frequency_fd = fopen(TOF_FREQUENCY_NODE, "w");
    if(NULL == frequency_fd)
    {
        ALOGE("tof_frequency open file fail: %s", TOF_FREQUENCY_NODE);
        return -1;
    }
    size = fprintf(frequency_fd,"%d", freq_mode);
    fclose(frequency_fd);

    ALOGI(" ------ [cjq]set tof mode complete");
    return 0;
}

/*
Func: Calc_Exposure_Value - calculate the register value for given exposure time
Input: exposure - exposure time in us, e.g., 100us or 675us
Input: freq - frequency in MHz, e.g., 60.24MHz or 80.32MHz
Return: 0 on failure, register value on success
*/
#define PRESCALER   8
unsigned short Calc_Exposure_Value(unsigned int exposure, float freq)
{
    float maxlimit, minlimit;
    unsigned short exposure_regValue;
    maxlimit = 16383*(8/freq);
    minlimit = 8/freq;
    if(exposure<minlimit || exposure>maxlimit)
        return 0;
    else
        exposure_regValue = (float)exposure*freq/PRESCALER + 16384; //1*pow(2,14);
    return exposure_regValue;
}

/*
Func: tof_set_exposure - update exposure time
Input: expo_time - exposure time in us, e.g., 100us
Return: 0 on success, -1 on failure
*/
int tof_set_exposure(int expo_time)
{
    unsigned int reg1, reg2, expo_reg_val;
    FILE *expo_reg_fp=NULL;
    int size=0;

    reg1 = Calc_Exposure_Value(expo_time, TOF_FREQ_1);
    if(0 == reg1)
        return -1;
    reg2 = Calc_Exposure_Value(expo_time, TOF_FREQ_2);
    if(0 == reg2)
        return -1;

    expo_reg_val = (reg1<<16)|reg2;
    expo_reg_fp = fopen(TOF_EXPO_REG_NODE, "w");
    if(NULL == expo_reg_fp)
    {
        ALOGE("tof_expo_reg open file fail: %s", TOF_EXPO_REG_NODE);
        return -1;
    }
    size = fprintf(expo_reg_fp,"%08x", expo_reg_val);
    fclose(expo_reg_fp);

    return 0;
}
