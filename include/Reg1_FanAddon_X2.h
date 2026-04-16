#pragma once

// OpenKNX Reg1 Fan-Addon-X2 board

// --- Reg1 Base pins (matches SEN-REG1-PowerMeter-3Phase) ---
#define PROG_LED_PIN 25
#define PROG_LED_PIN_ACTIVE_ON HIGH
#define PROG_BUTTON_PIN 23
#define PROG_BUTTON_PIN_INTERRUPT_ON FALLING
#define SAVE_INTERRUPT_PIN 5
#define KNX_UART_NUM 0
#define KNX_UART_RX_PIN 1
#define KNX_UART_TX_PIN 0

// No dedicated status LED on Reg1 — reuse PROG_LED
#define STATUS_LED_PIN PROG_LED_PIN

// --- Fan 1 (HW Channel A) ---
#define FAN1_S1_PWM_PIN 18   // PWM_A: via U3 level shifter -> Q1 open-drain
#define FAN1_SW_PIN     29   // POW_A: via U3 level shifter -> Q7/Q6 high-side switch
#define FAN1_TACHO_PIN  27   // TACHO_A: isolated via U2 optocoupler (active low)

// --- Fan 2 (HW Channel B) ---
#define FAN2_S1_PWM_PIN 28   // PWM_B: via U3 level shifter -> Q2 open-drain
#define FAN2_SW_PIN     26   // POW_B: via U3 level shifter -> Q9/Q8 high-side switch
#define FAN2_TACHO_PIN  17   // TACHO_B: isolated via U1 optocoupler (active low)
// #define PIN_SPARE       16   // Exposed on J1 pin 10, not connected on PCB

// --- S2 pins unused (single-motor fans, no push-pull Maico operation) ---
#define FAN1_S2_PWM_PIN -1
#define FAN2_S2_PWM_PIN -1
