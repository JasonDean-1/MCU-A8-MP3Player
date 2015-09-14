/*
 * Buttons scan device driver
 *
 * Author: Charles Yang (QQ: 582913383)
 * Company: ★博航嵌入式 北航博士店★
 * Web: http://www.broadon.cn  http://armdspfpga.taobao.com
 
 ****************************************************************************************
ARM/DSP/FPGA全系列开发板全国代理★博航嵌入式 北航博士店★

北京博航恒业科技有限公司
地址：北京市海淀区知春路118号，知春电子城3层1325室
电话：13581501210/010-62659896 
传真：010-62659896
网址：http://www.broadon.cn
网店：http://armdspfpga.taobao.com
Q Q： 313638714 
MSN： jin--yong@hotmail.com 
Email： lyw@broadon.cn 
联系人： 梁先生
******************************************************************************************
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

MODULE_AUTHOR("http://www.broadon.cn  http://armdspfpga.taobao.com");     
MODULE_DESCRIPTION("S5PV210 Buttons Driver");
MODULE_LICENSE("Dual BSD/GPL");

/*
 * We export one key device.  There's no need for us to maintain any
 * special housekeeping info, so we just deal with raw cdev.
 */
static struct cdev buttons_cdev_broadon;
static int buttons_major_broadon = 0;
static struct class *buttons_cls_broadon;
static struct device *buttons_dev_broadon;
static struct timer_list buttons_timer_broadon;

struct buttons_broadon {
	int number;
	int pin;
	unsigned int irq;
	char *name;	
};

/* 
 *  Initialize the buttons_broadon structure such as irqno, pin, etc.
 */
static struct buttons_broadon buttons_broadon_irq_arr [] = {
    {0, S5PV210_GPH2(0),   IRQ_EINT(16),  "KEY1"},    /* K1 */
    {1, S5PV210_GPH2(1),   IRQ_EINT(17),  "KEY2"},    /* K2 */
    {2, S5PV210_GPH2(2),   IRQ_EINT(18),  "KEY3"},    /* K3 */
    {3, S5PV210_GPH2(3),   IRQ_EINT(19),  "KEY4"},    /* K4 */
    {4, S5PV210_GPH3(0),   IRQ_EINT(24),  "KEY5"},    /* K5 */
    {5, S5PV210_GPH3(1),   IRQ_EINT(25),  "KEY6"},    /* K6 */
    {6, S5PV210_GPH3(2),   IRQ_EINT(26),  "KEY7"},    /* K7 */
    {7, S5PV210_GPH3(3),   IRQ_EINT(27),  "KEY8"},    /* K8 */
};

static int buttons_broadon_values = 0;
struct buttons_broadon *buttons_broadon_irq;

/* 
 *  Declare the buttons wait queue
 */
static DECLARE_WAIT_QUEUE_HEAD(buttons_broadon_waitqueue);

/* 
 *  Desc: The flag of the interrupt event
 *        This value will be set to 1 when invoking the interrupt service routine,
 *        and it'll be set to 0 when invoking buttons_read_broadon function.
 */
static volatile int condition = 0;

/*  
 *  Desc:       Buttons interrupt service routine
 *  @params:    irq:    the irq number
 *              dev_id: the specific member of the buttons_broadon_irq array when the interrupt is occured.
 *  @return:    return whether the interrupt is handled successfully or not.
 */
static irqreturn_t buttons_isr_broadon(int irq, void *dev_id)
{
    buttons_broadon_irq = (struct buttons_broadon *)dev_id;
	mod_timer(&buttons_timer_broadon, jiffies+HZ/100);
	return IRQ_RETVAL(IRQ_HANDLED);	
}

static void buttons_timer_fnc_broadon(unsigned long data)
{    
    struct buttons_broadon *buttons_broadon_tmp = buttons_broadon_irq;
	int up = gpio_get_value(buttons_broadon_tmp->pin);

    if (!buttons_broadon_tmp)
        return;

	if (up) {
		condition = 0;
	} else {
		buttons_broadon_values = buttons_broadon_tmp->number;
		condition = 1;
	}	
    	
	wake_up_interruptible(&buttons_broadon_waitqueue);
}

/* 
 *  Desc:   buttons open function which will be invoked by app's open function
 *          the open function will request the buttons' irq.
 *  @param:     inode:  structure inode
 *              file:   structure file
 *  @return:    return whether the open function is invoked successfully or not
 */
static int buttons_open_broadon(struct inode *inode, struct file *file)
{
    int i;
    int err;
    
    for (i = 0; i < sizeof(buttons_broadon_irq_arr)/sizeof(buttons_broadon_irq_arr[0]); i++) {
        err = request_irq(buttons_broadon_irq_arr[i].irq, buttons_isr_broadon, IRQ_TYPE_EDGE_FALLING, 
                          buttons_broadon_irq_arr[i].name, (void *)&buttons_broadon_irq_arr[i]);

        if (err)
            break;
    }

    if (err) {
        i--;
        for (; i >= 0; i--) {
    		disable_irq(buttons_broadon_irq_arr[i].irq);
        	free_irq(buttons_broadon_irq_arr[i].irq, (void *)&buttons_broadon_irq_arr[i]);
        }
        return -EBUSY;
    }
    
    return 0;
}


/* 
 *  Desc:   buttons close function which will be invoked by app's close function
 *          the close function will disable the buttons' irq
 *  @param:     inode:  structure inode
 *              file:   structure file
 *  @return:    return whether the close function is invoked successfully or not
 */
static int buttons_close_broadon(struct inode *inode, struct file *file)
{
    int i;
    for (i = 0; i < sizeof(buttons_broadon_irq_arr)/sizeof(buttons_broadon_irq_arr[0]); i++) {
    	disable_irq(buttons_broadon_irq_arr[i].irq);
        free_irq(buttons_broadon_irq_arr[i].irq, (void *)&buttons_broadon_irq_arr[i]);
    }
    
    return 0;
}


