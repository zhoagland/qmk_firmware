/* Copyright 2026 ZDH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "quantum.h"

// Forward declaration for deferred I2C initialization
void matrix_init_expanders(void);

void early_hardware_init_pre(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (5U * 2U))) | (1U << (5U * 2U));
    GPIOA->BSRR  = (1U << 5U);
    GPIOC->MODER &= ~(3U << (13U * 2U));
}

void early_hardware_init_post(void) {
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (5U * 2U))) | (1U << (5U * 2U));
    GPIOA->BSRR  = (1U << 5U);
    GPIOC->MODER &= ~(3U << (13U * 2U));
}

void keyboard_pre_init_kb(void) {
    // Early hardware setup before USB enumeration
    // Keep this minimal to avoid blocking USB
    gpio_set_pin_output(MODEL_M_HEARTBEAT_LED);
    gpio_write_pin_high(MODEL_M_HEARTBEAT_LED);
    gpio_set_pin_input(MODEL_M_USER_BUTTON);

    keyboard_pre_init_user();
}

void keyboard_post_init_kb(void) {
#if MODEL_M_ENABLE_EXPANDERS
    matrix_init_expanders();
#endif
    
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    static uint32_t last_blink = 0;
    static bool     led_on     = false;

    if ((GPIOC->IDR & (1U << 13U)) == 0) {
        gpio_write_pin_low(MODEL_M_HEARTBEAT_LED);
        housekeeping_task_user();
        return;
    }

    if (timer_elapsed32(last_blink) >= 500) {
        last_blink = timer_read32();
        led_on     = !led_on;
        gpio_write_pin(MODEL_M_HEARTBEAT_LED, led_on);
    }

    housekeeping_task_user();
}

