/* 同一控制结构用于Buck/Boost；沿用原HAL句柄、函数名和观察变量。 */
#include "app_control.h"
#include "platform.h"

volatile PI_CTRL_DATA_DEF voltage_pi;
volatile PI_CTRL_DATA_DEF current_pi;
volatile PI_CTRL_DATA_DEF voltage_only_pi;
volatile float output_voltage_target; /* Buck目标VBAT；Boost目标VBUS，单位V。 */
volatile float current_reference;
volatile float current_limit;
volatile float pwm_limit;        /* 主动开关最大导通计数 */
volatile float pwm_duty_counts;        /* 主动开关导通计数；Boost不是GPIO0高电平计数 */
volatile float openloop_command; /* 百分数：5表示5%，0表示停机 */
volatile uint8_t power_output_enabled;
volatile uint8_t control_fault_latched;
static int8_t key_step_direction;
volatile uint32_t power_mode = DCDC_DEFAULT_POWER_MODE;     /* 运行中可写：0 Buck，1 Boost */
volatile uint32_t control_mode = DCDC_DEFAULT_CONTROL_MODE; /* 运行中可写：0电流，1双环，2开环，3仅Buck电压 */
volatile uint32_t active_power_mode = MODE_INACTIVE;
volatile uint32_t active_control_mode = MODE_INACTIVE;
volatile CONTROL_SETTINGS buck_current_settings = {BUCK_CURRENT_KP, BUCK_CURRENT_KI, 0, 0};
volatile CONTROL_SETTINGS buck_dual_settings = {BUCK_DUAL_CURRENT_KP, BUCK_DUAL_CURRENT_KI, BUCK_VOLTAGE_KP, BUCK_VOLTAGE_KI};
volatile CONTROL_SETTINGS boost_current_settings = {BOOST_CURRENT_KP, BOOST_CURRENT_KI, 0, 0};
volatile CONTROL_SETTINGS boost_dual_settings = {BOOST_DUAL_CURRENT_KP, BOOST_DUAL_CURRENT_KI, BOOST_VOLTAGE_KP, BOOST_VOLTAGE_KI};
volatile CONTROL_SETTINGS buck_voltage_settings = {0, 0, BUCK_SINGLE_VOLTAGE_KP, BUCK_SINGLE_VOLTAGE_KI};
static uint32_t pending_power, pending_control;
static uint8_t arm_pending;
static uint16_t mode_stable_ticks;

static uint8_t BuckControl_ModeValid(uint32_t power, uint32_t control)
{
    return power <= POWER_BOOST && control <= CONTROL_VOLTAGE &&
           !(power == POWER_BOOST && control == CONTROL_VOLTAGE);
}

static void BuckControl_LoadGains(void)
{
    volatile CONTROL_SETTINGS *settings;
    if (active_power_mode == POWER_BOOST)
        settings = (active_control_mode == CONTROL_DUAL) ? &boost_dual_settings : &boost_current_settings;
    else
        settings = (active_control_mode == CONTROL_DUAL) ? &buck_dual_settings : &buck_current_settings;
    current_pi.Kp = settings->current_kp;
    current_pi.Ki = settings->current_ki;
    current_pi.Kd = 0.0f;
    voltage_pi.Kp = settings->voltage_kp;
    voltage_pi.Ki = settings->voltage_ki;
    voltage_pi.Kd = 0.0f;
    voltage_only_pi.Kp = buck_voltage_settings.voltage_kp;
    voltage_only_pi.Ki = buck_voltage_settings.voltage_ki;
    voltage_only_pi.Kd = 0.0f;
}

static uint8_t BuckControl_Finite(float value)
{
    union { float value; uint32_t bits; } representation;
    representation.value = value;
    return (representation.bits & 0x7F800000U) != 0x7F800000U;
}

static void BuckControl_DisableOutput(void)
{
    arm_pending=0U;
    GPIO_writePin(POWER_EN,0U);
    power_output_enabled = 0U;
}

static void BuckControl_Stop(void)
{
    BuckControl_DisableOutput();
    /* SD# is low; keep carrier bounded for the next period-load. */
    pwm_duty_counts = 0.0f;
    voltage_pi.Out = voltage_pi.ui = voltage_pi.up = 0.0f;
    current_pi.Out = current_pi.ui = current_pi.up = 0.0f;
    voltage_only_pi.Out = voltage_only_pi.ui = voltage_only_pi.up = 0.0f;
}

static uint8_t BuckControl_PiValid(volatile PI_CTRL_DATA_DEF *pi)
{
    return BuckControl_Finite(pi->Kp) && pi->Kp >= 0.0f && pi->Kp <= 100.0f &&
           BuckControl_Finite(pi->Ki) && pi->Ki >= 0.0f &&
           pi->Ki <= 100.0f / DCDC_CONTROL_PERIOD_S &&
           BuckControl_Finite(pi->Kd) && pi->Kd == 0.0f;
}

