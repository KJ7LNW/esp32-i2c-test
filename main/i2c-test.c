/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2024- by Eric Wheeler, KJ7LNW.  All rights reserved.
 *
 * The official website and doumentation is available here:
 *   https://github.com/KJ7LNW/esp32-i2c-test
 */


// This example implements both sync and async i2c depending on the settings
// below.  It was written to facilitate testing of the Espressif's ESP32
// i2c_master code.

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_heap_caps.h"

#include "driver/i2c_master.h"

// Comment or un-comment the `#define I2C_USE_CALLBACK` below as you wish:
//   Uncommented:
//       - Use async i2c with callback and FreeRTOS task-wakeup.
//       - Achives ~7500 single-byte samples per second at 400kHz with FreeRTOS
//         systicks at 1000Hz.
//       - This uses very little CPU overhead.
//
//   Commented:
//       - Use sync i2c
//       - Achives ~995 single-byte samples per second at 400kHz with FreeRTOS
//         systicks at 1000Hz. This method is slower because of context switch
//         overhead.  If you get ~99 samples/sec, then you are probably running
//         FreeRTOS systicks at 100Hz.
//       - This uses lots of CPU overhead due to context switch overhead. It
//         could be mitigated by increasing vTaskDelay(1) to something higher,
//         but the sample time would suffer.

//#define I2C_USE_CALLBACK

// Set your SDA, SCL pins, and clock speed:
#define I2C_SCL 11
#define I2C_SDA 10
#define I2C_CLOCK_HZ 100000 // 100kHz

// Set a device
#define I2C_DEVICE	  0x68 // DS3231 RTC Clock addr
#define I2C_DEVICE_REG	  0x00 // Device register from which to start reading I2C_DEVICE_NBYTES
#define I2C_DEVICE_NBYTES 1    // Number of bytes to read via i2c_master_transmit_receive()

// You probably don't need to change these:
#define I2C_TASK_PRIO 10
#define I2C_TIMEOUT_MS 100

TaskHandle_t i2c_task_handle;
i2c_master_bus_handle_t i2c_bus_handle;

// Read data buffer
volatile uint8_t i2c_data[I2C_DEVICE_NBYTES] = {0};

// Counter for meas/sec stats printed each second
volatile int i2c_completion_counter = 0;

volatile i2c_master_event_t last_i2c_event = 0;
volatile i2c_master_event_t i2c_event = 0;

volatile esp_err_t          i2c_err = 0;

// Just value to string mappings:
char *i2c_events[] = {
	[I2C_EVENT_ALIVE] = "I2C_EVENT_ALIVE",
	[I2C_EVENT_DONE] = "I2C_EVENT_DONE",
	[I2C_EVENT_NACK] = "I2C_EVENT_NACK",
};

#ifdef I2C_USE_CALLBACK
bool esp32_i2c_dev_callback(i2c_master_dev_handle_t i2c_dev, const i2c_master_event_data_t *evt_data, void *arg)
{
	i2c_event = evt_data->event;

	if (i2c_event != I2C_EVENT_ALIVE)
	{
		vTaskResume(i2c_task_handle);

		// Return true because we woke up a high priority task.
		return true;
	}
	else
		return false;


}
#endif

void i2c_task(void *p)
{
	i2c_device_config_t dev_cfg;
	i2c_master_dev_handle_t dev_handle;
	
	// add device
	dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
	dev_cfg.device_address = I2C_DEVICE;
	dev_cfg.scl_speed_hz = I2C_CLOCK_HZ;

	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle) != ESP_OK);

#ifdef I2C_USE_CALLBACK
	// register callback
	static i2c_master_event_callbacks_t cbs;
	cbs.on_trans_done = esp32_i2c_dev_callback;

	ESP_ERROR_CHECK(i2c_master_register_event_callbacks(dev_handle, &cbs, NULL) != ESP_OK);
#endif

	while (1)
	{
		uint8_t target = I2C_DEVICE_REG;
		int n_bytes = I2C_DEVICE_NBYTES;

		i2c_event = -1;

		i2c_err = i2c_master_transmit_receive(dev_handle, (void *) &target, 1,
			(uint8_t*)i2c_data, n_bytes, I2C_TIMEOUT_MS);

		if (i2c_err != ESP_ERR_INVALID_STATE)
			ESP_ERROR_CHECK(i2c_err);

#ifdef I2C_USE_CALLBACK
		// if running async, suspend the task to be woken by the callback:
		while (i2c_event == -1)
		{
			vTaskSuspend(NULL);
		}
		last_i2c_event = i2c_event;
#else
		// if running sync, just delay a tick:
		vTaskDelay(1);
#endif
		i2c_completion_counter++;

	}
}

void initI2C()
{
	// init bus
	i2c_master_bus_config_t i2c_mst_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = -1,
		.scl_io_num = I2C_SCL,
		.sda_io_num = I2C_SDA,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
#ifdef I2C_USE_CALLBACK
		.trans_queue_depth = 128,
#endif
	};

	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

	xTaskCreate(i2c_task,
		"i2c_task",
		2048,  // probably more than enough
		NULL,
		I2C_TASK_PRIO,
		&i2c_task_handle);

}

void print_stats()
{

	printf("\r\n=== MALLOC_CAP_8BIT\r\n");
	heap_caps_print_heap_info(MALLOC_CAP_8BIT);

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
	char *stats = malloc(1024);
	if (stats == NULL)
	{
		printf("failed to allocate memory\r\n");
		return;
	}

	printf("\r\n=== TASK STATS\r\n");
	printf("name\t\trun ctr\tavg %%cpu\r\n");
	vTaskGetRunTimeStats(stats);
	printf(stats);
	printf("text length=%d\r\n", strlen(stats));

	printf("\r\n=== TASK LIST\r\n");
	printf("Name\t\tstate\tprio\tcore?\tfree stack\ttask number\r\n");
	vTaskList(stats);
	printf(stats);
	printf("text length=%d\r\n", strlen(stats));
	printf("Tasks are reported as blocked (B), ready (R), deleted (D) or suspended (S).\r\n");

	free(stats);
#endif
}

void app_main(void)
{
	initI2C();

	TickType_t lastwake = xTaskGetTickCount();
	while (1)
	{
		print_stats();
		for (int i = 0; i < I2C_DEVICE_NBYTES; i++)
			printf("%d. %02X\r\n", i, i2c_data[i]);
		printf("meas/sec=%d, last_i2c_event=%d (%s), i2c_err=%d\r\n",
			i2c_completion_counter, last_i2c_event, i2c_events[last_i2c_event], i2c_err);
		i2c_completion_counter = 0;

		vTaskDelayUntil( &lastwake, 1000 / portTICK_PERIOD_MS);
	}

}
