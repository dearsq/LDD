/*************************************************************************
    > File Name: poll.c
    > Author: Younix
    > Mail: foreveryounix@gmail.com 
    > Created Time: 2016年10月20日 星期四 15时06分07秒
    > poll() 函数典型模板
 ************************************************************************/

static unsigned int xxx_poll(struct file *filp, poll_table *wait)
{
    unsigned int mask = 0;
    struct xxx_dev *dev = filp->private_data;       //获得设备结构指针

    poll_wait(filp, &dev->r_wait, wait);            //加入读等待队列
    poll_wait(filp, &dev->w_wait, wait);            //加入写等待队列
    
    if(...)                                         //可读
        mask |= POLLIN | POLLRDNORM;                //标识数据可获得（对用户可读）

    if(...)                                         //可写
        mask |= POLLOUT | POLLWRNDORM;              //标识数据可写入

    ...
    return mask;
}
