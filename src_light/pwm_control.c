#include "wiced_timer.h"
#include "wiced_rtos.h"
#include "wiced_bt_trace.h"

#include "config.h"
#include "pwm_control.h"
#include "wiced_hal_wdog.h"
#include "log.h"

#include "wiced_platform.h"
#include "wiced_hal_pwm.h"

#if CHIP_SCHEME == CHIP5600
#define PWM_INP_CLK_IN_HZ (4096 * 1000)
#define PWM_FREQ_IN_HZ (1000)
#define PORT_EN WICED_P28
#define PORT_PWM WICED_P29
#define PWM_CHANNEL PWM3
#define WICED_PWM_CHANNEL WICED_PWM3
#elif CHIP_SCHEME == CHIP58834
#define PWM_INP_CLK_IN_HZ (4096 * 1000)
#define PWM_FREQ_IN_HZ (1000)
#define PWM_FREQ_IN_HZ_CTL (4096)
#define PORT_CTL WICED_P28
#define PORT_PWM WICED_P29
#define PWM_CHANNEL PWM3
#define CTL_CHANNEL PWM2
#define WICED_PWM_CHANNEL WICED_PWM3
#define WICED_CTL_CHANNEL WICED_PWM2
#elif CHIP_SCHEME == CHIP6322
#define PWM_INP_CLK_IN_HZ (4096 * 1000)
#define PWM_FREQ_IN_HZ (1000)
#define PORT_EN WICED_P28
#define PORT_PWM WICED_P29
#define PWM_CHANNEL PWM3
#define WICED_PWM_CHANNEL WICED_PWM3
#elif CHIP_SCHEME == CHIP1424
#define PWM_INP_CLK_IN_HZ (4096 * 1000)
#define PWM_FREQ_IN_HZ (1000)

#define PORT_PWM WICED_P29
#define PWM_CHANNEL PWM3
#define WICED_PWM_CHANNEL WICED_PWM3

#define PORT_PWM1 WICED_P26
#define PWM_CHANNEL1 PWM0
#define WICED_PWM_CHANNEL1 WICED_PWM0

#elif CHIP_SCHEME == CHIP2306
#define PWM_INP_CLK_IN_HZ (4096 * 1000)
#define PWM_FREQ_IN_HZ (1000)
#define PORT_PWM WICED_P29
#define PWM_CHANNEL PWM3
#define WICED_PWM_CHANNEL WICED_PWM3
#elif CHIP_SCHEME == CHIP_DEVKIT
#define PWM_INP_CLK_IN_HZ (4096 * 1000)
#define PWM_FREQ_IN_HZ (1000)
#define PORT_PWM WICED_P29 
#define PWM_CHANNEL PWM3
#define WICED_PWM_CHANNEL WICED_PWM3

#define PORT_PWM1 WICED_P26
#define PWM_CHANNEL1 PWM0
#define WICED_PWM_CHANNEL1 WICED_PWM0

#else
#error 方案错误
#endif

static void pwm_init(void);

void led_controller_initial(void)
{
    pwm_init();
}

void pwm_set(uint16_t);
void pwm_set1(uint16_t);
static void pwm_on(void);
static void pwm_off(void);

uint8_t last_onoff = 0;

void led_controller_status_update(uint8_t onoff, uint16_t level, uint16_t temperature)
{
#define s(x) ((x) ? "on" : "off")
    LOG_DEBUG("LEVEL: %d\n", level);
    LOG_DEBUG("temperature: %d\n", temperature);
    LOG_DEBUG("%s -> %s\n", s(last_onoff), s(onoff));
    if (onoff)
    {
        if (last_onoff != 1)
        {
            pwm_on();
        }
        pwm_set(65535*temperature/65535*level/65535);
        pwm_set1(65535-(65535*(65535-temperature)/65535*level/65535));
  
    }
    else
    {
        if (last_onoff)
            pwm_off();
    }

    last_onoff = onoff;
}

void led_controller_status_update_angle(uint8_t onoff, uint16_t level, uint16_t angle)
{
#define s(x) ((x) ? "on" : "off")
    LOG_DEBUG("LEVEL: %d\n", level);
    LOG_DEBUG("temperature: %d\n", angle);
    LOG_DEBUG("%s -> %s\n", s(last_onoff), s(onoff));
    if (onoff)
    {
        if (last_onoff != 1)
        {
            pwm_on();
        }
        pwm_set(65535*level/65535*angle/65535);
        pwm_set1(65535*(65535-level)/65535*angle/65535);
    }
    else
    {
        if (last_onoff)
            pwm_off();
    }

    last_onoff = onoff;
}

