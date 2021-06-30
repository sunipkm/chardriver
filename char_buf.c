#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>	     /* this is the file structure, file open read close */
#include <linux/cdev.h>	     /* this is for character device, makes cdev avilable*/
#include <linux/semaphore.h> /* this is for the semaphore*/
#include <linux/uaccess.h>   /* this is for copy_user vice vers*/
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/compiler.h>
#include<linux/slab.h>

int chardev_init(void);
void chardev_exit(void);

static int device_open(struct inode *, struct file *);
static int device_close(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

/*new code*/
struct circ_buf *rxbuf;
#define RXBUF_LEN 128
volatile bool data_available;
static DECLARE_WAIT_QUEUE_HEAD(rxq);
struct cdev *mcdev; /* this is the name of my char driver that i will be registering*/
int major_number;   /* will store the major number extracted by dev_t*/
int ret;	    /* used to return values*/
dev_t dev_num;	    /* will hold the major number that the kernel gives*/

#define DEVICENAME "charDev"

/* inode reffers to the actual file on disk*/
static int device_open(struct inode *inode, struct file *filp)
{
	//buff_rptr = buff_wptr = device_buffer;
	// printk(KERN_INFO "charDev : device opened succesfully\n");
	return 0;
}

static ssize_t device_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
	int head, tail, byte_count, out_count, remainder, seq_len, new_tail, new_end;
	wait_event_interruptible(rxq, (data_available == true)); // wait while byte count is zero
	head = READ_ONCE(rxbuf->head);
	tail = READ_ONCE(rxbuf->tail);
	byte_count = CIRC_CNT(head, tail, RXBUF_LEN);
	printk(KERN_INFO "charDev : Read : head %d tail %d bytes %d\n", head, tail, byte_count);
	if (byte_count >= count)
	{
		out_count = count;
	}
	else if (byte_count > 0)
	{
		out_count = byte_count;
		WRITE_ONCE(data_available, false);
	}
	else // should not trigger
	{
		WRITE_ONCE(data_available, false);
		goto ret;
	}
	new_end = CIRC_CNT_TO_END(head, tail, RXBUF_LEN) + 1;
	remainder = out_count % new_end;
	seq_len = out_count - remainder;
	new_tail = (tail + out_count) & (RXBUF_LEN - 1);
	printk(KERN_INFO "charDev : Read : End %d Rem %d Seq %d Out %d Tail %d\n", new_end, remainder, seq_len, out_count, new_tail);
	/* Write the block making sure to wrap around the end of the buffer */
	out_count -= copy_to_user(buf, rxbuf->buf + tail, remainder); // copy_to_user returns bytes not written
	out_count -= copy_to_user(buf + remainder, rxbuf->buf, seq_len);
	new_tail = (tail + out_count) & (RXBUF_LEN - 1);
	WRITE_ONCE(rxbuf->tail, new_tail);
	return out_count;
ret:
	return 0;
}

static ssize_t device_write(struct file *fp, const char *buff, size_t len, loff_t *ppos)
{
	int head, tail, space, new_head;
	head = READ_ONCE(rxbuf->head);
	tail = READ_ONCE(rxbuf->tail);
	space = CIRC_SPACE(head, READ_ONCE(rxbuf->tail), RXBUF_LEN);
	printk(KERN_INFO "charDev : Write : Writing, head %d tail %d space %d bytes %lu", head, tail, space, len);
	if (space >= len)
	{
		int remainder = len % (CIRC_SPACE_TO_END(head, tail, RXBUF_LEN) + 1);
		int seq_len = len - remainder;
		new_head = (head + len) & (RXBUF_LEN - 1);
		printk("charDev : Write : Rem %d Seq %d new head: %d\n", remainder, seq_len, new_head);
		/* Write the block making sure to wrap around the end of the buffer */
		copy_from_user(rxbuf->buf + head, buff , remainder);
		copy_from_user(rxbuf->buf, buff + remainder, seq_len);
		WRITE_ONCE(rxbuf->head, new_head);
		WRITE_ONCE(data_available, true);
		wake_up_interruptible(&rxq);
	}
	else
	{
		printk(KERN_ERR "charDev : Buffer full\n");
		return -ENOBUFS;
	}
	return len;
}

static int device_close(struct inode *inode, struct file *filp)
{
	// printk(KERN_INFO "charDev : device has been closed\n");
	return ret;
}

struct file_operations fops = {
    /* these are the file operations provided by our driver */
    .owner = THIS_MODULE,    /* prevents unloading when operations are in use*/
    .open = device_open,     /* to open the device*/
    .write = device_write,   /* to write to the device*/
    .read = device_read,     /* to read the device*/
    .release = device_close, /* to close the device*/
};

int chardev_init(void)
{
	/* we will get the major number dynamically this is recommended please read ldd3*/
	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICENAME);
	if (ret < 0)
	{
		printk(KERN_ALERT " charDev : failed to allocate major number\n");
		return ret;
	}
	else
		printk(KERN_INFO " charDev : mjor number allocated succesful\n");
	major_number = MAJOR(dev_num);
	printk(KERN_INFO "charDev : major number of our device is %d\n", major_number);
	printk(KERN_INFO "charDev : to use mknod /dev/%s c %d 0\n", DEVICENAME, major_number);
	printk(KERN_INFO "charDev : Execute chmod 0777 /dev/%s after running mknod\n", DEVICENAME);

	mcdev = cdev_alloc(); /* create, allocate and initialize our cdev structure*/
	mcdev->ops = &fops;   /* fops stand for our file operations*/
	mcdev->owner = THIS_MODULE;

	/* we have created and initialized our cdev structure now we need to
        add it to the kernel*/
	ret = cdev_add(mcdev, dev_num, 1);
	if (ret < 0)
	{
		printk(KERN_ALERT "charDev : device adding to the kerknel failed\n");
		return ret;
	}
	else
		printk(KERN_INFO "charDev : device additin to the kernel succesful\n");

	rxbuf = kmalloc(sizeof(struct circ_buf), GFP_KERNEL);
	if (!(rxbuf))
	{
		ret = -ENOMEM;
		printk(KERN_ERR "charDev : Error allocating mem for ringbuf\n");
		return ret;
	}
	rxbuf->buf = kmalloc(RXBUF_LEN, GFP_KERNEL);
	if (!rxbuf->buf)
	{
		ret = -ENOMEM;
		printk(KERN_ERR "charDev : Error allocating mem for ringbuf data buffer\n");
		kfree(rxbuf);
		return ret;
	}
	rxbuf->head = 0;
	rxbuf->tail = 0;
	data_available = false;
	return 0;
}

void chardev_exit(void)
{
	kfree(rxbuf->buf);
	kfree(rxbuf);
	cdev_del(mcdev); /*removing the structure that we added previously*/
	printk(KERN_INFO " CharDev : removed the mcdev from kernel\n");

	unregister_chrdev_region(dev_num, 1);
	printk(KERN_INFO " CharDev : unregistered the device numbers\n");
	printk(KERN_ALERT " charDev : character driver is exiting\n");
}

MODULE_AUTHOR("Trung Kien - letrungkien.k53.hut@gmail.com");
MODULE_DESCRIPTION("A BASIC CHAR DRIVER");

module_init(chardev_init);
module_exit(chardev_exit);