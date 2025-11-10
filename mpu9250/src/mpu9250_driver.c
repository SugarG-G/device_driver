// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define MPU9250_WHO_AM_I          0x75
#define MPU9250_WHO_AM_I_VAL      0x71
#define MPU9250_PWR_MGMT_1        0x6B
#define MPU9250_SMPLRT_DIV        0x19
#define MPU9250_CONFIG            0x1A
#define MPU9250_GYRO_CONFIG       0x1B
#define MPU9250_ACCEL_CONFIG      0x1C

#define MPU9250_ACCEL_XOUT_H      0x3B
#define MPU9250_GYRO_XOUT_H       0x43

struct my9250_state {
	struct i2c_client *client;
	struct regmap *regmap;
	struct iio_dev *indio;
	/* scale 설정 간단화: ±2g, ±250dps 고정 */
	int accel_scale_ug;  /* micro-g per LSB */
	int gyro_scale_udps; /* micro-deg/s per LSB */
};

static const struct regmap_config my9250_regmap_cfg = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x7F,
};

/* raw 16비트 읽기 헬퍼 */
static int my9250_read16(struct my9250_state *st, unsigned int reg, s16 *out)
{
	int hi, lo, ret;

	ret = regmap_read(st->regmap, reg, &hi);
	if (ret) return ret;
	ret = regmap_read(st->regmap, reg + 1, &lo);
	if (ret) return ret;

	*out = (s16)((hi << 8) | (lo & 0xFF));
	return 0;
}

/* IIO 채널: accel x/y/z, gyro x/y/z */
enum {
	CH_ACCEL_X, CH_ACCEL_Y, CH_ACCEL_Z,
	CH_GYRO_X, CH_GYRO_Y, CH_GYRO_Z,
	CH_MAX
};

static const struct iio_chan_spec my9250_channels[] = {
	{ .type = IIO_ACCEL, .modified = 1, .channel2 = IIO_MOD_X,
	  .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) },
	{ .type = IIO_ACCEL, .modified = 1, .channel2 = IIO_MOD_Y,
	  .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) },
	{ .type = IIO_ACCEL, .modified = 1, .channel2 = IIO_MOD_Z,
	  .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) },
	{ .type = IIO_ANGL_VEL, .modified = 1, .channel2 = IIO_MOD_X,
	  .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) },
	{ .type = IIO_ANGL_VEL, .modified = 1, .channel2 = IIO_MOD_Y,
	  .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) },
	{ .type = IIO_ANGL_VEL, .modified = 1, .channel2 = IIO_MOD_Z,
	  .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) },
};

static int my9250_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct my9250_state *st = iio_priv(indio_dev);
	s16 raw;
	int ret, base;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
		case IIO_ACCEL:
			switch (chan->channel2) {
			case IIO_MOD_X: base = MPU9250_ACCEL_XOUT_H; break;
			case IIO_MOD_Y: base = MPU9250_ACCEL_XOUT_H + 2; break;
			case IIO_MOD_Z: base = MPU9250_ACCEL_XOUT_H + 4; break;
			default: return -EINVAL;
			}
			ret = my9250_read16(st, base, &raw);
			if (ret) return ret;
			*val = raw;
			return IIO_VAL_INT;
		case IIO_ANGL_VEL:
			switch (chan->channel2) {
			case IIO_MOD_X: base = MPU9250_GYRO_XOUT_H; break;
			case IIO_MOD_Y: base = MPU9250_GYRO_XOUT_H + 2; break;
			case IIO_MOD_Z: base = MPU9250_GYRO_XOUT_H + 4; break;
			default: return -EINVAL;
			}
			ret = my9250_read16(st, base, &raw);
			if (ret) return ret;
			*val = raw;
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_SCALE:
		/* 단순화: accel=±2g(16384 LSB/g), gyro=±250dps(131 LSB/dps) */
		if (chan->type == IIO_ACCEL) {
			/* scale를 m/s^2 단위로 제공: 1 LSB = 9.80665/16384 */
			*val = 0;                  /* 정수부 */
			*val2 = (int)(9806650LL / 16384); /* micro(m/s^2) */
			return IIO_VAL_INT_PLUS_MICRO;
		} else if (chan->type == IIO_ANGL_VEL) {
			/* rad/s 단위. 1 dps = PI/180 rad/s, 1 LSB = 1/131 dps */
			/* => 1 LSB = (PI/180)/131 rad/s */
			*val = 0;
			*val2 = 1000000L; /* 미세 조정 없이 userspace에서 변환 추천 */
			return IIO_VAL_INT_PLUS_MICRO;
		}
		return -EINVAL;
	default:
		return -EINVAL;
	}
}

static const struct iio_info my9250_iio_info = {
	.read_raw = my9250_read_raw,
};

static int my9250_chip_init(struct my9250_state *st)
{
	int val, ret;

	/* WHO_AM_I 확인 */
	ret = regmap_read(st->regmap, MPU9250_WHO_AM_I, &val);
	if (ret) return ret;
	if (val != MPU9250_WHO_AM_I_VAL)
		return -ENODEV;

	/* 슬립 해제, 내부 클럭 사용 */
	ret = regmap_write(st->regmap, MPU9250_PWR_MGMT_1, 0x00);
	if (ret) return ret;

	/* 최소 초기화: LPF 기본값, 샘플분주, 풀스케일 기본 */
	regmap_write(st->regmap, MPU9250_SMPLRT_DIV, 0x07);   /* 예: 1kHz/8 */
	regmap_write(st->regmap, MPU9250_CONFIG, 0x03);       /* DLPF */
	regmap_write(st->regmap, MPU9250_GYRO_CONFIG, 0x00);  /* ±250dps */
	regmap_write(st->regmap, MPU9250_ACCEL_CONFIG, 0x00); /* ±2g */

	return 0;
}

static int my9250_probe(struct i2c_client *client)
{
	struct iio_dev *indio;
	struct my9250_state *st;
	int ret;

	indio = devm_iio_device_alloc(&client->dev, sizeof(*st));
	if (!indio) return -ENOMEM;

	st = iio_priv(indio);
	st->client = client;
	st->regmap = devm_regmap_init_i2c(client, &my9250_regmap_cfg);
	if (IS_ERR(st->regmap)) return PTR_ERR(st->regmap);

	ret = my9250_chip_init(st);
	if (ret) return ret;

	indio->name = "my-mpu9250";
	indio->modes = INDIO_DIRECT_MODE;
	indio->info  = &my9250_iio_info;
	indio->channels = my9250_channels;
	indio->num_channels = ARRAY_SIZE(my9250_channels);

	ret = devm_iio_device_register(&client->dev, indio);
	if (ret) return ret;

	dev_info(&client->dev, "my-mpu9250 ready\n");
	return 0;
}

static void my9250_remove(struct i2c_client *client) { }

static const struct of_device_id my9250_of_match[] = {
	{ .compatible = "myvendor,my-mpu9250" },
	{ }
};
MODULE_DEVICE_TABLE(of, my9250_of_match);

static const struct i2c_device_id my9250_id[] = {
	{ "my-mpu9250", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, my9250_id);

static struct i2c_driver my9250_driver = {
	.driver = {
		.name = "my-mpu9250",
		.of_match_table = my9250_of_match,
	},
	.probe = my9250_probe,
	.remove = my9250_remove,
	.id_table = my9250_id,
};
module_i2c_driver(my9250_driver);

MODULE_AUTHOR("you");
MODULE_DESCRIPTION("Minimal MPU9250 IIO driver example");
MODULE_LICENSE("GPL");
