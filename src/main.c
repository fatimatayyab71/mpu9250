/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

static const char *now_str(void)
{
	static char buf[16]; /* ...HH:MM:SS.MMM */
	uint32_t now = k_uptime_get_32();
	unsigned int ms = now % MSEC_PER_SEC;
	unsigned int s;
	unsigned int min;
	unsigned int h;
	unsigned int uT;

	now /= MSEC_PER_SEC;
	s = now % 60U;
	now /= 60U;
	min = now % 60U;
	now /= 60U;
	h = now;

	snprintf(buf, sizeof(buf), "%u:%02u:%02u.%03u",
		 h, min, s, ms);
	return buf;
}

static int process_mpu9250(const struct device *dev)
{
	struct sensor_value temperature;
	struct sensor_value accel[3];
	struct sensor_value gyro[3];
	struct sensor_value magn[3];
	int rc = sensor_sample_fetch(dev);

	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ,
					accel);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ,
					gyro);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_MAGN_XYZ,
					magn);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP,
					&temperature);
	}
	if (rc == 0) {
		printf("[%s]:%g Cel\n"
		       "  accel %f %f %f m/s/s\n"
		       "  gyro  %f %f %f rad/s\n"
			   "  magn  %f %f %f uT",
		       now_str(),
		       sensor_value_to_double(&temperature),
		       sensor_value_to_double(&accel[0]),
		       sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]),
		       sensor_value_to_double(&gyro[0]),
			   sensor_value_to_double(&gyro[1]),
		       sensor_value_to_double(&gyro[2]),
			   sensor_value_to_double(&magn[0]),
		       sensor_value_to_double(&magn[1]),
		       sensor_value_to_double(&magn[2]));
	} else {
		printf("sample fetch/get failed: %d\n", rc);
	}

	return rc;
}

#ifdef CONFIG_MPU9250_TRIGGER
static struct sensor_trigger trigger;

static void handle_mpu9250_drdy(const struct device *dev,
				const struct sensor_trigger *trig)
{
	int rc = process_mpu9250(dev);

	if (rc != 0) {
		printf("cancelling trigger due to failure: %d\n", rc);
		(void)sensor_trigger_set(dev, trig, NULL);
		return;
	}
}
#endif /* CONFIG_MPU9250_TRIGGER */

int main(void)
{
	const struct device *const mpu9250 = DEVICE_DT_GET_ONE(invensense_mpu9250);

	if (!device_is_ready(mpu9250)) {
		printf("Device %s is not ready\n", mpu9250->name);
		return 0;
	}

#ifdef CONFIG_MPU9250_TRIGGER
	trigger = (struct sensor_trigger) {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};
	if (sensor_trigger_set(mpu9250, &trigger,
			       handle_mpu9250_drdy) < 0) {
		printf("Cannot configure trigger\n");
		return 0;
	}
	printk("Configured for triggered sampling.\n");
#endif

	while (!IS_ENABLED(CONFIG_MPU9250_TRIGGER)) {
		int rc = process_mpu9250(mpu9250);

		if (rc != 0) {
			break;
		}
		k_sleep(K_SECONDS(2));
	}

	/* triggered runs with its own thread after exit */
	return 0;
}
