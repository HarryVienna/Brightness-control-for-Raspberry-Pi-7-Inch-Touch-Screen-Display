#include <stdio.h>
#include <stdlib.h>
#include <gpiod.h>
#include <unistd.h>
#include <math.h>

#define CHARGE_PIN 24
#define READ_PIN 18

#define STACK_SIZE 10

#define BRIGHTNESS_FILE "/sys/class/backlight/rpi_backlight/brightness"
#define BRIGHTNESS_MAX 255
#define BRIGHTNESS_MIN 16
#define	CONSUMER	"backlight"

typedef struct
{
	float data[STACK_SIZE];
	int top;
} stack_t;

void push(stack_t *stack, float value)
{
	if (stack->top < STACK_SIZE) {
        stack->data[stack->top++] = value;
    } else {
        // stack is full, so "pop" the oldest value by shifting all
        // elements to the left by one position
        for (int i = 0; i < STACK_SIZE - 1; ++i) {
            stack->data[i] = stack->data[i + 1];
        }
        stack->data[STACK_SIZE - 1] = value;
    }
}

float get_average(const stack_t *stack) {
    float sum = 0;
    for (int i = 0; i < stack->top; ++i) {
        sum += stack->data[i];
    }
    return sum / stack->top;
}

float map(float x)
{
	// 0 -15
	// f(x) = -64/5·x + 255
	// 15-60
	// f(x) = -32/45·x + 221/3
	// 60-240
	// f(x) = -31/180·x + 124/3

	if ( x <= 15) {
		return -64.0/5.0 * x + 255.0;
	}
	else if (x > 15 && x <= 60) {
		return -32.0/45.0 * x + 221.0/3.0;
	}
	else {
		return -31.0/180.0 * x + 124.0/3.0;
	}
}


void set_brightness(int brightness_value) {
    FILE *brightness_file;

    if (brightness_value > BRIGHTNESS_MAX) {
        brightness_value = BRIGHTNESS_MAX;
    }
    if (brightness_value < BRIGHTNESS_MIN) {
        brightness_value = BRIGHTNESS_MIN;
    }    
    
    brightness_file = fopen(BRIGHTNESS_FILE, "w");
    
    fprintf(brightness_file, "%d", brightness_value);
    
    fclose(brightness_file);
}

int main()
{
	
	stack_t stack;
	stack.top = 0;

	struct timespec ts = { 0, 10000000 }; // 10 milliseconds
	struct timespec start, end;

	struct gpiod_chip * gpiochip;
	struct gpiod_line * gpio_charge_line;
	struct gpiod_line * gpio_read_line;

	int ret;

	// Init GPIO
	gpiochip = gpiod_chip_open("/dev/gpiochip0");
	if (gpiochip == NULL)
		goto error_open;

	// Get GPIO pins
	gpio_charge_line = gpiod_chip_get_line(gpiochip, CHARGE_PIN);
	gpio_read_line = gpiod_chip_get_line(gpiochip, READ_PIN);
	if (gpio_charge_line == NULL)
		goto close_chip;
	if (gpio_read_line == NULL)
		goto close_chip;

	float y, y_stern, y_stern__prev;
	y_stern__prev = 0;

	int brightness, brightness_prev, brightness_cur;
	brightness_cur = 16;
	while (1)
	{
		// Discharge 
		ret = gpiod_line_request_output(gpio_charge_line, CONSUMER, 0);
		if (ret != 0)
			goto release_lines;
		ret = gpiod_line_request_output(gpio_read_line, CONSUMER, 0);
		if (ret != 0)
			goto release_lines;

		usleep(10000);

		gpiod_line_release(gpio_charge_line);
		gpiod_line_release(gpio_read_line);

		ret = gpiod_line_request_rising_edge_events(gpio_read_line, CONSUMER);
		if (ret != 0)
			goto release_lines;

		ret = gpiod_line_request_output(gpio_charge_line, CONSUMER, 1);
		if (ret != 0)
			goto release_lines;
		
		// Warten auf Signal von 0 auf 1
		clock_gettime(CLOCK_MONOTONIC, &start);
		ret = gpiod_line_event_wait(gpio_read_line, &ts);
		clock_gettime(CLOCK_MONOTONIC, &end);

		if (ret > 0 && end.tv_nsec > start.tv_nsec) {
			y = end.tv_nsec - start.tv_nsec;
		} else {
			y = 10000000;
		}
		y = y / 50000; // Map to 0 - 240

		gpiod_line_release(gpio_charge_line);
		gpiod_line_release(gpio_read_line);

		// https://de.wikipedia.org/wiki/Exponentielle_Gl%C3%A4ttung
		y_stern = 0.1 * y + (1 - 0.1) * y_stern__prev;
		y_stern__prev = y_stern;


		//printf("y: %f   %f\n", y, y_stern);

		
	 	brightness = (int)map(y_stern);
		if (brightness > brightness_cur) {
			brightness_cur++;
		}
		else if (brightness < brightness_cur) {
			brightness_cur--;
		}
		set_brightness(brightness_cur);
		printf("Brightness:     %d\n", brightness_cur);
/*
		int duration = 100000; // 100 milliseconds
		int i;
		if(brightness_prev > brightness){
			for (i = brightness_prev; i >= brightness; i--) {
					set_brightness(i);
					usleep(duration / abs(brightness - brightness_prev));
				}
		} 
		else if(brightness_prev < brightness){
			for (i = brightness_prev; i <= brightness; i++) {
					set_brightness(i);
					usleep(duration / abs(brightness - brightness_prev));
				}
		}
		brightness_prev = brightness;
*/

		//sleep(1);
		usleep(100000);
	}

	release_lines:
		gpiod_line_release(gpio_charge_line);
		gpiod_line_release(gpio_read_line);
	close_chip:
		gpiod_chip_close(gpiochip);
	error_open:
		return ret;
}
