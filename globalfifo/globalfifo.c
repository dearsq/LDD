/*************************************************************************
    > File Name: globalfifo.c
    > Author: Younix
    > Mail: foreveryounix@gmail.com 
    > Created Time: 2016年10月18日 星期二 14时56分47秒
 ************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>		//kmalloc
#include <linux/uaccess.h>	//copy_from_user,copy_to_user

#define GLOBALFIFO_SIZE	0x1000
#define FIFO_CLEAR		0x01
#define GLOBALFIFO_MAJOR 230

static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);	//S_IRUGO 表示所有人都有权限


struct globalfifo_dev {
	struct cdev cdev;
    unsigned int current_len;   //表征当前 fifo 的有效数据的长度
	unsigned char mem[GLOBALFIFO_SIZE];
	struct mutex mutex;		//增加互斥体
    wait_queue_head_t r_wait;
    wait_queue_head_t w_wait;
};

struct globalfifo_dev *globalfifo_devp;


/* open 和 release */
static int globalfifo_open(struct inode *inode, struct file * filp)
{
	filp->private_data = globalfifo_devp;
	return 0;
}

static int globalfifo_release(struct inode *indoe, struct file * filp)
{
	return 0;
}


static long globalfifo_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
	struct globalfifo_dev * dev = filp->private_data;

	switch(cmd) {
	case FIFO_CLEAR:
		mutex_lock(&dev->mutex);
		dev->current_len = 0;
		memset(dev->mem, 0, GLOBALFIFO_SIZE);
		mutex_unlock(&dev->mutex);
		
		printk(KERN_INFO "globalfifo is set to zero \n");
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static unsigned int globalfifo_poll(struct file *filp, poll_table * wait)
{
    unsigned int mask = 0;
    struct globalfifo_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);

    poll_wait(filp, &dev->r_wait, wait);
    poll_wait(filp, &dev->w_wait, wait);

    if(dev->current_len != 0)
    {
        mask |= POLLIN | POLLRDNORM;
    }
    if(dev->current_len != GLOBALFIFO_SIZE)
    {
        mask |= POLLOUT | POLLWRNORM;
    }

    mutex_unlock(&dev->mutex);
    return mask;
}

/* 读写函数 */
static ssize_t globalfifo_read(struct file *filp, char __user * buf, size_t count, loff_t * ppos)
{
	int ret = 0;
	struct globalfifo_dev * dev = filp->private_data;
    DECLARE_WAITQUEUE(wait, current);       //1. 定义等待队列 wait 元素

	mutex_lock(&dev->mutex); 
    
    add_wait_queue(&dev->r_wait, &wait);    //2. 将等待队列 wait 添加到 等待队列头 r_wait 后

    while(dev->current_len == 0){           //fifo 没有数据,就循环
        if(filp->f_flags & O_NONBLOCK){     //非阻塞就退出
            ret = -EAGAIN;
            goto out;
        }
        __set_current_state(TASK_INTERRUPTIBLE);

        mutex_unlock(&dev->mutex);          //在 schedule() 前释放互斥体，因为 w_wait 可能会需要访问 互斥体

        schedule();
        if(signal_pending(current)){
            ret = -ERESTARTSYS;
            goto out2;
        }
	    mutex_lock(&dev->mutex); 
    }

    if(count > dev->current_len)
        count = dev->current_len;

	if(copy_to_user(buf, dev->mem, count)){
		ret = -EFAULT;
        goto out;
	}else {
        memcpy(dev->mem, dev->mem + count, dev->current_len - count);
        dev->current_len -= count;
        printk(KERN_INFO "read %zu bytes(s), current_len:%d\n",count,dev->current_len);

        wake_up_interruptible(&dev->w_wait);

		ret = count;
	}
out:    
	mutex_unlock(&dev->mutex);
out2:
    remove_wait_queue(&dev->r_wait, &wait);
    set_current_state(TASK_RUNNING);
    
	return ret;
}

