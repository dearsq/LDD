/*************************************************************************
    > File Name: wait_queue.c
    > Author: Younix
    > Mail: foreveryounix@gmail.com 
    > Created Time: 2016年10月18日 星期二 14时13分58秒
    > 作用： 等待队列的实现
 ************************************************************************/

static ssize_t xxx_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	...
	DECLARE_WAITQUEUE(wait, current);	/* 定义等待队列元素 */
	add_wait_queue( &xxx_wait , &wait);	/* 添加元素 wait 到等待队列 xxx_wait */

	/* 等待设备缓冲区可写 */
	do{
		avail = device_writable(...);
		if( avail < 0){
			if(file->f_flags & O_NONBLOCK)	//是非阻塞
			{
				ret = -EAGAIN;
				goto out;
			}
			__set_current_state(TASK_INTERRUPTIBLE); /* 改变进程状态 */
			schedule();							/* 调度其他进程执行 */
			if(signal_pending(current)) 		/* 如果是因为信号唤醒*/
			{
				ret = -ERESTARTSYS;
				goto out;
			}
		}
	} while( avail < 0);

	/* 写设备缓冲区 */
	device_write(...);
out:
	remove_wait_queue(&xxx_wait, &wait);
	set_current_sate(TASK_RUNNING);
	return ret;
}