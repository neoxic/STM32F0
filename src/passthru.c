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

#define PORT1 GPIOA
#define PORT2 GPIOA

#define PIN1 4
#define PIN2 13

void main(void) {
	rcc_clock_setup_in_hsi_out_48mhz(); // PCLK=48MHz
	RCC_AHBENR = RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOFEN;
	GPIO_PUPDR(PORT1) &= ~(3 << (PIN1 << 1));
	GPIO_PUPDR(PORT2) &= ~(3 << (PIN2 << 1));
	GPIO_PUPDR(PORT1) |= 1 << (PIN1 << 1);
	GPIO_PUPDR(PORT2) |= 1 << (PIN2 << 1);
	GPIO_OSPEEDR(PORT1) |= 3 << (PIN1 << 1);
	GPIO_OSPEEDR(PORT2) |= 3 << (PIN2 << 1);
	GPIO_MODER(PORT1) &= ~(3 << (PIN1 << 1));
	GPIO_MODER(PORT2) &= ~(3 << (PIN2 << 1));
	for (;;) {
		if (!(GPIO_MODER(PORT1) & (1 << (PIN1 << 1)))) {
			if (!(GPIO_IDR(PORT1) & (1 << PIN1))) {
				GPIO_BSRR(PORT2) = (1 << PIN2) << 16;
				GPIO_MODER(PORT2) |= 1 << (PIN2 << 1);
			} else if (GPIO_MODER(PORT2) & (1 << (PIN2 << 1))) {
				GPIO_BSRR(PORT2) = 1 << PIN2;
				GPIO_MODER(PORT2) &= ~(1 << (PIN2 << 1));
			}
		}
		if (!(GPIO_MODER(PORT2) & (1 << (PIN2 << 1)))) {
			if (!(GPIO_IDR(PORT2) & (1 << PIN2))) {
				GPIO_BSRR(PORT1) = (1 << PIN1) << 16;
				GPIO_MODER(PORT1) |= 1 << (PIN1 << 1);
			} else if (GPIO_MODER(PORT1) & (1 << (PIN1 << 1))) {
				GPIO_BSRR(PORT1) = 1 << PIN1;
				GPIO_MODER(PORT1) &= ~(1 << (PIN1 << 1));
			}
		}
	}
}
