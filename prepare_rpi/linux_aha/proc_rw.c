#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include<linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define MODNAME "proc_hello"

int len,temp;

char msg[100];

int read_proc(struct file *filp,char *buf,size_t count,loff_t *offp ) 
{
	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	if(count>temp)
	{
		count=temp;
	}
	temp=temp-count;
	if (copy_to_user(buf,msg, count) != 0)
		printk ("%s copy_to_user failed!\n", MODNAME);	
	if (count==0)
		temp=len;

	return count;
}

int write_proc(struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	if (copy_from_user(msg,buf,count) != 0)
		printk ("%s copy_from_user failed!\n", MODNAME);	
	len=count;
	temp=len;
	return count;
}

struct file_operations proc_fops = {
read: read_proc,
write: write_proc
};

void create_new_proc_entry(void) 
{
	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	proc_create("hello",0,NULL,&proc_fops);
/*	msg=kmalloc(GFP_KERNEL,10*sizeof(char));
	if (!msg)
		printk ("%s: proc_entry failed!\n", MODNAME);*/
}


int proc_init (void) {
	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	create_new_proc_entry();
	return 0;
}

void proc_cleanup(void) {
	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	remove_proc_entry("hello",NULL);
}

MODULE_LICENSE("GPL"); 
module_init(proc_init);
module_exit(proc_cleanup);
