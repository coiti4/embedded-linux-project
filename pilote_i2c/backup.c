#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/wait.h>

#define qsize 16

/* Count nb of times probe is called */
uint8_t probe_nb = 0;

struct fifo_element {
    int16_t x;
    int16_t y;
    int16_t z;
};

/* Declare a struct adxl345_device structure containing for the moment a single struct
miscdevice field */
struct adxl345_device {
    struct miscdevice miscdev;
    DECLARE_KFIFO(fifo, struct fifo_element, qsize);
};

static irqreturn_t adxl345_int(int irq, void *dev_id) 
{
    pr_err("in interruption func\n");
    
    return IRQ_HANDLED;
}

ssize_t adxl345_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    /* f_pos is the current position in the file, to be updated. In the case of devices,
    the notion of current position in the f_pos file often has no meaning */
    // count is the maximum number of bytes to read
    // buf is the buffer (in user space) to fill with data from the device
    // filp the structure file associated with the device
    
    /* Returns the actual number of bytes written to buf (which may be less than count), 0
    to indicate the end of the file or a negative number to indicate an error */
    int i; // variable that will indicate the number of bytes written to buf
    /* Retrieve adxl345 using filp->private_data */
    struct adxl345_device *adxl345 = container_of(filp->private_data, struct adxl345_device, miscdev);

    /* Retrieve the instance of the i2c_client corresponding to the device. */
    struct i2c_client *client = to_i2c_client(adxl345->miscdev.parent);

    char reg[2];
    char kbuf[2];
    reg[0] = 0x33; // DATAX1 register
    reg[1] = 0x32; // DATAX0 register
    /* Get a sample from the accelerometer */
    count = count > 2 ? 2 : count;
    for (i = 0; i < count; i++) {
        if (i2c_master_send(client, reg + i, 1) < 0) {
            pr_err("Error sending DATAX data\n");
            return -1;
        }
        if (i2c_master_recv(client, kbuf + i, 1) < 0) {
            pr_err("Error receiving DATAX data\n");
            return -1;
        }
        if (copy_to_user(buf + i, kbuf + i, sizeof(kbuf))){
            pr_err("Error copying user buf\n");
            return -1;
        }
    }
    return i;
}   

static const struct file_operations adxl345_fops = {
    .owner = THIS_MODULE,
    .read = adxl345_read};





