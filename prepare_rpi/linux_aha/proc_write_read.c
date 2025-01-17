#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#define MAX_PROC_SIZE 100

static char proc_data[MAX_PROC_SIZE];

static struct proc_dir_entry *proc_write_entry;

int read_proc(char *buf,char **start,off_t offset,int count,int *eof,void *data )
{
int len=0;
 len = sprintf(buf,"\n %s\n ",proc_data);

return len;
}

int write_proc(struct file *file,const char *buf,int count,void *data )
{

	if(count > MAX_PROC_SIZE)
		count = MAX_PROC_SIZE;
	if(copy_from_user(proc_data, buf, count))
		return -EFAULT;

	return count;
}

void create_new_proc_entry(void)
{
	proc_write_entry = create_proc_entry("proc_entry",0666,NULL);
	if(!proc_write_entry)
	{
		printk(KERN_INFO "Error creating proc entry");
		return -ENOMEM;
	}
	proc_write_entry->read_proc = read_proc ;
	proc_write_entry->write_proc = write_proc;
	printk(KERN_INFO "proc initialized");

}

int proc_init (void) {
    create_new_proc_entry();
    return 0;
}

void proc_cleanup(void) {
    printk(KERN_INFO " Inside cleanup_module\n");
    remove_proc_entry("proc_entry",NULL);
}
MODULE_LICENSE("GPL");   
module_init(proc_init);
