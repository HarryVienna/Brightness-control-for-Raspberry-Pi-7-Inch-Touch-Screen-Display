#include <stdio.h>
#include <stdlib.h>
#include <gpiod.h>
#include <unistd.h>
#include <math.h>

#define CHARGE_PIN 24
#define READ_PIN 18

#define MAX_STACK_SIZE 10

typedef struct Stack {
    int values[MAX_STACK_SIZE];
    int size;
} Stack;

void push(Stack *stack, int value) {
    if (stack->size < MAX_STACK_SIZE) {
        stack->values[stack->size] = value;
        stack->size++;
    } else {
        // Shift all values down one position
        for (int i = 1; i < MAX_STACK_SIZE; i++) {
            stack->values[i-1] = stack->values[i];
        }
        // Add new value to end of stack
        stack->values[MAX_STACK_SIZE-1] = value;
    }
}

int getAverage(Stack *stack) {
    if (stack->size == 0) {
        return 0;
    }

    int sum = 0;
    for (int i = 0; i < stack->size; i++) {
        sum += stack->values[i];
    }
    return sum / stack->size;
}

int map(int value) {
  // Make sure value is within the valid range
  if (value < 0) {
    value = 0;
  } else if (value > 4096) {
    value = 4096;
  }

  // Calculate the mapping value
  return 255 - (value / 4096.0) * 239;
}

float map2(int value) {
  // Make sure value is within the valid range
  if (value < 0) {
    value = 0;
  } else if (value > 4096) {
    value = 4096;
  }

  // Calculate the mapping value using a logarithmic function
  double log_value = log10(value + 1) / log10(4096 + 1);
  return log_value * 240 + 16;
}

int main()
{
	Stack stack;
	stack.size = 0;

        struct gpiod_chip *gpiochip;
        struct gpiod_line *gpio_charge_line;
	struct gpiod_line *gpio_read_line;
        int ret;

        gpiochip = gpiod_chip_open("/dev/gpiochip0");
        if (gpiochip == NULL)
                goto error_open;


        gpio_charge_line = gpiod_chip_get_line(gpiochip, CHARGE_PIN);
	gpio_read_line = gpiod_chip_get_line(gpiochip, READ_PIN);
        if (gpio_charge_line == NULL)
                goto error_gpio;
        if (gpio_read_line == NULL)
                goto error_gpio;

        ret = gpiod_line_request_output(gpio_charge_line, "gpio", 0);
        if (ret != 0)
                goto error_gpio;


while(1) {
	gpiod_line_set_value(gpio_charge_line, 0);
        ret = gpiod_line_request_output(gpio_read_line, "gpio", 0);
        if (ret != 0)
                goto error_gpio;

	//sleep(1);
	usleep(10000);

	gpiod_line_release(gpio_read_line);

	ret = gpiod_line_request_input(gpio_read_line, "gpio");
        if (ret != 0)
                goto error_gpio;

	int counter = 0;
	gpiod_line_set_value(gpio_charge_line, 1);

	int val;
	while(counter < 32000) {
		val = gpiod_line_get_value(gpio_read_line);
		if (val == 1) {
     			 break;
    		}
		counter++;
	}
	gpiod_line_set_value(gpio_charge_line, 0);

	printf("counter: %d\n", counter);

	gpiod_line_release(gpio_read_line);

	push(&stack, counter);

	printf("Average:     %d\n", getAverage(&stack));

	printf("Brightness:     %f\n", map2(getAverage(&stack)));

	//sleep(1);
	usleep(20000);

}
	gpiod_chip_close(gpiochip);



error_gpio:
        gpiod_chip_close(gpiochip);
error_open:
        return -1;
}
