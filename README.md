# Overview

This example implements both sync and async i2c depending on the settings
below.  It was written to facilitate testing of the Espressif's ESP32
i2c\_master code.

- Comment or un-comment the `#define I2C_USE_CALLBACK` at the top of `i2c-test.c` as you wish:
    - Uncommented:
      - Use async i2c with callback and FreeRTOS task-wakeup.
      - Achives ~7500 single-byte samples per second at 400kHz with FreeRTOS
        systicks at 1000Hz.
      - This uses only ~25% CPU overhead, which is quite efficient at that sample rate.

    - Commented:
      - Use sync i2c
      - Achives ~995 single-byte samples per second at 400kHz with FreeRTOS
        systicks at 1000Hz. This method is slower because of context switch
        overhead.  If you get ~99 samples/sec, then you are probably running
        FreeRTOS systicks at 100Hz.
      - This uses lots of CPU overhead due to context switch overhead. It
        could be mitigated by increasing vTaskDelay(1) to something higher,
        but the sample time would suffer.

When running it prints output like the following, with meas/sec stats at the 
bottom:

## Asynchronous I2C Output (~7500 samples/sec)

```c
=== MALLOC_CAP_8BIT
Heap summary for capabilities 0x00000004:
  At 0x4080e9e0 len 449584 free 388036 allocated 58820 min_free 387816
    largest_free_block 385012 alloc_blocks 60 free_blocks 1 total_blocks 61
  At 0x4087c610 len 12116 free 10348 allocated 0 min_free 10348
    largest_free_block 10228 alloc_blocks 0 free_blocks 1 total_blocks 1
  At 0x50000000 len 16360 free 14592 allocated 0 min_free 14592
    largest_free_block 14580 alloc_blocks 0 free_blocks 1 total_blocks 1
  Totals:
    free 412964 allocated 58580 min_free 412744 largest_free_block 385000

=== TASK STATS
name            run ctr avg %cpu
main            6716371         6
IDLE            70006198                69
Tmr Svc         10              <1
esp_timer       46960           <1
text length=113

=== TASK LIST
Name            state   prio    core?   free stack      task number
main            X       1       0       18560   2
IDLE            R       0       0       1320    3
esp_timer       S       22      0       3864    1
Tmr Svc         B       1       0       1780    4
text length=122
Tasks are reported as blocked (B), ready (R), deleted (D) or suspended (S).
0. 34
meas/sec=7506, last_i2c_event=1 (I2C_EVENT_DONE), i2c_err=0
```

## Synchronous I2C Output (~995 samples/sec)

```c
=== MALLOC_CAP_8BIT
Heap summary for capabilities 0x00000004:
  At 0x4080e780 len 450192 free 411568 allocated 35992 min_free 410304
    largest_free_block 409588 alloc_blocks 54 free_blocks 1 total_blocks 55
  At 0x4087c610 len 12116 free 10348 allocated 0 min_free 10348
    largest_free_block 10228 alloc_blocks 0 free_blocks 1 total_blocks 1
  At 0x50000000 len 16360 free 14592 allocated 0 min_free 14592
    largest_free_block 14580 alloc_blocks 0 free_blocks 1 total_blocks 1
  Totals:
    free 436496 allocated 35776 min_free 435232 largest_free_block 409576

=== TASK STATS
name            run ctr avg %cpu
main            33570677                5
IDLE            325726113               50
i2c_task        291773639               44
Tmr Svc         11              <1
esp_timer       46952           <1
text length=147

=== TASK LIST
Name            state   prio    core?   free stack      task number
main            X       1       0       18560   2
IDLE            R       0       0       1320    3
i2c_task        B       10      0       1496    5
esp_timer       S       22      0       3864    1
Tmr Svc         B       1       0       1780    4
text length=153
Tasks are reported as blocked (B), ready (R), deleted (D) or suspended (S).
0. 53
meas/sec=992, last_i2c_event=0 (I2C_EVENT_ALIVE), i2c_err=0
```
