#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_heap_caps.h"

#include "driver/i2c_master.h"

#define I2C_SCL 11
#define I2C_SDA 10
#define I2C_TASK_PRIO 10
#define I2C_TIMEOUT_MS 100
#define I2C_CLOCK_HZ 100000

#define I2C_DEVICE	  0x68 // DS3231 RTC Clock addr
#define I2C_DEVICE_NBYTES 12   // Number of bytes to read

TaskHandle_t i2c_task_handle;
i2c_master_bus_handle_t i2c_bus_handle;

volatile uint8_t i2c_data[I2C_DEVICE_NBYTES] = {0};

volatile i2c_master_event_t i2c_event, last_i2c_event;
volatile int i2c_completion_counter = 0;

bool esp32_i2c_dev_callback(i2c_master_dev_handle_t i2c_dev, const i2c_master_event_data_t *evt_data, void *arg)
{
	i2c_event = evt_data->event;

	vTaskResume(i2c_task_handle);

	return true;
}

void i2c_task(void *p)
{
	i2c_device_config_t dev_cfg;
	i2c_master_dev_handle_t dev_handle;
	
	// add device
	dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
	dev_cfg.device_address = I2C_DEVICE;
	dev_cfg.scl_speed_hz = I2C_CLOCK_HZ;

	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle) != ESP_OK);

	// register callback
	static i2c_master_event_callbacks_t cbs;
	cbs.on_trans_done = esp32_i2c_dev_callback;

	ESP_ERROR_CHECK(i2c_master_register_event_callbacks(dev_handle, &cbs, NULL) != ESP_OK);

	while (1)
	{
		uint8_t target = 0;
		int i, n_bytes = I2C_DEVICE_NBYTES;

		i2c_event = -1;

		esp_err_t e = i2c_master_transmit_receive(dev_handle, (void *) &target, 1,
			i2c_data, n_bytes, I2C_TIMEOUT_MS);

		ESP_ERROR_CHECK(e);

		while (i2c_event == -1)
		{
			vTaskSuspend(NULL);
		}
		last_i2c_event = i2c_event;
		i2c_completion_counter++;


		//sleep(1);

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
		.trans_queue_depth = 128,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};

	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

	xTaskCreate(i2c_task,
		"i2c_task",
		2048,
		NULL,
		I2C_TASK_PRIO,
		&i2c_task_handle);

}

void print_stats()
{
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

	printf("before free\r\n");
	free(stats);
	printf("after free\r\n");
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
		printf("meas/sec=%d last_i2c_event=%d\r\n",
			i2c_completion_counter, last_i2c_event);
		i2c_completion_counter = 0;

		vTaskDelayUntil( &lastwake, 1000 / portTICK_PERIOD_MS);
	}

}
