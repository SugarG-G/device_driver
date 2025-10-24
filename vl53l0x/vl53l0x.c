// Minimal VL53L0X I2C kernel module skeleton (register access + XSHUT control)
// NOTE: This does not implement full ranging; ST's full init/algorithms are not public.

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/slab.h>

struct vl53l0x_data {
	struct i2c_client *client;
	struct regmap *regmap;
	struct gpio_desc *xshutdown; /* optional, active-low */
	u16 reg_addr; /* sysfs-selected register address (16-bit) */
};

static const struct regmap_config vl53l0x_regmap_cfg = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
	.cache_type = REGCACHE_NONE,
};

static ssize_t reg_addr_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct vl53l0x_data *data = dev_get_drvdata(dev);
	return sysfs_emit(buf, "0x%04x\n", data->reg_addr);
}

static ssize_t reg_addr_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct vl53l0x_data *data = dev_get_drvdata(dev);
	unsigned int addr;
	if (kstrtouint(buf, 0, &addr))
		return -EINVAL;
	if (addr > 0xFFFF)
		return -EINVAL;
	data->reg_addr = (u16)addr;
	return count;
}
static DEVICE_ATTR_RW(reg_addr);

static ssize_t reg_val_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct vl53l0x_data *data = dev_get_drvdata(dev);
	unsigned int val;
	int ret = regmap_read(data->regmap, data->reg_addr, &val);
	if (ret)
		return ret;
	return sysfs_emit(buf, "0x%02x\n", val & 0xFF);
}

static ssize_t reg_val_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct vl53l0x_data *data = dev_get_drvdata(dev);
	unsigned int val;
	if (kstrtouint(buf, 0, &val))
		return -EINVAL;
	if (val > 0xFF)
		return -EINVAL;
	if (regmap_write(data->regmap, data->reg_addr, (u8)val))
		return -EIO;
	return count;
}
static DEVICE_ATTR_RW(reg_val);

static ssize_t xshut_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct vl53l0x_data *data = dev_get_drvdata(dev);
	int active;
	if (!data->xshutdown)
		return sysfs_emit(buf, "-1\n");
	/* gpiod_get_value returns logical value: 1 == active (asserted), 0 == inactive (released) */
	active = gpiod_get_value_cansleep(data->xshutdown);
	if (active < 0)
		return active;
	return sysfs_emit(buf, "%d\n", active ? 0 : 1);
}

static ssize_t xshut_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct vl53l0x_data *data = dev_get_drvdata(dev);
	unsigned long v;
	if (!data->xshutdown)
		return -ENODEV;
	if (kstrtoul(buf, 0, &v))
		return -EINVAL;
	/* write 1 to release (inactive), 0 to assert reset (active) */
	gpiod_set_value_cansleep(data->xshutdown, v ? 0 : 1);
	if (v) /* give sensor time to boot when releasing reset */
		usleep_range(1000, 2000);
	return count;
}
static DEVICE_ATTR_RW(xshut);

static struct attribute *vl53l0x_attrs[] = {
	&dev_attr_reg_addr.attr,
	&dev_attr_reg_val.attr,
	&dev_attr_xshut.attr,
	NULL,
};

static const struct attribute_group vl53l0x_attr_group = {
	.attrs = vl53l0x_attrs,
};

static int vl53l0x_probe(struct i2c_client *client)
{
	struct vl53l0x_data *data;
	int ret;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;

	/* Optional XSHUT line (active-low). Default to released (inactive). */
	data->xshutdown = devm_gpiod_get_optional(&client->dev, "xshutdown", GPIOD_OUT_LOW);
	if (IS_ERR(data->xshutdown))
		return PTR_ERR(data->xshutdown);

	if (data->xshutdown) {
		/* Ensure the device is out of reset */
		gpiod_set_value_cansleep(data->xshutdown, 0);
		usleep_range(1000, 2000);
	}

	data->regmap = devm_regmap_init_i2c(client, &vl53l0x_regmap_cfg);
	if (IS_ERR(data->regmap))
		return dev_err_probe(&client->dev, PTR_ERR(data->regmap), "regmap init failed\n");

	/* Default sysfs register address */
	data->reg_addr = 0x0000;

	i2c_set_clientdata(client, data);

	ret = sysfs_create_group(&client->dev.kobj, &vl53l0x_attr_group);
	if (ret)
		return ret;

	dev_info(&client->dev, "VL53L0X skeleton bound at 0x%02x\n", client->addr);
	return 0;
}

static void vl53l0x_remove(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &vl53l0x_attr_group);
}

static const struct of_device_id vl53l0x_of_match[] = {
	{ .compatible = "opensource,vl53l0x-simple" },
	{}
};
MODULE_DEVICE_TABLE(of, vl53l0x_of_match);

static struct i2c_driver vl53l0x_i2c_driver = {
	.driver = {
		.name = "vl53l0x-simple",
		.of_match_table = vl53l0x_of_match,
	},
	.probe_new = vl53l0x_probe,
	.remove = vl53l0x_remove,
};

module_i2c_driver(vl53l0x_i2c_driver);

MODULE_AUTHOR("Your Name <you@example.com>");
MODULE_DESCRIPTION("VL53L0X minimal I2C driver (register access + XSHUT)");
MODULE_LICENSE("GPL");
