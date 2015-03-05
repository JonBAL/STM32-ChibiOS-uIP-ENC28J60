/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef _BOARD_H_
#define _BOARD_H_

/*
 * Setup for STMicroelectronics STM32VL-Discovery board.
 */

/*
 * Идентификатор платы.
 */
#define BOARD_ST_STM32VL_DISCOVERY
#define BOARD_NAME              "ST STM32VL-Discovery"

/* Частоты кварцев на плате */
#define STM32_LSECLK            32768
#define STM32_HSECLK            24000000

/* Тип микроконтроллера, см в ChibiOS/os/hal/platforms/hal_lld.h */
#define STM32F10X_MD

#define GPIOA_ENC_WOL				0
#define GPIOA_ENC_INT				1
#define GPIOA_ENC_RST				2
#define GPIOA_PIN3					3
#define GPIOA_ENC_CS				4
#define GPIOA_ENC_SCK				5
#define GPIOA_ENC_MISO			6
#define GPIOA_ENC_MOSI			7
#define GPIOA_PIN8					8
#define GPIOA_USART1_TX			9
#define GPIOA_USART1_RX			10
#define GPIOA_PIN11					11
#define GPIOA_PIN12					12
#define GPIOA_SWDIO					13
#define GPIOA_SWCLK					14
#define GPIOA_PIN15					15

#define GPIOB_PIN0					0
#define GPIOB_PIN1					1
#define GPIOB_PIN2					2
#define GPIOB_PIN3					3
#define GPIOB_PIN4					4
#define GPIOB_PIN5					5
#define GPIOB_PIN6					6
#define GPIOB_PIN7					7
#define GPIOB_PIN8					8
#define GPIOB_PIN9					9
#define GPIOB_PIN10					10
#define GPIOB_PIN11					11
#define GPIOB_PIN12					12
#define GPIOB_PIN13					13
#define GPIOB_PIN14					14
#define GPIOB_PIN15					15

/*
 * Настройка портов ввода вывода, указанная здесь конфигурация
 * автоматически настраивается после инициализации кода.
 *
 * Расшифровка:
 *   0 - Аналоговый ввод.
 *   1 - Вывод Push Pull на 10 МГц.
 *   2 - Вывод Push Pull на 2 МГц.
 *   3 - Вывод Push Pull на 50 МГц.
 *   4 - Цифровой вход.
 *   5 - Вывод Open Drain на 10 МГц.
 *   6 - Вывод Open Drain на 2 МГц.
 *   7 - Вывод Open Drain на 50 МГц.
 *   8 - Цифровой вход с подтяжкой в зависимости от значения в ODR.
 *   9 - Вывод Push Pull на 10 МГц от периферии.
 *   A - Вывод Push Pull на 2 МГц от периферии.
 *   B - Вывод Push Pull на 50 МГц от периферии.
 *   C - Зарезервировано.
 *   D - Вывод Open Drain на 10 МГц от периферии.
 *   E - Вывод Open Drain на 2 МГц от периферии.
 *   F - Вывод Open Drain на 50 МГц от периферии.
 * В референсной доке по STM32 расписаны подробности.
 */

/*
 * Port A setup.
 */
#define VAL_GPIOACRL            0xB4B34348      /*  PA7...PA0 */
#define VAL_GPIOACRH            0x888884B8      /* PA15...PA8 */
#define VAL_GPIOAODR            0xFFFFFFFF

/*
 * Port B setup.
 */
#define VAL_GPIOBCRL            0x33333333      /*  PB7...PB0 */
#define VAL_GPIOBCRH            0x33333333      /* PB15...PB8 */
#define VAL_GPIOBODR            0xFFFFFFFF

/*
 * Port C setup.
 */
#define VAL_GPIOCCRL            0x88888888      /*  PC7...PC0 */
#define VAL_GPIOCCRH            0x88888888      /* PC15...PC8 */
#define VAL_GPIOCODR            0xFFFFFCFF

/*
 * Port D setup.
 * Everything input with pull-up except:
 * PD0  - Normal input (XTAL).
 * PD1  - Normal input (XTAL).
 */
#define VAL_GPIODCRL            0x88888844      /*  PD7...PD0 */
#define VAL_GPIODCRH            0x88888888      /* PD15...PD8 */
#define VAL_GPIODODR            0xFFFFFFFF

/*
 * Port E setup.
 * Everything input with pull-up except:
 */
#define VAL_GPIOECRL            0x88888888      /*  PE7...PE0 */
#define VAL_GPIOECRH            0x88888888      /* PE15...PE8 */
#define VAL_GPIOEODR            0xFFFFFFFF

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif
  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* _BOARD_H_ */
