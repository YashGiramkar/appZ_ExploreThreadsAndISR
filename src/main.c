#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>


#ifdef CONFIG_MYISRENABLE
   #include "myISR.h"
#endif

// Producer thread sleep time in milliseconds
#define PRODUCER_SLEEP_TIME_MS               2200
// Stack Size for producer and consumer threads
#define STACKSIZE                            2048
// Producer thread priority
#define PRODUCER_THREAD_PRIORITY             6
//  Consumer thread priority
#define CONSUMER_THREAD_PRIORITY             7


// Get the node addresses from DTS via the aliases
#define LED0_NODE                            DT_ALIAS(led0)
#define LED1_NODE                            DT_ALIAS(led1)
#define LED2_NODE                            DT_ALIAS(led2)
#define USER_BUTTON_NODE                     DT_ALIAS(sw0)

// Determine the GPIO device tree specifications for the specific pin
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(USER_BUTTON_NODE, gpios);

typedef struct {
    uint32_t x_reading;
    uint32_t y_reading;
    uint32_t z_reading;
} SensorReading;


static void myTimer_Handler(struct k_timer *dummy)
{
    /*Interrupt Context - System Timer ISR */
    static bool flip= true;
    if (flip)
        gpio_pin_toggle_dt(&led0);
    else
        gpio_pin_toggle_dt(&led1);

    flip = !flip;

   //  The following would be a really bad idea, the controller doesnt even reset , it just freezes and we have to reset it manually.
   //  Sleep for 2000ms to simulate a long running task in the ISR. This will cause the system timer to miss some ticks and we can see the effect of that in the logs.
   // k_msleep(2000);
}


LOG_MODULE_REGISTER(ThreadAndISRLogger, LOG_LEVEL_DBG);
K_TIMER_DEFINE(myTimer, myTimer_Handler, NULL);
K_MSGQ_DEFINE(SensorDataQueue, sizeof(SensorReading), 16, 4);


int main(void)
{
   int ret =0;

   // TODO: Checks for configuration settings are currently removed, we should set them later
   ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
   ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
   ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_LOW);
   ret = gpio_pin_configure_dt(&button, GPIO_INPUT);

   // Start periodic timer that expires once every 0.5 second
   k_timer_start(&myTimer, K_MSEC(500), K_MSEC(500));

   return 0;
}


static void producer_func(void *unused1, void *unused2, void *unused3)
{
   ARG_UNUSED(unused1);
   ARG_UNUSED(unused2);
   ARG_UNUSED(unused3);

   while (1)
   {
      static SensorReading acc_val = {100, 100, 100};
      int ret;

      ret = k_msgq_put(&SensorDataQueue,&acc_val,K_FOREVER);
      if (ret)
      {
         LOG_ERR("Return value from k_msgq_put = %d",ret);
      }
      else
      {
         LOG_INF("Values put in the queue: %d.%d.%d\r\n", acc_val.x_reading, acc_val.y_reading,
         acc_val.z_reading);
      }

      acc_val.x_reading += 1;
      acc_val.y_reading += 1;
      acc_val.z_reading += 1;
      k_msleep(PRODUCER_SLEEP_TIME_MS);
   }
}

static void consumer_func(void *unused1, void *unused2, void *unused3)
{
   ARG_UNUSED(unused1);
   ARG_UNUSED(unused2);
   ARG_UNUSED(unused3);

   while (1)
   {
      SensorReading temp;
      int ret;
      ret = k_msgq_get(&SensorDataQueue,&temp,K_FOREVER);
      if (ret){
         LOG_ERR("Return value from k_msgq_get = %d",ret);
      }
      else
      {

      }

      LOG_INF("Values got from the queue: %d.%d.%d\r\n", temp.x_reading, temp.y_reading,
         temp.z_reading);
   }
}

K_THREAD_DEFINE(producer, STACKSIZE, producer_func, NULL, NULL, NULL,
                                                PRODUCER_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(consumer, STACKSIZE, consumer_func, NULL, NULL, NULL,
                                                CONSUMER_THREAD_PRIORITY, 0, 0);