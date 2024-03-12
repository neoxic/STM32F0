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

#define VALVE_MIN 80 // Still closed
#define VALVE_MUL 60 // Input multiplier (%)

#define PUMP_MIN 100 // Minimum duty
#define PUMP_MAX 260 // Maximum duty
#define PUMP_LIM 20 // Acceleration limit

#define DRIVE_MIN 50 // Minimum duty
#define DRIVE_MAX 500 // Maximum duty
#define DRIVE_LIM 20 // Acceleration limit
#define DRIVE_PWM 4 // PWM frequency divider F_PWM=96000/(DRIVE_PWM+1) (comment out for servo PWM)

#define BUZZER_FREQ 1318 // Frequency (Hz) (comment out for active buzzer)

#define VOLT1 3336 // mV
#define VOLT2 3720 // xx.xxV = VOLT1*(R1+R2)/R2

static int input1(int t) {
	t = t < 1500 ? 1500 - t : t - 1500;
	return t < VALVE_MIN ? 0 : VALVE_MUL * (t - VALVE_MIN) / 200;
}

static int input2(int t) {
	return t - 1500;
}

static int input3(int t) {
	if (t < 1450) return 0;
	if (t > 1550) return 2;
	return 1;
}

static int output1(int t) {
	if (!t) return 1500;
	if ((t += PUMP_MIN) > PUMP_MAX) return 1500 + PUMP_MAX;
	return 1500 + t;
}

static int output2(int t) {
	if (t > -DRIVE_MIN && t < DRIVE_MIN) return 1500;
	if (t < -DRIVE_MAX) return 1500 - DRIVE_MAX;
	if (t > DRIVE_MAX) return 1500 + DRIVE_MAX;
	return 1500 + t;
}

#ifdef DRIVE_PWM
static int output3(int t, int *f, int *r) {
	if (t < 1500 - DRIVE_MIN) {
		*f = 0;
		*r = 1;
		return 1500 - t;
	}
	if (t > 1500 + DRIVE_MIN) {
		*f = 1;
		*r = 0;
		return t - 1500;
	}
	*f = 0;
	*r = 0;
	return 0;
}
#endif

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

static int u1, u2, u3;
static int i1, i2, i3, i4, i5;
static int s1, s2;

void update(void) {
	s1 = input3(chv[5]);
	s2 = input3(chv[6]);

	i1 = input1(chv[0]);
	i2 = input1(chv[1]);
	i3 = input2(chv[2]);
	i4 = input2(chv[3]);
	i5 = input1(chv[4]);

	u1 = ramp(output2(i3 + i4), u1, DRIVE_LIM);
	u2 = ramp(output2(i3 - i4), u2, DRIVE_LIM);
	u3 = ramp(output1(i1 + i2 + i5), u3, PUMP_LIM);

#ifdef DRIVE_PWM
	int f1, r1, f2, r2;
	TIM1_CCR2 = output3(u1, &f1, &r1);
	TIM1_CCR3 = output3(u2, &f2, &r2);
	GPIOA_BSRR = (f1 ? 0x02 : 0x20000) | (r1 ? 0x20 : 0x200000); // A1,A5
	GPIOF_BSRR = (f2 ? 0x01 : 0x10000) | (r2 ? 0x02 : 0x020000); // F0,F1
#else
	TIM1_CCR2 = u1;
	TIM1_CCR3 = u2;
#endif
	TIM14_CCR1 = u3;

	static int bm;
	int b = bm;
	if (s2) b = s2 + 2; // Forced blinking
	else if (i3 < -100 || (i4 > -50 && i4 < 50)) b = 0;
	else if (i3 > -50) {
		if (i4 < -250) b = 1; // Left turn
		else if (i4 > 250) b = 2; // Right turn
	}
	if (b != bm) {
		TIM3_CR1 = 0; // Change OC mode atomically
		TIM3_CCMR1 = TIM_CCMR1_OC1M_FORCE_LOW; // Reset OC1REF
		TIM3_CCMR2 = TIM_CCMR2_OC4M_FORCE_LOW; // Reset OC4REF
		if (b & 1) TIM3_CCMR1 = TIM_CCMR1_OC1M_TOGGLE;
		if (b & 2) TIM3_CCMR2 = TIM_CCMR2_OC4M_TOGGLE;
		TIM3_PSC = b & 4 ? 95 : 767; // Strobe
		TIM3_EGR = TIM_EGR_UG;
		TIM3_CR1 = TIM_CR1_CEN;
		bm = b;
	}

	GPIOA_BSRR = i3 < -50 ? 0x20000000 : 0x2000; // A13
	GPIOA_BSRR = s1 ? 0x4000 : 0x40000000; // A14

	WWDG_CR = 0xff; // Reset watchdog
#ifdef DEBUG
	static int n;
	if (++n < 26) return; // 130Hz -> 5Hz
	SCB_SCR = 0; // Resume main loop
	n = 0;
#endif
}

int sensors[3] = {0x100201, 0x000203};

int sensor(int i, int v) {
	switch (i) {
		case 0: // Temperature sensor
			return (v * VOLT1 / 3300 - ST_TSENSE_CAL1_30C) * 800 / (ST_TSENSE_CAL2_110C - ST_TSENSE_CAL1_30C) + 700;
		case 1: // Voltage divider
			return (v * VOLT2) >> 12;
	}
	return 0;
}

