#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#define MPU9250_WHO_AM_I 0x75
#define MPU9250_PWR_MGMT_1 0x6B
#define MPU9250_INT_PIN_CFG 0x37

static const struct regmap_config mpu9250_regmap_cfg = {
    .reg_bits = 8,
    .val_bits = 8,
};

struct mpu9250
{
    struct i2c_client *client;
    struct regmap *regmap;
};

static int mpu9250_hw_init(struct mpu9250* st)
{
    int ret, val;
    ret = regmap_read(st->regmap, MPU9250_WHO_AM_I, &val);
    if(ret) return ret;
    if(val != 0x71) return -ENODEV;

    ret = regmap_write(st->regmap, MPU9250_PWR_MGMT_1, 0x00);
    if(ret) return ret;

    ret = regmap_write(st->regmap, MPU9250_INT_PIN_CFG, 0x02);
    return ret;
}

static int mpu9250_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
    struct mpu9250* st = devm_kzalloc(&client->dev, sizeof(*st), GFP_KERNEL);
    if(!st) return -ENOMEM;
    st->client = client;
    st->regmap = devm_regmap_init_i2c(client, &mpu9250_regmap_cfg);
    return mpu9250_hw_init(st);
}

static void mpu9250_remove(struct i2c_cilent* client) { }

static const struct of_device_id mpu9250_of_match[] = {
    {
        .compatible = "invensense, mpu9250"
    },
    {

    }
};
MODULE_DEVICE_TABLE(of, mpu9250_of_match);

static const struct i2c_device_id mpu9250_id[] = {
    { "mpu9250", 0 }, {}
}
MODULE_DEVICE_TABLE(i2c, mpu9250_id);

static struct i2c_driver mpu9250_driver = {
    .driver = {
        .name = "mpu9250",
        .of_match_table = mpu9250_of_match,
    },
    .probe = mpu9250_probe,
    .remove = mpu9250_remove,
    .id_table = mpu9250_id,
};
module_i2c_driver(mpu9250_driver);

MODULE_LICENSE("GPL");