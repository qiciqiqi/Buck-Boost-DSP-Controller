#include "platform.h"
#define KEY_DEBOUNCE_MS 20U
static uint16_t key_last_sample;
static uint16_t key_stable_state;
static uint32_t key_changed_at;

void AppKey_Init(void)
{
  key_last_sample = GPIO_readPin(USER_KEY);
  key_stable_state = 0U;
  key_changed_at = Platform_Millis();
}

/* 主循环持续轮询：按下/松开均需稳定20ms；长按不连发，不使用HAL_Delay。 */
uint8_t AppKey_PollEvent(void)
{
  uint16_t sample = GPIO_readPin(USER_KEY);
  uint32_t now = Platform_Millis();

  if (sample != key_last_sample)
  {
    key_last_sample = sample;
    key_changed_at = now;
  }
  else if ((sample != key_stable_state) &&
           ((uint32_t)(now - key_changed_at) >= KEY_DEBOUNCE_MS))
  {
    key_stable_state = sample;
    return (sample == 0U) ? 1U : 0U;
  }
  return 0U;
}

