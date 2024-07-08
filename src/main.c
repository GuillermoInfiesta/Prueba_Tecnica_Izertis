#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

/* Frequency at which the LEDs will blink*/
#define TIMMER_LEDS_FREQ 2
#define THREADS_PRIO 5
#define STACK_SIZE 1024

/* The devicetree node identifiers for the "led0", "led1", "led2" & "led3" aliases. */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

/* Devicetree node identifier for the "sw0" alias */
#define SW0_NODE DT_ALIAS(sw0)

/* Define the sematphores for our synchronizing of the threads */
K_SEM_DEFINE(sem, 0, 1);

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb;
void reset_count(const struct device *dev, struct gpio_callback *cb,
				 uint32_t pins);

static int hours_counter, minutes_counter = 0;

/*
 * Method in charge of configuring the LEDs we will use as outputs,
 * they are set as inactive so they start with a logic '0'.
 * RETURN:
 * 0 -> No errors
 * -1 -> An error ocurred when configuring one of the LEDs
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
 * Method in charge of configuring the button pin as an input,
 * configure when the interruption should be triggered and link
 * the callback function for when the button is pressed.
 * RETURN:
 * 0 -> No errors
 * -1 -> An error ocurred configuring the button
 */
int configure_button()
{
	int ret;

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0)
	{
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0)
	{
		return -1;
	}

	gpio_init_callback(&button_cb, reset_count, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb);

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
}

/*
 * Method linked to the callback of the button when its pressed, since
 * we want to restart the count once the button is pressed, all we do
 * is reset the counters.
 */
void reset_count(const struct device *dev, struct gpio_callback *cb,
				 uint32_t pins)
{
	minutes_counter = 0;
	hours_counter = 0;
}

/*
 * Method in charge of making a specific LED blink a specific number of timmes
 * PARAMS:
 * led -> The LED which we want to blink
 * flash_per_sec -> The frequency at which the LED will blink
 * flash_cnt -> How many times we want the LED to blink
 */
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
	int flash_period_ms = 1000 / flash_per_sec;
	for (int i = 0; i < flash_cnt; i++)
	{
		ret = gpio_pin_set_dt(&led, 1);
		if (ret < 0)
		{
			return -1;
		}
		k_msleep(flash_period_ms / 2);

		ret = gpio_pin_set_dt(&led, 0);
		if (ret < 0)
		{
			return -1;
		}
		k_msleep(flash_period_ms / 2);
	}

	return 0;
}

/*
 * Method in charge of switching the LEDs to display the time
 * our device has been active.
 */
void display_active_time()
{
	/*
	 * Split the counters in tens and units.
	 * e.g: 17 / 10 = 1  &  17 % 10 = 7.
	 */
	int hours_tens = hours_counter / 10;
	int hours_units = hours_counter % 10;
	int minutes_tens = minutes_counter / 10;
	int minutes_units = minutes_counter % 10;

	int ret;

	ret = flash_led(led3, TIMMER_LEDS_FREQ, hours_tens);
	if (ret != 0)
	{
		return;
	}

	ret = flash_led(led2, TIMMER_LEDS_FREQ, hours_units);
	if (ret != 0)
	{
		return;
	}

	ret = flash_led(led1, TIMMER_LEDS_FREQ, minutes_tens);
	if (ret != 0)
	{
		return;
	}

	ret = flash_led(led0, TIMMER_LEDS_FREQ, minutes_units);
	if (ret != 0)
	{
		return;
	}
};

/*
 * Method the timing thread will execute. It first configures the
 * gpio´s and then goes in an infinite loop of waiting a minute and
 * giving the semaphore to the other thread.
 */
void timing_thread()
{

	int ret;

	if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&led2) || !gpio_is_ready_dt(&led3) || !gpio_is_ready_dt(&button))
	{
		return;
	}

	ret = configure_LEDS();
	if (ret != 0)
	{
		return;
	}

	ret = configure_button();
	if (ret != 0)
	{
		return;
	}

	while (true)
	{
		k_sem_give(&sem);
		k_msleep(1000 * 60);
	}
}

/*
 * Method the leds thread will execute, it constantly
 * waits for the semaphore to be available (1 minute past)
 * and once it is, it updates the active time and displays it,
 * going back again to waiting after
 */
void leds_thread()
{
	while (true)
	{
		k_sem_take(&sem, K_FOREVER);

		update_active_time();
		display_active_time();
	}
}

K_THREAD_DEFINE(task1_id, STACK_SIZE, timing_thread, NULL, NULL, NULL, THREADS_PRIO, 0, 0);
K_THREAD_DEFINE(task2_id, STACK_SIZE, leds_thread, NULL, NULL, NULL, THREADS_PRIO, 0, 0);