static ssize_t globalfifo_write(struct file *filp,const char __user * buf, size_t count, loff_t * ppos)
{
	int ret = 0;
	struct globalfifo_dev * dev = filp->private_data;

    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->w_wait, &wait);

    while(dev->current_len == GLOBALFIFO_SIZE) {
        if(filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        __set_current_state(TASK_INTERRUPTIBLE);

        mutex_unlock(&dev->mutex);

        schedule();
        if(signal_pending(current)){
            ret = -ERESTARTSYS;
            goto out2;
        }
	    mutex_lock(&dev->mutex);
    }
    
	if(count > GLOBALFIFO_SIZE - dev->current_len)
		count = GLOBALFIFO_SIZE - dev->current_len;

	if(copy_from_user(dev->mem + dev->current_len, buf, count)){ 
		ret = -EFAULT; 
        goto out;
	}else {
        dev->current_len += count;
		printk(KERN_INFO "written %zu bytes(s),current_len:%d\n", count, dev->current_len);

        wake_up_interruptible(&dev->r_wait);
        ret = count;
	}

out:    
	mutex_unlock(&dev->mutex);
out2:
    remove_wait_queue(&dev->w_wait, &wait);
    set_current_state(TASK_RUNNING);

	return ret;
}

/* llseek 函数 */
/*
static loff_t globalfifo_llseek(struct file *filp, loff_t offset, int orig)	//orig 是标志位
{
	loff_t ret = 0;
	switch(orig) {
		case 0:				//从文件头开始
			if(offset < 0){
				ret = -EINVAL;
				break;
			}
			if((unsigned int)offset > GLOBALFIFO_SIZE){
				ret = -EINVAL;
				break;
			}
			filp->f_pos = (unsigned int)offset;			//一直写到
			ret = filp->f_pos;
			break;
		case 1:				//从 当前文件指针filp->fpos 开始
			if(filp->f_pos + offset < 0){
				ret = -EINVAL;
				break;
			}
			if((unsigned int)offset + filp->f_pos > GLOBALfifo_SIZE){
				ret = -EINVAL;
				break;
			}
			filp->f_pos += offset;
			ret = filp->f_pos;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}
*/
static const struct file_operations globalfifo_fops =  {
	.owner	=	THIS_MODULE,
	//.llseek	=	globalfifo_llseek,
    .poll   =   globalfifo_pol
	.read	=	globalfifo_read,
	.write	=	globalfifo_write,
	.unlocked_ioctl = globalfifo_ioctl,
	.open	=	globalfifo_open,
	.release =	globalfifo_release,
};



/* 完成 cdev 的 初始化 和 添加*/
static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index)
{
	int err, devno = MKDEV(globalfifo_major, index);

	cdev_init(&dev->cdev, &globalfifo_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
		printk(KERN_NOTICE "Error %d adding globalfifo %d", err, index);
}

/* 模块初始化 */
static int __init globalfifo_init(void)
{
	int ret;
	dev_t devno = MKDEV(globalfifo_major, 0);

	if(globalfifo_major)
		ret = register_chrdev_region(devno, 1, "globalfifo");
	else{
		ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo");
		globalfifo_major = MAJOR(devno);
	}
	if(ret < 0 )
		return ret;

	globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);	//kzalloc = kmalloc + memset 
	if(!globalfifo_devp){
		ret = -ENOMEM;
		goto fail_malloc;
	}

	globalfifo_setup_cdev(globalfifo_devp, 0);

	mutex_init(&globalfifo_devp->mutex); //互斥体初始化

    init_waitqueue_head(&globalfifo_devp->r_wait);
    init_waitqueue_head(&globalfifo_devp->w_wait);

	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}

/* */
static void __exit globalfifo_exit(void)
{
	cdev_del(&globalfifo_devp->cdev);
	kfree(globalfifo_devp);
	unregister_chrdev_region(MKDEV(globalfifo_major, 0), 1);
}

module_init(globalfifo_init);
module_exit(globalfifo_exit);
MODULE_AUTHOR("Younix <foreveryounix@gmail.com>");
MODULE_LICENSE("GPL v2");