static int adxl345_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    char buf [2];
    char *name;
    int irq = 50;
    wait_queue_head_t queue_;
    /* Dynamically allocate memory for an instance of the struct adxl345_device */
    struct adxl345_device *adxl345;
    /* Allocate memory for the adxl345 device */
    adxl345 = kmalloc(sizeof(struct adxl345_device), GFP_KERNEL);
    if (!adxl345) {
        pr_err("Error allocating memory for adxl345 device\n");
        return -ENOMEM;
    }
    // read the DEVID register of the accelerometer. This register contains a fixed value (0xE5)
    buf[0] = 0x00;
    
    name = kasprintf(GFP_KERNEL, "adxl345-%d", probe_nb);
    if (!name)
    {
        pr_err("Error allocation failure\n");
        return -ENOMEM;
    }
    /* Increment nb times probe is called */
    probe_nb++;

    /* Fill miscdevice structure of adxl345_device */
    adxl345->miscdev.minor = MISC_DYNAMIC_MINOR;
    adxl345->miscdev.name = name;
    adxl345->miscdev.fops = &adxl345_fops;
    adxl345->miscdev.parent = &client->dev; 

    /* Associate this instance with the struct i2c_client */
    i2c_set_clientdata(client, adxl345); // This function allows to store any pointer in the i2c_client structure
    
    /* Register miscdevice structure */
    if (misc_register(&adxl345->miscdev)) {
        pr_err("Error registering misc device\n");
        return -1;
    }

    if (i2c_master_send(client, buf, 1) < 0) {
        pr_err("Error sending DEVID data\n");
        return -1;
    }
    if (i2c_master_recv(client, buf, 1) < 0) {
        pr_err("Error receiving DEVID data\n");
        return -1;
    }
    pr_err("DEVID register value: 0x%x\n", buf[0]);

    /* Output data rate: 100 Hz (output data rate, BW_RATE register) */
    buf[0] = 0x2C;
    buf[1] = 0x0A; // Normal operation, 100Hz output
    
    if (i2c_master_send(client, buf, 2) < 0) {
        pr_err("Error sending BW_RATE data\n");
        return -1;
    }

    /*watermark interrupts enabled (INT_ENABLE register) */
    buf[0] = 0x2E;
    buf[1] = 0x02; // watermark interrupts enabled

    if (i2c_master_send(client, buf, 2) < 0) {
        pr_err("Error sending INT_ENABLE data\n");
        return -1;
    }

    /* Default data format (DATA_FORMAT register) */
    buf[0] = 0x31;
    buf[1] = 0x00; // Default data format

    if (i2c_master_send(client, buf, 2) < 0) {
        pr_err("Error sending DATA_FORMAT data\n");
        return -1;
    }

    /* FIFO stream mode (stream mode, FIFO_CTL register) */
    buf[0] = 0x38;
    buf[1] = 0x54; // Stream mode, 20 samples

    if (i2c_master_send(client, buf, 2) < 0) {
        pr_err("Error sending FIFO_CTL data\n");
        return -1;
    }

    /* Measurement mode activated (POWER_CTL register) */
    buf[0] = 0x2D;
    buf[1] = 0x08; // Measurement mode activated

    if (i2c_master_send(client, buf, 2) < 0) {
        pr_err("Error sending POWER_CTL data\n");
        return -1;
    }

    pr_err("aca\n");

    if (devm_request_threaded_irq(&client->dev, irq, NULL, adxl345_int,
        IRQF_ONESHOT, "adxl345", adxl345)) {
        pr_err("Error requesting threaded irq\n");
        return -1;
    }

    // Initialise la FIFO avant son utilisation :
    INIT_KFIFO(adxl345->fifo);

    // Initialise la fille d'attente :
    DECLARE_WAIT_QUEUE_HEAD(queue_);

    pr_err("ADXL345 initialized\n");


    return 0;
}
static int adxl345_remove(struct i2c_client *client)
{
    char buf [2];
    /* Retrieve adxl345 using i2c_get_clientdata*/
    struct adxl345_device *adxl345 = i2c_get_clientdata(client);

    /* Decrement nb times probe is called */
    probe_nb--;
    /* Standby mode (POWER_CTL register) */
    buf[0] = 0x2D;
    buf[1] = 0x00; // Standby mode

    if (i2c_master_send(client, buf, 2) < 0) {
        pr_err("Error sending POWER_CTL data\n");
        return -1;
    }

    /* Unregister miscdevice structure */
    misc_deregister(&adxl345->miscdev);

    /* Free memory for the adxl345 device */
    kfree(adxl345->miscdev.name);
    kfree(adxl345);
    
    pr_err("ADXL345 removed\n");

    return 0;
}
/* The following list allows the association between a device and its driver
driver in the case of a static initialization without using
device tree.
Each entry contains a string used to make the association
association and an integer that can be used by the driver to
driver to perform different treatments depending on the physical
the physical device detected (case of a driver that can manage
different device models).*/
static struct i2c_device_id adxl345_idtable[] = {
    { "qemu,adxl345", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, adxl345_idtable);
#ifdef CONFIG_OF
    /* If device tree support is available, the following list
    allows to make the association using the device tree.
    Each entry contains a structure of type of_device_id. The field
    compatible field is a string that is used to make the association
    with the compatible fields in the device tree. The data field is
    a void* pointer that can be used by the driver to perform different
    perform different treatments depending on the physical device detected.
    device detected.*/
    static const struct of_device_id adxl345_of_match[] = {
    { .compatible = "vendor,adxl345" }, 
        {}
    };
    MODULE_DEVICE_TABLE(of, adxl345_of_match);
#endif  
static struct i2c_driver adxl345_driver = {
.driver = {
        /* The name field must correspond to the name of the module
        and must not contain spaces. */
        .name = "qemu,adxl345",
        .of_match_table = of_match_ptr(adxl345_of_match),
    },
        .id_table = adxl345_idtable,
        .probe = adxl345_probe,
        .remove = adxl345_remove,
};

module_i2c_driver(adxl345_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("adxl345 driver");
MODULE_AUTHOR("Me");