//TODO: fix it
#define EN_PORT WICED_P28

// uint8_t need_init = 1;

static void pwm_init(void)
{
    pwm_on();
    //led_controller_status_update(1,20,0);
}

static void pwm_on()
{
    last_onoff = 1;
    // LOG_VERBOSE("pwm_on\n");
#if CHIP_SCHEME == CHIP5600
    pwm_config_t pwm_config;

    wiced_hal_gpio_select_function(EN_PORT, WICED_GPIO);

    // wiced_hal_pwm_configure_pin(PORT_PWM, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);

    wiced_hal_aclk_enable(PWM_INP_CLK_IN_HZ, ACLK1, ACLK_FREQ_24_MHZ);
    // wiced_hal_pwm_configure_pin(PORT_PWM, PWM_CHANNEL);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_PWM_CHANNEL);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 99, PWM_FREQ_IN_HZ, &pwm_config);
    wiced_hal_pwm_start(PWM_CHANNEL, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 0);
    wiced_hal_pwm_enable(PWM_CHANNEL);

    wiced_hal_gpio_configure_pin(EN_PORT, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    wiced_hal_gpio_set_pin_output(EN_PORT, GPIO_PIN_OUTPUT_LOW);
#elif CHIP_SCHEME == CHIP58834
    pwm_config_t pwm_config;

    // wiced_hal_pwm_configure_pin(PORT_CTL, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_CTL, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);

    // wiced_hal_pwm_configure_pin(PORT_PWM, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);

    wiced_hal_aclk_enable(PWM_INP_CLK_IN_HZ, ACLK1, ACLK_FREQ_24_MHZ);
    // wiced_hal_pwm_configure_pin(PORT_PWM, PWM_CHANNEL);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_PWM_CHANNEL);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 99, PWM_FREQ_IN_HZ, &pwm_config);
    wiced_hal_pwm_start(PWM_CHANNEL, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 0);
    wiced_hal_pwm_enable(PWM_CHANNEL);

    wiced_hal_aclk_enable(PWM_INP_CLK_IN_HZ, ACLK1, ACLK_FREQ_24_MHZ);
    // wiced_hal_pwm_configure_pin(PORT_CTL, CTL_CHANNEL);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_CTL_CHANNEL);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 99, PWM_FREQ_IN_HZ_CTL, &pwm_config);
    wiced_hal_pwm_start(CTL_CHANNEL, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 0);
    wiced_hal_pwm_enable(CTL_CHANNEL);
#elif CHIP_SCHEME == CHIP6322
    pwm_config_t pwm_config;

    wiced_hal_gpio_select_function(EN_PORT, WICED_GPIO);

    wiced_hal_aclk_enable(PWM_INP_CLK_IN_HZ, ACLK1, ACLK_FREQ_24_MHZ);
    // wiced_hal_pwm_configure_pin(PORT_PWM, PWM_CHANNEL);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_PWM_CHANNEL);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 99, PWM_FREQ_IN_HZ, &pwm_config);
    wiced_hal_pwm_start(PWM_CHANNEL, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 0);
    wiced_hal_pwm_enable(PWM_CHANNEL);

    wiced_hal_gpio_configure_pin(EN_PORT, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);
    wiced_hal_gpio_set_pin_output(EN_PORT, GPIO_PIN_OUTPUT_HIGH);
#elif CHIP_SCHEME == CHIP1424
      pwm_config_t pwm_config;

    wiced_hal_gpio_select_function(EN_PORT, WICED_GPIO);

    wiced_hal_aclk_enable(PWM_INP_CLK_IN_HZ, ACLK1, ACLK_FREQ_24_MHZ);
    // wiced_hal_pwm_configure_pin(PORT_PWM, PWM_CHANNEL);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_PWM_CHANNEL);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 1, PWM_FREQ_IN_HZ, &pwm_config);
    if(pwm_config.toggle_count > 65532)
    {
        pwm_config.toggle_count = 65532;
    }
    wiced_hal_pwm_start(PWM_CHANNEL, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 0);
    wiced_hal_pwm_enable(PWM_CHANNEL);

    wiced_hal_gpio_select_function(PORT_PWM1,WICED_PWM_CHANNEL1);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 1, PWM_FREQ_IN_HZ, &pwm_config);
    if(pwm_config.toggle_count > 65532)
    {
        pwm_config.toggle_count = 65532;
    }
    wiced_hal_pwm_start(PWM_CHANNEL1, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 1);
    wiced_hal_pwm_enable(PWM_CHANNEL1);


#elif CHIP_SCHEME == CHIP2306
    pwm_config_t pwm_config;

    wiced_hal_gpio_select_function(EN_PORT, WICED_GPIO);

    wiced_hal_aclk_enable(PWM_INP_CLK_IN_HZ, ACLK1, ACLK_FREQ_24_MHZ);
    // wiced_hal_pwm_configure_pin(PORT_PWM, PWM_CHANNEL);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_PWM_CHANNEL);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 1, PWM_FREQ_IN_HZ, &pwm_config);
    if(pwm_config.toggle_count > 65532)
    {
        pwm_config.toggle_count = 65532;
    }
    wiced_hal_pwm_start(PWM_CHANNEL, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 0);
    wiced_hal_pwm_enable(PWM_CHANNEL);
#elif CHIP_SCHEME == CHIP_DEVKIT
    pwm_config_t pwm_config;

    wiced_hal_gpio_select_function(EN_PORT, WICED_GPIO);

    wiced_hal_aclk_enable(PWM_INP_CLK_IN_HZ, ACLK1, ACLK_FREQ_24_MHZ);
    // wiced_hal_pwm_configure_pin(PORT_PWM, PWM_CHANNEL);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_PWM_CHANNEL);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 1, PWM_FREQ_IN_HZ, &pwm_config);
    if(pwm_config.toggle_count > 65532)
    {
        pwm_config.toggle_count = 65532;
    }
    wiced_hal_pwm_start(PWM_CHANNEL, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 0);
    wiced_hal_pwm_enable(PWM_CHANNEL);

    wiced_hal_gpio_select_function(PORT_PWM1,WICED_PWM_CHANNEL1);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 1, PWM_FREQ_IN_HZ, &pwm_config);
    if(pwm_config.toggle_count > 65532)
    {
        pwm_config.toggle_count = 65532;
    }
    wiced_hal_pwm_start(PWM_CHANNEL1, PMU_CLK, pwm_config.toggle_count, pwm_config.init_count, 1);
    wiced_hal_pwm_enable(PWM_CHANNEL1);