void tim3_isr(void) {
	static int n;
	TIM3_SR = ~TIM_SR_UIF;
	if (s2 != 2) goto beep;
	if (++n & 7) return;
	if (n & 0x20) {
		TIM3_CCMR1 = n & 0x08 ? TIM_CCMR1_OC1M_TOGGLE : TIM_CCMR1_OC1M_FORCE_LOW;
		TIM3_CCMR2 = n & 0x10 ? TIM_CCMR2_OC4M_TOGGLE : TIM_CCMR2_OC4M_FORCE_LOW;
	} else {
		TIM3_CCMR1 = n & 0x10 ? TIM_CCMR1_OC1M_TOGGLE : TIM_CCMR1_OC1M_FORCE_LOW;
		TIM3_CCMR2 = n & 0x08 ? TIM_CCMR2_OC4M_TOGGLE : TIM_CCMR2_OC4M_FORCE_LOW;
	}
beep:
	TIM17_CCMR1 = i3 < -50 && TIM17_CCMR1 == TIM_CCMR1_OC1M_FORCE_LOW ? TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 : TIM_CCMR1_OC1M_FORCE_LOW;
}

void main(void) {
	rcc_clock_setup_in_hsi_out_48mhz(); // PCLK=48MHz

	RCC_AHBENR = RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOFEN;
	RCC_APB2ENR = RCC_APB2ENR_SYSCFGCOMPEN | RCC_APB2ENR_ADCEN | RCC_APB2ENR_TIM1EN | RCC_APB2ENR_USART1EN | RCC_APB2ENR_TIM16EN | RCC_APB2ENR_TIM17EN;
	RCC_APB1ENR = RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM14EN | RCC_APB1ENR_WWDGEN;

	// Default GPIO state - output low
	GPIOA_AFRL = 0x51041100; // A2 (USART1_TX), A3 (USART1_RX), A4 (TIM14_CH1), A6 (TIM3_CH1), A7 (TIM17_CH1)
	GPIOA_AFRH = 0x00000220; // A9 (TIM1_CH2), A10 (TIM1_CH3)
	GPIOB_AFRL = 0x00000010; // B1 (TIM3_CH4)
	GPIOA_ODR = 0x2000; // A13 (high)
	GPIOA_PUPDR = 0x00000050; // A2,A3 (pull-up)
	GPIOA_MODER = 0x5569a6a7; // A0 (analog), A2 (USART1_TX), A3 (USART1_RX), A4 (TIM14_CH1), A6 (TIM3_CH1), A7 (TIM17_CH1), A9 (TIM1_CH2), A10 (TIM1_CH3)
	GPIOB_MODER = 0x55555559; // B1 (TIM3_CH4)
	GPIOF_MODER = 0x55555555;

	WWDG_CFR = 0x1ff; // Watchdog timeout 4096*8*64/PCLK=~43ms

	nvic_enable_irq(NVIC_TIM3_IRQ);

#ifdef DRIVE_PWM
	TIM1_PSC = DRIVE_PWM;
	TIM1_ARR = 499;
#else
	TIM1_PSC = 47; // 1MHz
	TIM1_ARR = 3999; // 250Hz
#endif
	TIM1_EGR = TIM_EGR_UG;
	TIM1_CR1 = TIM_CR1_CEN;
	TIM1_BDTR = TIM_BDTR_MOE;
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE | TIM_CCMR2_OC3M_PWM1;
	TIM1_CCER = TIM_CCER_CC2E | TIM_CCER_CC3E;
	TIM1_DIER = TIM_DIER_UIE | TIM_DIER_CC1IE | TIM_DIER_CC4IE;

	TIM3_PSC = 767; // 62.5kHz
	TIM3_ARR = 20832; // 3Hz
	TIM3_EGR = TIM_EGR_UG;
	TIM3_CR1 = TIM_CR1_CEN;
	TIM3_CCMR1 = TIM_CCMR1_OC1M_FORCE_LOW;
	TIM3_CCMR2 = TIM_CCMR2_OC4M_FORCE_LOW;
	TIM3_CCER = TIM_CCER_CC1E | TIM_CCER_CC1P | TIM_CCER_CC4E | TIM_CCER_CC4P;
	TIM3_DIER = TIM_DIER_UIE;

	TIM14_PSC = 47; // 1MHz
	TIM14_ARR = 3999; // 250Hz
	TIM14_EGR = TIM_EGR_UG;
	TIM14_CR1 = TIM_CR1_CEN;
	TIM14_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1;
	TIM14_CCER = TIM_CCER_CC1E;

#ifdef BUZZER_FREQ
	TIM17_ARR = 48000000 / BUZZER_FREQ - 1;
	TIM17_CCR1 = 48000000 / BUZZER_FREQ / 2;
#else
	TIM17_CCR1 = -1;
#endif
	TIM17_EGR = TIM_EGR_UG;
	TIM17_CR1 = TIM_CR1_CEN;
	TIM17_BDTR = TIM_BDTR_MOE;
	TIM17_CCMR1 = TIM_CCMR1_OC1M_FORCE_LOW;
	TIM17_CCER = TIM_CCER_CC1E;

	initsensor();
	initserial();
#ifdef DEBUG
	printf("\n");
	printf("  U1   U2   U3      I1   I2   I3   I4   I5    SW\n");
#endif
	for (;;) {
		SCB_SCR = SCB_SCR_SLEEPONEXIT; // Suspend main loop
		__WFI();
#ifdef DEBUG
		printf("%4d %4d %4d    %4d %4d %4d %4d %4d    %d %d\n",
			u1, u2, u3, i1, i2, i3, i4, i5, s1, s2);
#endif
	}
}
