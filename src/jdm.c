/*
** Copyright (C) Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** This firmware is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This firmware is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this firmware. If not, see <http://www.gnu.org/licenses/>.
*/

#include "common.h"

#define CH1_TRIM 0 // Bucket
#define CH2_TRIM 95 // Lift arm
#define CH5_TRIM 115 // Ripper

#define VALVE_MIN 200 // Still closed
#define VALVE_MAX 300 // Fully open
#define VALVE_MUL 150 // Input multiplier (%)

#define PUMP_MIN 150 // Minimum duty
#define PUMP_MAX 500 // Maximum duty
#define PUMP_LIM 20 // Acceleration limit

#define DRIVE_MIN 50 // Minimum duty
#define DRIVE_MAX 500 // Maximum duty
#define DRIVE_LIM 10 // Acceleration limit

#define VOLT1 3316 // mV
#define VOLT2 3640 // xx.xxV = VOLT1*(R1+R2)/R2

#define FAN_OFF 250
#define FAN_ON 300

static int input1(int t, int *u, int x) {
	*u = t + x;
	t = t < 1500 ? 1500 - t : t - 1500;
	if (t < VALVE_MIN) return 0;
	if (t < VALVE_MAX) return VALVE_MUL * (t - VALVE_MIN) / 200;
	return VALVE_MUL * (t - (VALVE_MIN + VALVE_MAX) / 2) / 100;
}

static int input2(int t) {
	return t - 1500;
}

static int input3(int t) {
	if (t < 1450) return 0;
	if (t > 1550) return 2;
	return 1;
}

static int output1(int t, int *s) {
	if (!(*s = !!t)) return 1500;
	if ((t += PUMP_MIN) > PUMP_MAX) return 1500 + PUMP_MAX;
	return 1500 + t;
}

static int output2(int t) {
	if (t > -DRIVE_MIN && t < DRIVE_MIN) return 1500;
	if (t < -DRIVE_MAX) return 1500 - DRIVE_MAX;
	if (t > DRIVE_MAX) return 1500 + DRIVE_MAX;
	return 1500 + t;
}

static int output3(int t) {
	if (t < 0) return 1500;
	if (t > 500) return 2000;
	return 1500 + t;
}

static int ramp(int t, int u, int x) {
	if (!u || !x) return t;
	if (t < 1500) {
		u -= x;
		if (u > 1450) u = 1450;
		if (t < u) return u;
	} else {
		u += x;
		if (u < 1550) u = 1550;
		if (t > u) return u;
	}
	return t;
}

static int u1, u2, u3, u4, u5, u6, u7, u8;
static int i1, i2, i3, i4, i5;
static int s1, s2, s3;

void update(void) {
	s1 = input3(chv[5]);
	s2 = input3(chv[6]);
	s3 = input3(chv[7]);

	i1 = input1(chv[0], &u1, CH1_TRIM);
	i2 = input1(chv[1], &u2, CH2_TRIM);
	i3 = input2(chv[2]);
	i4 = input2(chv[3]);
	i5 = input1(chv[4], &u3, CH5_TRIM);

	int sl;
	u4 = ramp(output1(i1 + i2 + i5, &sl), u4, PUMP_LIM);
	u5 = ramp(output2(i3 + i4), u5, DRIVE_LIM);
	u6 = ramp(output2(i3 - i4), u6, DRIVE_LIM);
	u7 = s2 ? 2000 : s3 ? 1000 : 1500;
	u8 = output3(i1 + i2 + abs(i3) + abs(i4) + i5);

	TIM3_CCR1 = u1;
	TIM3_CCR2 = u2;
	TIM3_CCR4 = u3;
	TIM14_CCR1 = u4;
	TIM1_CCR2 = u5;
	TIM1_CCR3 = u6;
	TIM1_CCR1 = u7;
	TIM1_CCR4 = u8;

	GPIOA_BSRR = s1 ? 0x4000 : 0x40000000; // A14
	GPIOA_BSRR = sl ? 0x20000000 : 0x2000; // A13

	WWDG_CR = 0xff; // Reset watchdog
#ifdef DEBUG
	static int n;
	if (++n < 26) return; // 130Hz -> 5Hz
	SCB_SCR = 0; // Resume main loop
	n = 0;
#endif
}

int sensors[3] = {0x000201, 0x010203};

int sensor(int i, int v) {
	switch (i) {
		case 0: { // TMP36 sensor
			int t = ((v * VOLT1) >> 12) - 500;
			GPIOA_BSRR = t > FAN_ON ? 0x20 : t < FAN_OFF ? 0x200000 : 0; // A5
			return t + 400;
		}
		case 1: // Voltage divider
			return (v * VOLT2) >> 12;
	}
	return 0;
}

