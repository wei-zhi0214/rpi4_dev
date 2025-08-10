#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>

#define DRIVER_NAME "myi2c"

static int myi2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    dev_info(&client->dev, "myi2c device probed at 0x%02x\n", client->addr);

    // 測試從 0x00 讀取一個 byte
    int val = i2c_smbus_read_byte_data(client, 0x00);
    if (val < 0)
        dev_err(&client->dev, "Failed to read from device\n");
    else
        dev_info(&client->dev, "Value at reg 0x00: 0x%02x\n", val);

    return 0;
}

static void myi2c_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "myi2c device removed\n");
    return ;
}

static const struct of_device_id myi2c_of_match[] = {
    { .compatible = "mycompany,myi2c", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, myi2c_of_match);

static const struct i2c_device_id myi2c_id[] = {
    { DRIVER_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, myi2c_id);

static struct i2c_driver myi2c_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = myi2c_of_match,
    },
    .probe = myi2c_probe,
    .remove = myi2c_remove,
    .id_table = myi2c_id,
};

module_i2c_driver(myi2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("你");
MODULE_DESCRIPTION("Simple I2C Client Driver");

