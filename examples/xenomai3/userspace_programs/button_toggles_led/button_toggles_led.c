#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <alchemy/task.h>
#include <alchemy/timer.h>

#include <rtdm/gpio.h>
 
#define IN  0
#define OUT 1
 
#define LOW  0
#define HIGH 1
 
#define POUT 22  
#define PIN  23  

RTIME SEC = 1000000000llu;
RTIME MSEC = 1000000llu;
RTIME USEC = 1000llu;

RT_TASK blink_task, hello_task, startTasks;
RT_TASK isr_task;

 
 
#define VALUE_MAX 33
static	int fd_in=0;
static	int fd_out_write=0;


static int GPIOWriteInit(int pin, int value) {
 
	int fd=0;
	char path[VALUE_MAX];
    snprintf(path, VALUE_MAX, "/dev/rtdm/pinctrl-bcm2835/gpio%d", pin);
	rt_printf("open device wronly: %s   ---\n",path );
	fd = open(path, O_WRONLY);
	//fd = open(path, O_RDWR|O_NONBLOCK); // cannot read from filehandle even with O_RDWR
	//fd = open(path, O_RDWR); // cannot read from filehandle even with O_RDWR
	if (-1 == fd) {
        int errsv = errno;
        rt_printf("Failed to failed to gpio pin %d , return %d errno %d\n", pin,fd,errsv);
		return(-1);
	}
    rt_printf("Succes to open pin %d\n", pin);

	rt_printf("set device output mode: %s   ---\n",path );
    int ret =ioctl(fd, GPIO_RTIOC_DIR_OUT, &value);
    int errsv = errno;
    if (ret) {
        rt_printf("Failed to set output mode to pin %d  ret %d errno %d\n", pin,ret,errsv);
        return ret;
    }
    rt_printf("Succes to set output mode to pin %d\n",pin);
    
    return fd;
}


static int GPIOInterruptInit(int pin) {
	int fd=0;
	int do_select = 0;
    //int trigger = GPIO_TRIGGER_LEVEL_LOW;
	int	trigger = GPIO_TRIGGER_EDGE_FALLING;
/*
	} trigger_types[] = {
		{ .name = "edge", .flag = GPIO_TRIGGER_EDGE_RISING },
		{ .name = "edge-rising", .flag = GPIO_TRIGGER_EDGE_RISING },
		{ .name = "edge-falling", .flag = GPIO_TRIGGER_EDGE_FALLING },
		{ .name = "edge-both", .flag = GPIO_TRIGGER_EDGE_FALLING|GPIO_TRIGGER_EDGE_RISING },
		{ .name = "level", .flag = GPIO_TRIGGER_LEVEL_LOW },
		{ .name = "level-low", .flag = GPIO_TRIGGER_LEVEL_LOW },
		{ .name = "level-high", .flag = GPIO_TRIGGER_LEVEL_HIGH },
  */
   
	char path[VALUE_MAX];
    snprintf(path, VALUE_MAX, "/dev/rtdm/pinctrl-bcm2835/gpio%d", pin);
	rt_printf("open device rdwr for interrupt: %s   ---\n",path );
	fd = open(path, O_RDWR);
	if (-1 == fd) {
        int errsv = errno;
        rt_printf("Failed to open  gpio pin %d , return %d errno %d [%s]\n", pin,fd,errsv,symerror(-errsv));
//				rt_printf("failed listening to gpio pin %d [%s]\n", pin, symerror(ret));
		return(-1);
	}
    rt_printf("Succes to open pin %d \n", pin);

	int ret = ioctl(fd, GPIO_RTIOC_IRQEN, &trigger);
    if (ret) {
		//ret = -errno;
		//warning("GPIO_RTIOC_IRQEN failed on %s [%s]", device, symerror(ret));
        int errsv = errno;
        rt_printf("GPIO_RTIOC_IRQEN failed on  gpio pin %d , return %d errno %d\n", pin,fd,errsv);
		return ret;
	}

    return fd;
}

static int GPIOWrite(int fd, int pin, int value) {

    rt_printf("write  value %d to pin %d \n", value, pin);
    int ret = write(fd, &value, sizeof(value));
    int errsv = errno;
    if (sizeof(value) != ret ) {
        rt_printf("Failed to write  value %d to pin %d  ret %d errno %d\n", value, pin,ret,errsv);
		return(-1);
	}
	return(0);
}




void runIsr(void *args) {
    // NOTE: initialization code is within linux : none-realtime  => should be done in advance
    int value;	
    int ret;
    int fd=fd_in;
    int pin=PIN;
    int switch_count=0;
    int count=0;
    RTIME lasttime,now;
    lasttime =rt_timer_read();
	for (;;) {
     
        // wait for interrupt 
       	ret = read(fd, &value, sizeof(value));
        count++;
		if (ret < 0) {
			ret = -errno;
			rt_printf("failed reading from gpio pin %d [%s]\n", pin, symerror(ret));
			break;
		}
     
        // handle interrupt
        now =rt_timer_read();
        if ( now > lasttime + 300000000) {
            // new interrupt (no bounce)  
            lasttime =now;
         
            // on new interrupt we assume switch is triggered and we toggle output 
    		rt_printf("\nINTERRUPT: received irq, GPIO state=%d count=%d\n", value,count);
    		GPIOWrite(fd_out_write,POUT, switch_count % 2);
            switch_count++;
        } else {
            // bounce: ignore  bounce interrupts
   	       // printf("BOUNCE: count=%d, time=%llu\n", count, now);
        } 
	}
}

int main(int argc, char *argv[]) {
	 	
	fd_out_write=GPIOWriteInit(POUT, 1);
	fd_in=GPIOInterruptInit(PIN); 
    rt_task_create(&isr_task, "IsrTask", 0, 40, 0);
	rt_task_start(&isr_task, &runIsr, NULL);
 	getchar();
 	
    close(fd_in);
    close(fd_out_write);
	return(0);
}

