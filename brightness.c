#include <stdio.h>
#include <stdlib.h>
#include <gpiod.h>
#include <unistd.h>
#include <math.h>

#define CHARGE_PIN 24
#define READ_PIN 18

#define READ_TIMEOUT 15000000

#define BRIGHTNESS_FILE "/sys/class/backlight/10-0045/brightness"
#define BRIGHTNESS_MAX 255
#define BRIGHTNESS_MIN 16

#define	CONSUMER "brightness"


float map_to_brightness(float x)
{
	// 0 - 10
	// f(x) = -96/5·x + 255
	// 10-50
	// f(x) = -4/5·x + 71
	// 50-150
	// f(x) = -3/20·x + 77/2

	if ( x <= 10) {
		return -96.0/5.0 * x + 255.0;
	}
	else if (x > 10 && x <= 50) {
		return -4.0/5.0 * x + 71.0;
	}
	else {
		return -3.0/20.0 * x + 77.0/2.0;
	}
}


void set_brightness(int brightness_value) {
    if (brightness_value > BRIGHTNESS_MAX) {
        brightness_value = BRIGHTNESS_MAX;
    }
    if (brightness_value < BRIGHTNESS_MIN) {
        brightness_value = BRIGHTNESS_MIN;
    }    
	
	FILE *brightness_file = fopen(BRIGHTNESS_FILE, "w");
    
    fprintf(brightness_file, "%d", brightness_value);
    
    fclose(brightness_file);;
}


int main()
{
	struct timespec ts = { 0, READ_TIMEOUT }; // 15 milliseconds
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

	int brightness, brightness_cur;
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

		// Charge 
		ret = gpiod_line_request_rising_edge_events(gpio_read_line, CONSUMER);
		if (ret != 0)
			goto release_lines;

		ret = gpiod_line_request_output(gpio_charge_line, CONSUMER, 1);
		if (ret != 0)
			goto release_lines;
		
		clock_gettime(CLOCK_MONOTONIC, &start);
		ret = gpiod_line_event_wait(gpio_read_line, &ts); // Wait until the signal changes from 0 to 1
		clock_gettime(CLOCK_MONOTONIC, &end);

		if (ret > 0 && end.tv_nsec > start.tv_nsec) {
			y = end.tv_nsec - start.tv_nsec;
		} else {
			y = READ_TIMEOUT;
		}

		gpiod_line_release(gpio_charge_line);
		gpiod_line_release(gpio_read_line);

		// Smooth values 
		y_stern = 0.1 * y + (1 - 0.1) * y_stern__prev; // https://de.wikipedia.org/wiki/Exponentielle_Gl%C3%A4ttung
		y_stern__prev = y_stern;

		//printf("y: %f   %f\n", y, y_stern);

		// Map timing values to brightness values
	 	brightness = (int)map_to_brightness( y_stern / 100000.0 ); // Map to 0 - 150
		/*
		// Set brightness of display
		set_brightness(brightness);
		printf("Brightness:     %d\n", brightness);

		sleep(1);
		*/
		 
		if (brightness > brightness_cur) {
			brightness_cur++;
		}
		else if (brightness < brightness_cur) {
			brightness_cur--;
		}
		set_brightness(brightness_cur);
		//printf("Brightness:     %d\n", brightness_cur);

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
