/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* 1000 msec = 1 sec */
#define SECOND_MS 1000
/* 60 sec = 1 h */
#define MINUTE_MS SECOND_MS * 60

#define TIMMER_LEDS_FREQ 2

/* The devicetree node identifiers for the "led0", "led1", "led2" & "led3" aliases. */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

//***Otras opciones: Un solo contador y hacer / % 60 + / % 10 para las unidades o tener 4 contadores***
static int hours_counter, minutes_counter = 0;

/*
 * Method in charge of configuring the LEDs we will use as outputs,
 * they are set as inactive so they start with a logic '0'.
 * RETURN:
 * 0 -> No errors
 * -1 -> An error ocurred when configuring one of the leds
 */
int configure_LEDS()
{
	int ret;

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (ret < 0)
	{
		return -1;
	}

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	if (ret < 0)
	{
		return -1;
	}

	ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
	if (ret < 0)
	{
		return -1;
	}

	ret = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);
	if (ret < 0)
	{
		return -1;
	}

	return 0;
}

/*
 * Method in charge of updating the counters that indicate the time
 * our device has been active.
 */
void update_active_time()
{
	if (minutes_counter == 59)
	{
		hours_counter++;
		minutes_counter = 0;
		return;
	}

	minutes_counter++;
	return;
}

int flash_led(const struct gpio_dt_spec led, const int flash_per_sec, const int flash_cnt)
{
	/*
	 * If the led flashes at a frequency higher than 4Hz it would be difficult to realy count
	 * the total ammount of times a led flashes, so we set a cap at the frequency here,
	 * just in case. Also, the minimum frequency for the LED will be set as 1Hz.
	 */
	if (flash_per_sec > 4 || flash_per_sec < 1)
	{
		return -1;
	}

	int ret;
	int flash_period = 1000 / flash_per_sec;
	for (int i = 0; i < flash_cnt; i++)
	{
		ret = gpio_pin_set_dt(&led, 1);
		if (ret < 0)
		{
			return -1;
		}
		k_msleep(flash_period / 2);

		ret = gpio_pin_set_dt(&led, 0);
		if (ret < 0)
		{
			return -1;
		}
		k_msleep(flash_period / 2);
	}

	return 0;
}
/*
 * Method in charge of switching the LEDs to display the time
 * our device has been active.
 */
int display_active_time()
{
	int hours_tens = hours_counter / 10;
	int hours_units = hours_counter % 10;
	int minutes_tens = minutes_counter / 10;
	int minutes_units = minutes_counter % 10;

	int ret;

	ret = flash_led(led3, TIMMER_LEDS_FREQ, hours_tens);
	if (ret != 0)
	{
		return -1;
	}

	ret = flash_led(led2, TIMMER_LEDS_FREQ, hours_units);
	if (ret != 0)
	{
		return -1;
	}

	ret = flash_led(led1, TIMMER_LEDS_FREQ, minutes_tens);
	if (ret != 0)
	{
		return -1;
	}

	ret = flash_led(led0, TIMMER_LEDS_FREQ, minutes_units);
	if (ret != 0)
	{
		return -1;
	}

	return 0;
}

/*
 * Main idea is to do the led switching each minute inside the main loop, since
 * its the only thing the device will do. In case there where more tasks at the same
 * time the led switching would be done inside an ISR or multithreads would be used
 */
int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&led2) || !gpio_is_ready_dt(&led3))
	{
		return 0;
	}

	ret = configure_LEDS();
	if (ret != 0)
	{
		return 0;
	}

	while (1)
	{
		/*
		ret = gpio_pin_toggle_dt(&led0);
		ret = gpio_pin_set_dt(&led0, 1);
		if (ret < 0)
		{
			return 0;
		}
		*/
		ret = display_active_time();
		if (ret != 0)
		{
			return 0;
		}
		k_msleep(MINUTE_MS);
		update_active_time();
	}
	return 0;
}