static void BuckControl_ApplyMode(void)
{
    float arr = (float)EPWM_getTimeBasePeriod(POWER_PWM_BASE);
    voltage_pi = (PI_CTRL_DATA_DEF){0};
    current_pi = (PI_CTRL_DATA_DEF){0};
    voltage_only_pi = (PI_CTRL_DATA_DEF){0};
    BuckControl_LoadGains();
    voltage_pi.Umax = DCDC_DUAL_CURRENT_MAX_A;

    current_pi.Umax = arr * DCDC_PWM_MAX_PERCENT / 100.0f;
    current_pi.Umin = arr * DCDC_PWM_MIN_PERCENT / 100.0f;

    voltage_only_pi.Umax = arr * DCDC_PWM_MAX_PERCENT / 100.0f;
    current_limit = DCDC_OVERCURRENT_A;
    pwm_limit = arr * DCDC_PWM_MAX_PERCENT / 100.0f;
    /* 所有模式均复位停机；闭环按键启动，开环人工写openloop_command。 */
    current_reference = openloop_command = 0.0f;
    output_voltage_target = 0.0f;
    key_step_direction = 1;
    BuckControl_Stop();
}

/* 仅上电调用；切换不清故障锁存，必须安全反馈且目标为0才能解锁。 */
void BuckControl_ParameterInit(void)
{
    control_fault_latched = 0U;
    active_power_mode = power_mode;
    active_control_mode = control_mode;
    pending_power = power_mode;
    pending_control = control_mode;
    mode_stable_ticks = 0U;
    if (!BuckControl_ModeValid(power_mode, control_mode))
        active_power_mode = active_control_mode = MODE_INACTIVE;
    BuckControl_ApplyMode();
}

/* 每次请求变化先停机；两个请求稳定10ms后接纳，不自动恢复旧目标。 */
static uint8_t BuckControl_CheckMode(void)
{
    uint32_t requested_power = power_mode;
    uint32_t requested_control = control_mode;
    if (requested_power == active_power_mode && requested_control == active_control_mode &&
        BuckControl_ModeValid(requested_power, requested_control)) return 1U;
    BuckControl_Stop();
    current_reference = output_voltage_target = openloop_command = 0.0f;
    active_power_mode = active_control_mode = MODE_INACTIVE;
    if (requested_power != pending_power || requested_control != pending_control)
    {
        pending_power = requested_power;
        pending_control = requested_control;
        mode_stable_ticks = 0U;
    }
    if (!BuckControl_ModeValid(requested_power, requested_control)) return 0U;
    if (++mode_stable_ticks < 200U) return 0U;
    active_power_mode = requested_power;
    active_control_mode = requested_control;
    BuckControl_ApplyMode();
    return 0U; /* 切换完成这拍仍停机；下一拍才接受新目标。 */
}

/* 同一按键：电流环调A，双环/单电压环调V，端点折返；开环不响应。 */
void BuckControl_KeyPressed(void)
{
    float target;
    bool interrupt_state = Interrupt_disableGlobal();
    float minimum, maximum, step;

    if (power_mode != active_power_mode || control_mode != active_control_mode ||
        !BuckControl_ModeValid(active_power_mode, active_control_mode) || active_control_mode == CONTROL_OPEN)
    {
        if (!interrupt_state) Interrupt_enableGlobal();
        return;
    }
    minimum = (DCDC_CONTROL_MODE == CONTROL_CURRENT) ? DCDC_CURRENT_MIN_A : DCDC_VOLTAGE_MIN_V;
    maximum = (DCDC_CONTROL_MODE == CONTROL_CURRENT) ? DCDC_CURRENT_MAX_A : DCDC_VOLTAGE_MAX_V;
    step = (DCDC_CONTROL_MODE == CONTROL_CURRENT) ? DCDC_CURRENT_STEP_A : DCDC_VOLTAGE_STEP_V;
    target = (active_control_mode == CONTROL_CURRENT) ? current_reference : output_voltage_target;
    if (!BuckControl_Finite(target) || target < minimum - 0.0001f || target > maximum + 0.0001f)
    {
        key_step_direction = 1;
        target = minimum;
    }
    else
    {
        if (target >= maximum - 0.0001f) key_step_direction = -1;
        else if (target <= minimum + 0.0001f) key_step_direction = 1;
        target = limit_value(target + (float)key_step_direction * step, minimum, maximum);
    }
    if (active_control_mode == CONTROL_CURRENT) current_reference = target;
    else output_voltage_target = target;
    if (!interrupt_state) Interrupt_enableGlobal();
}