/* 
 *  Desc:   buttons read function which will be invoked by app's read function
 *          the read function will copy the specific button's info to the user space
 *  @param:     filp:   structure file.
 *              buff:   buffer resides in the user space.
 *              count:  the size of the button's value.
 *              offp:   the offset of the file.
 *  @return:    return the actual size of the button's value or error.
 */
static int buttons_read_broadon(struct file *filp, char __user *buff, 
                        size_t count, loff_t *offp)
{
    unsigned long err;

	if (!condition) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else
            /*  
             *  If condition is 0, wait until the buttons_broadon_waitqueue is awakened 
             *  and the condition is 1 
             */
			wait_event_interruptible(buttons_broadon_waitqueue, condition);
	}

    if(count != sizeof buttons_broadon_values)
	    return -EINVAL;

    /* Clear the condition to 0 */
    condition = 0;

    /* Copy the buttion's value to the user space */
    err = copy_to_user(buff, &buttons_broadon_values, sizeof(buttons_broadon_values));

    return err ? -EFAULT : sizeof(buttons_broadon_values);
}

/*
 *  Desc:       This functions is invoked by select function of the app.
 *  
 *  @param:     file:   the file structure.
 *              wait:   poll_wait table
 *  @return:    return whether the button info is ready to read or not.
 */
static unsigned int buttons_poll_broadon(struct file *file,
        			 struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	poll_wait(file, &buttons_broadon_waitqueue, wait);
	if (condition)
    	mask |= POLLIN | POLLRDNORM;
	return mask;
}

/*
 *  Desc:   The file_operations structure contains the open, read, write functions
 *          These functions are invoked by the app's open, read, write respectively.
 */
static struct file_operations buttons_fops_broadon = {
    .owner   =   THIS_MODULE,
    .open    =   buttons_open_broadon,
    .release =   buttons_close_broadon, 
    .read    =   buttons_read_broadon,
    .poll    =   buttons_poll_broadon,
};

/*
 *  Desc:       Set up the cdev structure for a device.
 *
 *  @params:    dev:    the cdev structure.
 *              minor:  the minor number.
 *              fops:   the file_operations structure.
 *  @return:    no return.
 */
static void buttons_setup_cdev(struct cdev *dev, int minor, struct file_operations *fops)
{
    int err, devno = MKDEV(buttons_major_broadon, minor);

    cdev_init(dev, fops);
    dev->owner = THIS_MODULE;
    dev->ops = fops;
    err = cdev_add(dev, devno, 1);

    /* Fail gracefully if need be */
    if (err)
        printk (KERN_NOTICE "Error %d adding button%d", err, minor);
}

/*
 *  Desc:       The buttons init function which will be invoked by the insmod buttons_drv_mini2440.ko.
 *  @params:    NONE
 *  @return:    return whether the init functions is invoked successfully or not.
 */
static int __init buttons_init_broadon(void)
{
    int result;
    dev_t dev = MKDEV(buttons_major_broadon, 0);
	char dev_name[] = "buttons_broadon";
                                                                                                         
    /* Figure out our device number. */
    if (buttons_major_broadon)
        result = register_chrdev_region(dev, 1, dev_name);
    else {
        result = alloc_chrdev_region(&dev, 0, 1, dev_name);
        buttons_major_broadon = MAJOR(dev);
    }

    if (result < 0) {
        printk(KERN_WARNING "buttons: unable to get major %d\n", buttons_major_broadon);
        return result;
    }

    if (buttons_major_broadon == 0)
        buttons_major_broadon = result;

    /* Now set up cdev. */
    buttons_setup_cdev(&buttons_cdev_broadon, 0, &buttons_fops_broadon);

    /* Creating the buttons' class */
    buttons_cls_broadon = class_create(THIS_MODULE, "buttons_broadon");
	if (IS_ERR(buttons_cls_broadon)) {
        cdev_del(&buttons_cdev_broadon);        
        unregister_chrdev_region(MKDEV(buttons_major_broadon, 0), 1);
        return -EFAULT;
	}

    /* Creating the buttons' device */
    buttons_dev_broadon = device_create(buttons_cls_broadon, NULL, MKDEV(buttons_major_broadon, 0), NULL, dev_name);    
	if (IS_ERR(buttons_dev_broadon)) {
        class_destroy(buttons_cls_broadon);        
        cdev_del(&buttons_cdev_broadon);        
        unregister_chrdev_region(MKDEV(buttons_major_broadon, 0), 1);
        return PTR_ERR(buttons_dev_broadon);
	}

    /* Setup timer */    
	init_timer(&buttons_timer_broadon);
	buttons_timer_broadon.function = buttons_timer_fnc_broadon;
	add_timer(&buttons_timer_broadon); 
    
    printk("Buttons' device are installed successfully, with major %d\n", buttons_major_broadon);
 
    return 0;
}
module_init(buttons_init_broadon);

/*
 *  Desc:       The buttons exit function which will be invoked by the rmmod buttons_drv_mini2440.
 *  @params:    NONE
 *  @return:    no return.
 */
static void __exit buttons_exit_broadon(void)
{
    del_timer(&buttons_timer_broadon);
    device_destroy(buttons_cls_broadon, MKDEV(buttons_major_broadon, 0));
    class_destroy(buttons_cls_broadon);
    cdev_del(&buttons_cdev_broadon);
    unregister_chrdev_region(MKDEV(buttons_major_broadon, 0), 1);
    printk("Buttons' device are removed successfully!\n");
}
module_exit(buttons_exit_broadon);