void tim1_brk_up_trg_com_isr(void) {
	TIM1_SR = ~TIM_SR_UIF;
	int br = 0;
	if (TIM1_CCR1) br |= 0x1; // F0 high
	if (TIM1_CCR4) br |= 0x2; // F1 high
	GPIOF_BSRR = br;
}

void tim1_cc_isr(void) {
	int sr = TIM1_SR;
	int br = 0;
	if (sr & TIM_SR_CC1IF) { // F0 low
		TIM1_SR = ~TIM_SR_CC1IF;
		br |= 0x10000;
	}
	if (sr & TIM_SR_CC4IF) { // F1 low
		TIM1_SR = ~TIM_SR_CC4IF;
		br |= 0x20000;
	}
	GPIOF_BSRR = br;
}

void main(void) {
	rcc_clock_setup_in_hsi_out_48mhz(); // PCLK=48MHz

	RCC_AHBENR = RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOFEN;
	RCC_APB2ENR = RCC_APB2ENR_SYSCFGCOMPEN | RCC_APB2ENR_ADCEN | RCC_APB2ENR_TIM1EN | RCC_APB2ENR_USART1EN | RCC_APB2ENR_TIM16EN;
	RCC_APB1ENR = RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM14EN | RCC_APB1ENR_WWDGEN;

	// Default GPIO state - output low
	GPIOA_AFRL = 0x11041100; // A2 (USART1_TX), A3 (USART1_RX), A4 (TIM14_CH1), A6 (TIM3_CH1), A7 (TIM3_CH2)
	GPIOA_AFRH = 0x00000220; // A9 (TIM1_CH2), A10 (TIM1_CH3)
	GPIOB_AFRL = 0x00000010; // B1 (TIM3_CH4)
	GPIOA_ODR = 0x2000; // A13 (high)
	GPIOA_PUPDR = 0x00000050; // A2,A3 (pull-up)
	GPIOA_MODER = 0x5569a6af; // A0,A1 (analog), A2 (USART1_TX), A3 (USART1_RX), A4 (TIM14_CH1), A6 (TIM3_CH1), A7 (TIM3_CH2), A9 (TIM1_CH2), A10 (TIM1_CH3)
	GPIOB_MODER = 0x55555559; // B1 (TIM3_CH4)
	GPIOF_MODER = 0x55555555;

	WWDG_CFR = 0x1ff; // Watchdog timeout 4096*8*64/PCLK=~43ms

	nvic_enable_irq(NVIC_TIM1_BRK_UP_TRG_COM_IRQ);
	nvic_enable_irq(NVIC_TIM1_CC_IRQ);

	TIM1_PSC = 47; // 1MHz
	TIM1_ARR = 7999; // 125Hz
	TIM1_EGR = TIM_EGR_UG;
	TIM1_CR1 = TIM_CR1_CEN;
	TIM1_BDTR = TIM_BDTR_MOE;
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE | TIM_CCMR2_OC3M_PWM1;
	TIM1_CCER = TIM_CCER_CC2E | TIM_CCER_CC3E;
	TIM1_DIER = TIM_DIER_UIE | TIM_DIER_CC1IE | TIM_DIER_CC4IE;

	TIM3_PSC = 47; // 1MHz
	TIM3_ARR = 3999; // 250Hz
	TIM3_EGR = TIM_EGR_UG;
	TIM3_CR1 = TIM_CR1_CEN;
	TIM3_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
	TIM3_CCMR2 = TIM_CCMR2_OC4PE | TIM_CCMR2_OC4M_PWM1;
	TIM3_CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC4E;

	TIM14_PSC = 47; // 1MHz
	TIM14_ARR = 3999; // 250Hz
	TIM14_EGR = TIM_EGR_UG;
	TIM14_CR1 = TIM_CR1_CEN;
	TIM14_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1;
	TIM14_CCER = TIM_CCER_CC1E;

	initsensor();
	initserial();
#ifdef DEBUG
	printf("\n");
	printf("  U1   U2   U3   U4   U5   U6   U7   U8      I1   I2   I3   I4   I5    SW\n");
#endif
	for (;;) {
		SCB_SCR = SCB_SCR_SLEEPONEXIT; // Suspend main loop
		__WFI();
#ifdef DEBUG
		printf("%4d %4d %4d %4d %4d %4d %4d %4d    %4d %4d %4d %4d %4d    %d %d %d\n",
			u1, u2, u3, u4, u5, u6, u7, u8, i1, i2, i3, i4, i5, s1, s2, s3);
#endif
	}
}