void BuckControl_EmergencyStop(uint8_t fault)
{
    (void)fault;
    control_fault_latched=1U;
    BuckControl_Stop();
}
void BuckControl_Start(void)
{
    BuckControl_Stop(); BuckAdc_Start(); PowerTiming_Start();
}
void BuckControl_Tick(uint16_t down)
{
    float current, voltage, target, command, maximum;
    float arr;
    uint16_t voltage_raw;
    if(down)
    {
        if(arm_pending && adc_error_count==0U && !control_fault_latched &&
           power_mode==active_power_mode && control_mode==active_control_mode &&
           GPIO_readPin(POWER_PWM_EPWMA_GPIO)==0U)
        { GPIO_writePin(POWER_EN,1U); power_output_enabled=1U; arm_pending=0U; }
        return;
    }
    if (!BuckControl_CheckMode()) return;
    BuckControl_LoadGains();
    arr = (float)EPWM_getTimeBasePeriod(POWER_PWM_BASE);
    if (adc_frame_count == 0U || adc_error_count != 0U)
    {
        BuckControl_Stop();
        return;
    }
    BuckAdc_ReadFeedback(&current, &voltage, &voltage_raw);
    target = (active_control_mode == CONTROL_OPEN) ? openloop_command :
             ((active_control_mode == CONTROL_CURRENT) ? current_reference : output_voltage_target);
    if (BuckControl_Finite(target) && target <= 0.0f)
    {
        /* 只有确认采样未饱和且电压/电流已回到安全范围才允许清除锁存。 */
        if (BuckControl_Finite(current) && current < current_limit && current > -current_limit &&
            BuckControl_Finite(voltage) && voltage < DCDC_OVERVOLTAGE_V &&
            voltage_raw < 4090U && adc_current_raw > 5U && adc_current_raw < 4090U)
            control_fault_latched = 0U;
        BuckControl_Stop();
        return;
    }
    if (!BuckControl_Finite(target) || !BuckControl_Finite(pwm_limit) ||
        pwm_limit <= arr * DCDC_PWM_MIN_PERCENT / 100.0f ||
        pwm_limit > arr * DCDC_PWM_MAX_PERCENT / 100.0f ||
        !BuckControl_Finite(current_limit) || current_limit <= DCDC_DUAL_CURRENT_MAX_A ||
        current_limit <= DCDC_CURRENT_MAX_A || current_limit > DCDC_OVERCURRENT_A)
    {
        BuckControl_Stop();
        return;
    }
    if (!BuckControl_Finite(current) || !BuckControl_Finite(voltage) ||
        current >= current_limit || current <= -current_limit ||
        voltage >= DCDC_OVERVOLTAGE_V || voltage_raw >= 4090U ||
        adc_current_raw <= 5U || adc_current_raw >= 4090U)
        control_fault_latched = 1U;
    if (control_fault_latched)
    {
        BuckControl_Stop();
        return;
    }
    maximum = pwm_limit;
    if (DCDC_CONTROL_MODE == CONTROL_OPEN)
    {
        if (target > DCDC_OPEN_MAX_PERCENT)
        {
            BuckControl_Stop(); return;
        }
        command = limit_value(arr * target / 100.0f,
                              arr * DCDC_PWM_MIN_PERCENT / 100.0f, maximum);
    }
    else if (DCDC_CONTROL_MODE == CONTROL_VOLTAGE)
    {
        if (target < DCDC_VOLTAGE_MIN_V || target > DCDC_VOLTAGE_MAX_V ||
            !BuckControl_PiValid(&voltage_only_pi))
        {
            BuckControl_Stop(); return;
        }
        voltage_only_pi.Umax = maximum;
        command = pi_func_percent(&voltage_only_pi, target, voltage, arr, DCDC_CONTROL_PERIOD_S);
    }
    else
    {
        if (!BuckControl_PiValid(&current_pi))
        {
            BuckControl_Stop(); return;
        }
        if (DCDC_CONTROL_MODE == CONTROL_DUAL)
        {
            if (target < DCDC_VOLTAGE_MIN_V || target > DCDC_VOLTAGE_MAX_V ||
                !BuckControl_PiValid(&voltage_pi))
            {
                BuckControl_Stop(); return;
            }
            voltage_pi.Umax = DCDC_DUAL_CURRENT_MAX_A;
            voltage_pi.Umin = 0.0f;
            voltage_pi.ui = limit_value(voltage_pi.ui, 0.0f, voltage_pi.Umax);
            target = pi_func_seconds(&voltage_pi, target, voltage, DCDC_CONTROL_PERIOD_S);
        }
        else if (target < DCDC_CURRENT_MIN_A - 0.0001f || target > DCDC_CURRENT_MAX_A + 0.0001f)
        {
            BuckControl_Stop(); return;
        }
        current_pi.Umax = maximum;
        current_pi.Umin = arr * DCDC_PWM_MIN_PERCENT / 100.0f;
        command = pi_func_percent(&current_pi, target, current, arr, DCDC_CONTROL_PERIOD_S);
    }
    if (adc_error_count != 0U || !BuckControl_Finite(command))
    {
        BuckControl_Stop(); return;
    }
    if (power_mode != active_power_mode || control_mode != active_control_mode)
    {
        (void)BuckControl_CheckMode();
        return;
    }
    command = limit_value(command, arr * DCDC_PWM_MIN_PERCENT / 100.0f, maximum);
    pwm_duty_counts = command;
    /* IR2104: IN高->高侧HO；Boost低侧LO储能，故GPIO0高电平计数取补数。
       不改EPWM硬件极性，所有控制PI仍是“正误差增加主动开关占空比”。 */
    EPWM_setCounterCompareValue(POWER_PWM_BASE, EPWM_COUNTER_COMPARE_A,
        (uint16_t)((DCDC_POWER_MODE==POWER_BOOST)?arr-command:command));
    if(!power_output_enabled) arm_pending=1U;
}