#else
#error 方案错误
#endif
}

static void pwm_off()
{
    last_onoff = 0;
    // LOG_VERBOSE("pwm_off\n");
#if CHIP_SCHEME == CHIP5600
    wiced_hal_gpio_configure_pin(EN_PORT, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);
    wiced_hal_gpio_set_pin_output(EN_PORT, GPIO_PIN_OUTPUT_HIGH);
    wiced_hal_pwm_disable(PWM_CHANNEL);
    // wiced_hal_pwm_configure_pin(PORT_PWM, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);
#elif CHIP_SCHEME == CHIP58834
    wiced_hal_pwm_disable(CTL_CHANNEL);
    // wiced_hal_pwm_configure_pin(PORT_CTL, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_CTL,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_CTL, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    wiced_hal_pwm_disable(PWM_CHANNEL);
    // wiced_hal_pwm_configure_pin(PORT_PWM, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
#elif CHIP_SCHEME == CHIP6322
    wiced_hal_gpio_configure_pin(EN_PORT, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    // wiced_hal_gpio_set_pin_output(EN_PORT, GPIO_PIN_OUTPUT_HIGH);
    // wiced_hal_pwm_disable(PWM_CHANNEL);
    // wiced_hal_pwm_configure_pin(PORT_PWM, WICED_GPIO);
    // wiced_hal_gpio_configure_pin(PORT_PWM, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);
#elif CHIP_SCHEME == CHIP1424
    wiced_hal_pwm_disable(PWM_CHANNEL);
    // wiced_hal_pwm_configure_pin(PORT_PWM, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);

    wiced_hal_pwm_disable(PWM_CHANNEL1);
    // wiced_hal_pwm_configure_pin(PORT_PWM1, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM1,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM1, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);


#elif CHIP_SCHEME == CHIP2306
    wiced_hal_pwm_disable(PWM_CHANNEL);
    // wiced_hal_pwm_configure_pin(PORT_PWM, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
#elif CHIP_SCHEME == CHIP_DEVKIT
    wiced_hal_pwm_disable(PWM_CHANNEL);
    // wiced_hal_pwm_configure_pin(PORT_PWM, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);

    wiced_hal_pwm_disable(PWM_CHANNEL1);
    // wiced_hal_pwm_configure_pin(PORT_PWM1, WICED_GPIO);
    wiced_hal_gpio_select_function(PORT_PWM1,WICED_GPIO);
    wiced_hal_gpio_configure_pin(PORT_PWM1, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);

#else
#error 方案错误
#endif
}

//#define DIM_LBOUND (45)  //100*sqrt(0.2)
#define DIM_LBOUND (100)  //100*sqrt(1)
#define DIM_UBOUND (989)  //100*sqrt(98)  

uint16_t linner_adjust(uint16_t level)
{
    
#if 0
    float temp;
    temp = level;
    temp *= 85;
    temp /= 65535;
    temp = temp * temp;
    temp /= 100;
    // temp *= 90;
    // temp /= 100;
    temp += 15;
    // temp += 0.5;
    temp *= 65535;
    temp /= 100;
    if (temp > 65535)
        temp = 65535;
    if (temp < 0)
        temp = 0;
#endif

#if 1 
    float temp;
//    uint32_t temp2;
    temp=level;
    // linear projection
    if(temp>=655)
    {
       temp=((100*temp-65535)*(DIM_UBOUND-DIM_LBOUND)+99*65535*(DIM_LBOUND))/99000;
    }
    else
    {
      temp=(DIM_LBOUND*temp+5)/10;
    }
    //Eye Brightness curve
    temp=temp*temp/65535;
#endif 
    level = temp;
#if CHIP_SCHEME == CHIP5600
    return level;
#elif CHIP_SCHEME == CHIP58834
    uint32_t temp = level;
    temp *= 95;
    temp /= 100;
    temp += 5;
    return temp;
#elif CHIP_SCHEME == CHIP6322
    return 65535 - level;
#elif CHIP_SCHEME == CHIP1424
    return level;
#elif CHIP_SCHEME == CHIP2306
    if (level > 328 && level < 840)
        level = 840;
    return level;
#elif CHIP_SCHEME == CHIP_DEVKIT
    return level;
#else
#error 方案错误
#endif
}

void pwm_set(uint16_t level)
{
    pwm_config_t pwm_config;

    LOG_DEBUG("PWM_SET in: %d\n", level);

    // level = linner_adjust(level);
    // LOG_DEBUG("level: %d\n", level);

#define PWM_UBOUND (((PWM_INP_CLK_IN_HZ) / (PWM_FREQ_IN_HZ)))
#define DIVISOR (65536 / PWM_UBOUND)
#define ROUNDING(pwm) ((pwm + (DIVISOR / 2)) / (DIVISOR))
#define PWM(level) (ROUNDING(level) ? ROUNDING(level) : 0)
// 测试
#if PWM(0) != 0
#error 转换宏写错了
#endif
#if PWM(1) != 0
#error 转换宏写错了
#endif
#if PWM((DIVISOR / 2) - 1) != 0
#error 转换宏写错了
#endif
#if PWM((DIVISOR / 2)) != 1
#error 转换宏写错了
#endif
#if PWM(65535) != PWM_UBOUND
#error 转换宏写错了
#endif
#if PWM(65536) != PWM_UBOUND
#error 转换宏写错了
#endif
#if PWM(65536 - (DIVISOR / 2) - 1) != PWM_UBOUND - 1
#error 转换宏写错了
#endif
#if PWM(32767) != (PWM_UBOUND / 2)
#error 转换宏写错了
#endif
#if PWM(32768) != PWM_UBOUND / 2
#error 转换宏写错了
#endif
    // 测试结束
    pwm_config.init_count = 65536 - PWM_UBOUND;
    pwm_config.toggle_count = 65536 - (PWM_UBOUND - PWM(level));

    
    LOG_DEBUG("pwm_config.init_count: %d\n", pwm_config.init_count);
    LOG_DEBUG("pwm_config.toggle_count: %d\n", pwm_config.toggle_count);
    
    if(pwm_config.init_count > 65500)
    {
        pwm_config.init_count = 65500;
    }
    if(pwm_config.toggle_count > 65529)
    {
        pwm_config.toggle_count = 65529;
    }
    //保证PWM的脉宽不小于2us
    if(pwm_config.toggle_count < pwm_config.init_count+2*PWM_INP_CLK_IN_HZ/1000000)
    {
        pwm_config.toggle_count = pwm_config.init_count+2*PWM_INP_CLK_IN_HZ/1000000;
    }
    // PWM占空比最小为1%
    if(pwm_config.toggle_count < (pwm_config.init_count+PWM_UBOUND/100))
    {
        pwm_config.toggle_count = pwm_config.init_count + PWM_UBOUND/100;
    }

    //LOG_VERBOSE("init: %d toggle: %d diff: %d \n", pwm_config.init_count, pwm_config.toggle_count, pwm_config.toggle_count - pwm_config.init_count);
    

#if CHIP_SCHEME == CHIP5600
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP58834
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 50, PWM_FREQ_IN_HZ_CTL, &pwm_config);
    wiced_hal_pwm_change_values(CTL_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP6322
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP1424
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP2306
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP_DEVKIT
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#else
#error 方案错误
#endif
}


void pwm_set1(uint16_t level)
{
    pwm_config_t pwm_config;

    LOG_DEBUG("PWM_SET1 in: %d\n", level);

    // level = linner_adjust(level);
    // LOG_DEBUG("level1: %d\n", level);

#define PWM_UBOUND (((PWM_INP_CLK_IN_HZ) / (PWM_FREQ_IN_HZ)))
#define DIVISOR (65536 / PWM_UBOUND)
#define ROUNDING(pwm) ((pwm + (DIVISOR / 2)) / (DIVISOR))
#define PWM(level) (ROUNDING(level) ? ROUNDING(level) : 0)
// 测试
#if PWM(0) != 0
#error 转换宏写错了
#endif
#if PWM(1) != 0
#error 转换宏写错了
#endif
#if PWM((DIVISOR / 2) - 1) != 0
#error 转换宏写错了
#endif
#if PWM((DIVISOR / 2)) != 1
#error 转换宏写错了
#endif
#if PWM(65535) != PWM_UBOUND
#error 转换宏写错了
#endif
#if PWM(65536) != PWM_UBOUND
#error 转换宏写错了
#endif
#if PWM(65536 - (DIVISOR / 2) - 1) != PWM_UBOUND - 1
#error 转换宏写错了
#endif
#if PWM(32767) != (PWM_UBOUND / 2)
#error 转换宏写错了
#endif
#if PWM(32768) != PWM_UBOUND / 2
#error 转换宏写错了
#endif
    // 测试结束
    pwm_config.init_count = 65536 - PWM_UBOUND;
    pwm_config.toggle_count = 65536 - (PWM_UBOUND - PWM(level));

    
    LOG_DEBUG("pwm_config.init_count1: %d\n", pwm_config.init_count);
    LOG_DEBUG("pwm_config.toggle_count1: %d\n", pwm_config.toggle_count);
    
    if(pwm_config.init_count > 65500)
    {
        pwm_config.init_count = 65500;
    }
    if(pwm_config.toggle_count > 65529)
    {
        pwm_config.toggle_count = 65529;
    }
    //保证PWM的脉宽不小于2us
    if(pwm_config.toggle_count < pwm_config.init_count+2*PWM_INP_CLK_IN_HZ/1000000)
    {
        pwm_config.toggle_count = pwm_config.init_count+2*PWM_INP_CLK_IN_HZ/1000000;
    }
    //PWM占空比最小为1%
    if(pwm_config.toggle_count < (pwm_config.init_count+PWM_UBOUND/100))
    {
        pwm_config.toggle_count = pwm_config.init_count + PWM_UBOUND/100;
    }

    //LOG_VERBOSE("init: %d toggle: %d diff: %d \n", pwm_config.init_count, pwm_config.toggle_count, pwm_config.toggle_count - pwm_config.init_count);
    

#if CHIP_SCHEME == CHIP5600
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP58834
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
    wiced_hal_pwm_get_params(PWM_INP_CLK_IN_HZ, 50, PWM_FREQ_IN_HZ_CTL, &pwm_config);
    wiced_hal_pwm_change_values(CTL_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP6322
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP1424
    wiced_hal_pwm_change_values(PWM_CHANNEL1, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP2306
    wiced_hal_pwm_change_values(PWM_CHANNEL, pwm_config.toggle_count, pwm_config.init_count);
#elif CHIP_SCHEME == CHIP_DEVKIT
    wiced_hal_pwm_change_values(PWM_CHANNEL1, pwm_config.toggle_count, pwm_config.init_count);
#else
#error 方案错误
#endif
}