#include "stm32f10x.h"

#define LONG_PRESS_MS      2000
#define DOUBLE_CLICK_MS    500
#define DEBOUNCE_MS        30

#define KEY1 0
#define KEY2 1
#define KEY3 2

#define KEY_EVENT_NONE          0
#define KEY_EVENT_SINGLE_CLICK  1
#define KEY_EVENT_DOUBLE_CLICK  2
#define KEY_EVENT_LONG_PRESS    3

typedef enum {
    KS_RELEASED = 0,
    KS_PRESS_DEBOUNCE,
    KS_PRESSED,
    KS_RELEASE_DEBOUNCE,
    KS_WAIT_DOUBLE,
    KS_LONG_PRESSED,   
    KS_IGNORE 
} KeyState;

typedef struct {
    uint8_t id;
    GPIO_TypeDef* port;
    uint16_t pin;
    KeyState state;
    uint16_t timer; 
    uint8_t event_flag;
    uint8_t event;
    uint8_t pending_click; 
} Key_Type;

static Key_Type keys[3] = {
    {KEY1, GPIOB, GPIO_Pin_14, KS_RELEASED, 0, 0, KEY_EVENT_NONE, 0},
    {KEY2, GPIOB, GPIO_Pin_13, KS_RELEASED, 0, 0, KEY_EVENT_NONE, 0},
    {KEY3, GPIOB, GPIO_Pin_12, KS_RELEASED, 0, 0, KEY_EVENT_NONE, 0}
};

void Key_Init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void Key_Scan(void) {
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t cur = (GPIO_ReadInputDataBit(keys[i].port, keys[i].pin) == Bit_RESET) ? 1 : 0;

        switch (keys[i].state) {
            case KS_RELEASED:
                if (cur) {
                    keys[i].state = KS_PRESS_DEBOUNCE;
                    keys[i].timer = 0;
                }
                break;

            case KS_PRESS_DEBOUNCE:
                if (cur) {
                    keys[i].timer++;
                    if (keys[i].timer >= DEBOUNCE_MS) {
                        keys[i].state = KS_PRESSED;
                        keys[i].timer = 0;
                        keys[i].pending_click = 0;
                    }
                } else {
                    keys[i].state = KS_RELEASED;
                }
                break;

            case KS_PRESSED:
                if (cur) {
                    keys[i].timer++;
                    if (keys[i].timer >= LONG_PRESS_MS && !keys[i].event_flag) {
                        keys[i].event = KEY_EVENT_LONG_PRESS;
                        keys[i].event_flag = 1;
                        keys[i].pending_click = 0;
                        keys[i].state = KS_LONG_PRESSED; 
                        keys[i].timer = 0;
                    }
                } else { 
                    if (keys[i].timer < LONG_PRESS_MS) {
                        keys[i].state = KS_WAIT_DOUBLE;
                        keys[i].timer = 0;
                        keys[i].pending_click = 1;
                    } else {
                        keys[i].state = KS_RELEASED;
                        keys[i].timer = 0;
                    }
                }
                break;

            case KS_RELEASE_DEBOUNCE:
                if (cur) {
                    keys[i].state = KS_PRESSED;
                    keys[i].timer = 0;
                } else {
                    keys[i].timer++;
                    if (keys[i].timer >= DEBOUNCE_MS) {
                        if (keys[i].pending_click && !keys[i].event_flag) {
                            keys[i].state = KS_WAIT_DOUBLE;
                            keys[i].timer = 0;
                        } else {
                            keys[i].state = KS_RELEASED;
                        }
                    }
                }
                break;

            case KS_WAIT_DOUBLE:
                if (cur) { 
                    if (keys[i].timer <= DOUBLE_CLICK_MS && keys[i].pending_click) {
                        keys[i].event = KEY_EVENT_DOUBLE_CLICK;
                        keys[i].event_flag = 1;
                        keys[i].pending_click = 0;
                        keys[i].state = KS_IGNORE;
                        keys[i].timer = 0;
                    } else {
                        keys[i].state = KS_PRESS_DEBOUNCE;
                        keys[i].timer = 0;
                        keys[i].pending_click = 0;
                    }
                } else {
                    keys[i].timer++;
                    if (keys[i].timer > DOUBLE_CLICK_MS) {
                        if (keys[i].pending_click) {
                            keys[i].event = KEY_EVENT_SINGLE_CLICK;
                            keys[i].event_flag = 1;
                            keys[i].pending_click = 0;
                        }
                        keys[i].state = KS_RELEASED;
                        keys[i].timer = 0;
                    }
                }
                break;

            case KS_LONG_PRESSED:
                if (!cur) {
                    keys[i].state = KS_RELEASED;
                    keys[i].timer = 0;
                }
                break;

            case KS_IGNORE:
                if (!cur) {
                    keys[i].state = KS_RELEASED;
                    keys[i].timer = 0;
                }
                break;

            default:
                keys[i].state = KS_RELEASED;
                break;
        }
    }
}

uint8_t Key_GetEvent(uint8_t key_id) {
    if (key_id > KEY3) return KEY_EVENT_NONE;
    if (keys[key_id].event_flag) {
        keys[key_id].event_flag = 0;
        uint8_t evt = keys[key_id].event;
        keys[key_id].event = KEY_EVENT_NONE;
        return evt;
    }
    return KEY_EVENT_NONE;
}

uint8_t Key_IsPressed(uint8_t key_id) {
    if (key_id > KEY3) return 0;
    return (GPIO_ReadInputDataBit(keys[key_id].port, keys[key_id].pin) == Bit_RESET);
}

void Key_ClearAllEvents(void) {
    for (uint8_t i = 0; i < 3; i++) {
        keys[i].event_flag = 0;
        keys[i].event = KEY_EVENT_NONE;
        keys[i].pending_click = 0;
    }
}
