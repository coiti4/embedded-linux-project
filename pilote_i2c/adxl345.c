#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define qsize (16)

/* Count nb of times probe is called */
uint8_t probe_nb = 0;
DECLARE_WAIT_QUEUE_HEAD(queue_);

struct fifo_element
{
    int16_t x;
    int16_t y;
    int16_t z;
};

/* Declare a struct adxl345_device structure containing for the moment a single struct
miscdevice field */
struct adxl345_device {
    struct miscdevice miscdev;
    uint8_t option;
    //DECLARE_KFIFO(fifo, struct fifo_element, qsize);
    DECLARE_KFIFO_PTR(fifo, struct fifo_element);
};

static irqreturn_t adxl345_int(int irq, void *dev_id){
    struct adxl345_device *device = dev_id;
    struct i2c_client *client = to_i2c_client(device->miscdev.parent);
    struct fifo_element element, element_foo;
    int nb_samples, ret, i;
    char buf[1];
    buf[0] = 0x39;
    // pr_err("in interruption func\n");

    // Recuperez le nombre d echantillons disponibles dans la FIFO de l accelerometre (registre FIFO_STATUS)
    if (i2c_master_send(client, buf, 1) < 0)
    {
        pr_err("Error sending FIFO_STATUS data\n");
        return -1;
    }
    if (i2c_master_recv(client, buf, 1) < 0)
    {
        pr_err("Error receiving FIFO_STATUS data\n");
        return -1;
    }
    // pr_err("FIFO_STATUS register value: 0x%x\n", buf[0]);

    nb_samples = buf[0] & 0x3F; // Entries D5-D0 reports how many data values are stored in FIFO

    // Recuperez tous les echantillons depuis la FIFO de l accelerometre et stockez les dans votre FIFO interne
    buf[0] = 0x32; // X0
    for (i = 0; i < nb_samples; i++)
    {
        if (i2c_master_send(client, buf, 1) < 0)
        {
            pr_err("Error sending FIFO_STATUS data\n");
            return -1;
        }
        if (i2c_master_recv(client, (void *)&element, sizeof(struct fifo_element)) < 0)
        {
            pr_err("Error receiving FIFO_STATUS data\n");
            return -1;
        }
        ret = kfifo_in(&device->fifo, &element, 1);
        if (ret < 0)
        {
            pr_err("Error adding element to fifo\n");
            return -1;
        }
        else if (ret == 0)
        {
            // fifo full, so take out oldest element and put back newest
            if (!kfifo_out(&device->fifo, &element_foo, 1))
            {
                pr_err("Error getting element from fifo\n");
                return -1;
            }
            if (!kfifo_in(&device->fifo, &element, 1))
            {
                pr_err("Error adding element to fifo\n");
                return -1;
            }
        }
    }

    // Reveillez les eventuels processus en attente de donnees
    wake_up(&queue_);
    return IRQ_HANDLED;
}

long adxl345_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct adxl345_device *device = container_of(file->private_data, struct adxl345_device, miscdev);
    //struct i2c_client *client = to_i2c_client(device->mdev.parent);
    pr_err("IOCTL");
    /*let the application chose if we want to chose axis x, y or z*/
    switch (arg)
    {
    case 0:
        // X axis
        device->option = 0;
        break;
    case 1:
        // Y axis
        device->option = 1;
        break;
    case 2:
        // Z axis
        device->option = 2;
        break;
    default:
        pr_err("Error: invalid option\n");
        break;
    }
    return 0;
}

ssize_t adxl345_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    struct adxl345_device *device = container_of(filp->private_data, struct adxl345_device, miscdev);
    /*Retrieve struct i2c_client*/
    // struct i2c_client *client = to_i2c_client(device->mdev.parent);
    // char buf_l;
    struct fifo_element element;

    // Si non, mettez le processus en attente
    if (wait_event_interruptible(queue_, (!kfifo_is_empty(&device->fifo))))
    {
        pr_err("Error waiting for event\n");
        return -1;
    }

    // Renvoyez les donnees depuis la FIFO interne

    if (!kfifo_out(&device->fifo, &element, 1))
    {
        pr_err("Error getting element from fifo\n");
        return -1;
    }
    // Pass all or part of the data retrieved to the application
    switch (device->option)
    {
    case 0:
        pr_err("X axis");
        if (copy_to_user(buf, &(element.x), 2))
        {
            pr_err("Error copying data to user\n");
            return -1;
        }
        pr_err("2 bytes of data have been sent to user");
        break;
    case 1:
        pr_err("Y axis");
        if (copy_to_user(buf, &(element.y), 2))
        {
            pr_err("Error copying data to user\n");
            return -1;
        }
        pr_err("2 bytes of data have been sent to user");
        break;
    case 2:
        pr_err("Z axis");
        if (copy_to_user(buf, &(element.z), 2))
        {
            pr_err("Error copying data to user\n");
            return -1;
        }
        pr_err("2 bytes of data have been sent to user");
        break;
    case 3:
        pr_err("ALL axis");
        if (copy_to_user(buf, &(element), 6))
        {
            pr_err("Error copying data to user\n");
            return -1;
        }
        pr_err("2 bytes of data have been sent to user");
        break;
    default:
        pr_err("Error: invalid option\n");
        break;
    }

    pr_err("Read function\n");
    pr_err("data remaining in fifo: %d\n", kfifo_len(&device->fifo));
    return (device->option == 3) ? 6 : 2;
}   

static const struct file_operations adxl345_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = adxl345_ioctl,
    .read = adxl345_read};

static int adxl345_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    char buf [2];
    char *name;
    int err;
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

    /* watermarks interrupts enabled (INT_ENABLE register) */
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

    /* FIFO stream (stream mode, FIFO_CTL register) */
    buf[0] = 0x38;
    buf[1] = 0x54; // FIFO stream mode

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

    /* Fill miscdevice structure of adxl345_device */
    adxl345->miscdev.minor = MISC_DYNAMIC_MINOR;
    adxl345->miscdev.name = name;
    adxl345->miscdev.fops = &adxl345_fops;
    adxl345->miscdev.parent = &client->dev; 

    /* Associate this instance with the struct i2c_client */
    i2c_set_clientdata(client, adxl345); // This function allows to store any pointer in the i2c_client structure
    // pr_err("hola\n");
    /* Register miscdevice structure */
    if (misc_register(&adxl345->miscdev)) {
        pr_err("Error registering misc device\n");
        return -1;
    }
    // pr_err("chau\n");

    err = devm_request_threaded_irq(&client->dev, client->irq, NULL, adxl345_int, IRQF_ONESHOT, name, adxl345);
    if (err < 0)
    {
        pr_err("Error requesting irq\n");
        return -1;
    }

    // Initialise la FIFO avant son utilisation :
    //INIT_KFIFO(adxl345->fifo);
    if (kfifo_alloc(&adxl345->fifo, qsize, GFP_KERNEL))
    {
        pr_err("Error allocating fifo\n");
        return -1;
    }

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
    kfifo_free(&adxl345->fifo);
    
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
