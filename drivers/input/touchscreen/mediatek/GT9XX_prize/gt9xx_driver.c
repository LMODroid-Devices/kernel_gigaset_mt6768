/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *  
 * MediaTek Inc. (C) 2012. All rights reserved. 
 * 
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 *
 * Version: V2.4
 * Release Date: 2014/11/28
 */

#include "tpd.h"
#include "mtk_boot_common.h"
#define GUP_FW_INFO
#include "tpd_custom_gt9xx.h"

//#include "cust_gpio_usage.h" 
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#ifdef TPD_PROXIMITY
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif

#if GTP_SUPPORT_I2C_DMA
    #include <linux/dma-mapping.h>
#endif

/*add by prize*/
#define GTP_ICS_SLOT_REPORT   0
#if GTP_ICS_SLOT_REPORT
    #include <linux/input/mt.h>
    static s8  touch_id[GTP_MAX_TOUCH] = {-1};
#endif
/*add by prize*/

#include <linux/proc_fs.h>  /*proc*/


//prize-lixuefeng-20150512-start
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../../misc/mediatek/hardware_info/hardware_info.h"

extern struct hardware_info current_tp_info;
#endif
//prize-lixuefeng-20150512-end

extern struct tpd_device *tpd;
#ifdef VELOCITY_CUSTOM
extern int tpd_v_magnify_x;
extern int tpd_v_magnify_y;
#endif
static int tpd_flag = 0; 
int tpd_halt = 0;
static int tpd_eint_mode=1;
static struct task_struct *thread = NULL;
static int tpd_polling_time=50;
extern u8 gtp_loading_fw;

extern unsigned int touch_irq;
u32 gtp_eint_trigger_type = IRQF_TRIGGER_FALLING;
unsigned int touch_irq = 0;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
DEFINE_MUTEX(i2c_access);

#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

#if GTP_HAVE_TOUCH_KEY
const u16 touch_key_array[] = TPD_KEYS;
#define GTP_MAX_KEY_NUM ( sizeof( touch_key_array )/sizeof( touch_key_array[0] ) )

struct touch_vitual_key_map_t {
   int point_x;
   int point_y;
};

static struct touch_vitual_key_map_t touch_key_point_maping_array[] = GTP_KEY_MAP_ARRAY;

#endif

/*add by prize*/
/*d.j add 2015.03.09*/
#if GTP_GESTURE_WAKEUP
#define GT9XX_GESENABLE_PROC_NAME         "gt9xx_enable"
#define GT9XX_GESVAL_PROC_NAME            "gt9xx_gesval"
#endif
/*d.j end 2015.03.09*/
/*add by prize*/

#if GTP_GESTURE_WAKEUP
typedef enum
{
    DOZE_DISABLED = 0,
    DOZE_ENABLED = 1,
    DOZE_WAKEUP = 2,
}DOZE_T;
static DOZE_T doze_status = DOZE_DISABLED;
static s8 gtp_enter_doze(struct i2c_client *client);
#endif

#if GTP_CHARGER_SWITCH
    #ifdef MT6573
        #define CHR_CON0      (0xF7000000+0x2FA00)
    #else
        extern bool upmu_is_chr_det(void);
    #endif
    static void gtp_charger_switch(s32 dir_update);
#endif 

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
//static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

#if GTP_SUPPORT_I2C_DMA
s32 i2c_dma_write(struct i2c_client *client, u16 addr, u8 *txbuf, s32 len);
s32 i2c_dma_read(struct i2c_client *client, u16 addr, u8 *rxbuf, s32 len);

static u8 *gpDMABuf_va = NULL;
static u32 gpDMABuf_pa = 0;
#endif

s32 gtp_send_cfg(struct i2c_client *client);
void gtp_reset_guitar(struct i2c_client *client, s32 ms);
static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
s32 gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr, u8 *rxbuf, int len);
//extern void mt_eint_unmask(unsigned int line);
//extern void mt_eint_mask(unsigned int line);
static void tpd_on(void);
static void tpd_off(void);

#ifndef MT6572
extern void mt65xx_eint_set_hw_debounce(u8 eintno, u32 ms);
extern u32 mt65xx_eint_set_sens(u8 eintno, bool sens);
extern void mt65xx_eint_registration(u8 eintno, bool Dbounce_En,
                                     bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     bool auto_umask);
#endif

#ifdef GTP_CHARGER_DETECT
extern bool upmu_get_pchr_chrdet(void);
#define TPD_CHARGER_CHECK_CIRCLE    50
static struct delayed_work gtp_charger_check_work;
static struct workqueue_struct *gtp_charger_check_workqueue = NULL;
static void gtp_charger_check_func(struct work_struct *);
static u8 gtp_charger_mode = 0;
#endif


#if GTP_CREATE_WR_NODE
extern s32 init_wr_node(struct i2c_client *);
extern void uninit_wr_node(void);
#endif

#if (GTP_ESD_PROTECT || GTP_COMPATIBLE_MODE)
void force_reset_guitar(void);
#endif

#if GTP_ESD_PROTECT
static int clk_tick_cnt = 200;
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue = NULL;
static s32 gtp_init_ext_watchdog(struct i2c_client *client);
static void gtp_esd_check_func(struct work_struct *);
void gtp_esd_switch(struct i2c_client *client, s32 on);
u8 esd_running = 0;
spinlock_t esd_lock;
#endif

#if HOTKNOT_BLOCK_RW
u8 hotknot_paired_flag = 0;
#endif

#ifdef TPD_PROXIMITY
#define TPD_PROXIMITY_VALID_REG                   0x814E
#define TPD_PROXIMITY_ENABLE_REG                  0x8042
static u8 tpd_proximity_flag = 0;
static u8 tpd_proximity_detect = 1;//0-->close ; 1--> far away
#endif

#ifndef GTP_REG_REFRESH_RATE
#define GTP_REG_REFRESH_RATE		0x8056
#endif

struct i2c_client *i2c_client_point = NULL;
static const struct i2c_device_id tpd_i2c_id[] = {{"gt9xx", 0}, {}};
static unsigned short force[] = {0, 0xBA, I2C_CLIENT_END, I2C_CLIENT_END};
static const unsigned short *const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces,};
//static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO("gt9xx", (0xBA >> 1))};
static const struct of_device_id gt9xx_dt_match[] = {
	{.compatible = "mediatek,cap_touch"},
	// {.compatible = "goodix,gt9xx"},
	{},
};
MODULE_DEVICE_TABLE(of, gt9xx_dt_match);

static struct i2c_driver tpd_i2c_driver =
{
	.driver = {
	.name = "gt9xx",
	.of_match_table = of_match_ptr(gt9xx_dt_match),
	//.owner = THIS_MODULE,
	},
    .probe = tpd_i2c_probe,
    .remove = tpd_i2c_remove,
    .detect = tpd_i2c_detect,
    //.driver.name = "gt9xx",
    .id_table = tpd_i2c_id,
    //.address_list = (const unsigned short *) forces,
};

/*add by prize*/
#if GTP_GESTURE_WAKEUP
#define KEY_GESTURE_EVENT         67 //add end by yangsonglin 20150317

/* prize-lifenfen-20150829 */
#define GT9XX_GESVAL_VALUE_DEFAULT			0
/* prize-lifenfen-20150829 */

/*d.j add 2015.03.09*/
static u8 gesenable=1;
static u8 gesenable_user = GT9XX_GESVAL_VALUE_DEFAULT;
static u8 gesval = 0x00;
/* Node: gt9xx_enable --->flag for gesture ON or OFF */
static ssize_t gt91xx_gesenable_read_proc(struct file *, char __user *, size_t, loff_t *);
static ssize_t gt91xx_gesenable_write_proc(struct file *, const char __user *, size_t, loff_t *);

static struct proc_dir_entry *pgt91xx_gesenable_proc = NULL;
static const struct file_operations gesenable_proc_ops = {
    .owner = THIS_MODULE,
    .read = gt91xx_gesenable_read_proc,
    .write = gt91xx_gesenable_write_proc,
};


/* Node: gt9xx_gesture--->save the value for gesture*/
static ssize_t gt91xx_gesval_read_proc(struct file *, char __user *, size_t, loff_t *);
static ssize_t gt91xx_gesval_write_proc(struct file *, const char __user *, size_t, loff_t *);

static struct proc_dir_entry *pgt91xx_gesval_proc = NULL;
static const struct file_operations gesval_proc_ops = {
    .owner = THIS_MODULE,
    .read = gt91xx_gesval_read_proc,
    .write = gt91xx_gesval_write_proc,
};
/*d.j end 2015.03.09*/
#endif
/*add by prize*/

static u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
    = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
#if GTP_CHARGER_SWITCH
static u8 gtp_charger_config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
    = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
#endif
#pragma pack(1)
typedef struct
{
    u16 pid;                 //product id   //
    u16 vid;                 //version id   //
} st_tpd_info;
#pragma pack()

st_tpd_info tpd_info;
u8 int_type = 0;
u32 abs_x_max = 0;
u32 abs_y_max = 0;
u8 gtp_rawdiff_mode = 0;
u8 cfg_len = 0;
u8 pnl_init_error = 0;
u8 gtp_resetting = 0;
static u8 chip_gt9xxs = 0;  // true if chip type is gt9xxs,like gt915s

#if GTP_COMPATIBLE_MODE
u8 driver_num = 0;
u8 sensor_num = 0;
u8 gtp_ref_retries = 0;
u8 gtp_clk_retries = 0;
CHIP_TYPE_T gtp_chip_type = CHIP_TYPE_GT9;
u8 gtp_clk_buf[6];
u8 rqst_processing = 0;

extern u8 gup_check_fs_mounted(char *path_name);
extern u8 gup_clk_calibration(void);
extern s32 gup_load_calibration0(char *filepath);
extern s32 gup_fw_download_proc(void *dir, u8 dwn_mode);
void gtp_get_chip_type(struct i2c_client *client);
u8 gtp_fw_startup(struct i2c_client *client);
static u8 gtp_bak_ref_proc(struct i2c_client *client, u8 mode);
static u8 gtp_main_clk_proc(struct i2c_client *client);
static void gtp_recovery_reset(struct i2c_client *client);
#endif

/* proc file system */
s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len);
s32 i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len);
static struct proc_dir_entry *gt91xx_config_proc = NULL;

#ifdef TPD_REFRESH_RATE
/*******************************************************
Function:
	Write refresh rate

Input:
	rate: refresh rate N (Duration=5+N ms, N=0~15)

Output:
	Executive outcomes.0---succeed.
*******************************************************/
static u8 gtp_set_refresh_rate(u8 rate)
{
	u8 buf[3] = {GTP_REG_REFRESH_RATE>>8, GTP_REG_REFRESH_RATE& 0xff, rate};

	if (rate > 0xf)
	{
		GTP_ERROR("Refresh rate is over range (%d)", rate);
		return FAIL;
	}

	GTP_INFO("Refresh rate change to %d", rate);	
	return gtp_i2c_write(i2c_client_point, buf, sizeof(buf));
}

/*******************************************************
Function:
	Get refresh rate

Output:
	Refresh rate or error code
*******************************************************/
static u8 gtp_get_refresh_rate(void)
{
	int ret;
	
	u8 buf[3] = {GTP_REG_REFRESH_RATE>>8, GTP_REG_REFRESH_RATE& 0xff};
	ret = gtp_i2c_read(i2c_client_point, buf, sizeof(buf));
	if (ret < 0) 
		return ret;

	GTP_INFO("Refresh rate is %d", buf[GTP_ADDR_LENGTH]);	
	return buf[GTP_ADDR_LENGTH];
}

//=============================================================
static ssize_t show_refresh_rate(struct device *dev,struct device_attribute *attr, char *buf)
{
	int ret = gtp_get_refresh_rate(); 
	if (ret < 0)
		return 0;
	else
    	return sprintf(buf, "%d\n", ret);
}
static ssize_t store_refresh_rate(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	//u32 rate = 0;
	gtp_set_refresh_rate(simple_strtoul(buf, NULL, 16));
	return size;
}
static DEVICE_ATTR(tpd_refresh_rate, 0664, show_refresh_rate, store_refresh_rate);

static struct device_attribute *gt9xx_attrs[] =
{
	&dev_attr_tpd_refresh_rate,
};
#endif
//=============================================================


/* proc file system */
s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len);
s32 i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len);

static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    strcpy(info->type, "mtk-tpd");
    return 0;
}

#ifdef TPD_PROXIMITY
static s32 tpd_get_ps_value(void)
{
    return tpd_proximity_detect;
}

static s32 tpd_enable_ps(s32 enable)
{
    u8  state;
    s32 ret = -1;

    if (enable)
    {
        state = 1;
        tpd_proximity_flag = 1;
        GTP_INFO("TPD proximity function to be on.");
    }
    else
    {
        state = 0;
        tpd_proximity_flag = 0;
        GTP_INFO("TPD proximity function to be off.");
    }

    ret = i2c_write_bytes(i2c_client_point, TPD_PROXIMITY_ENABLE_REG, &state, 1);

    if (ret < 0)
    {
        GTP_ERROR("TPD %s proximity cmd failed.", state ? "enable" : "disable");
        return ret;
    }

    GTP_INFO("TPD proximity function %s success.", state ? "enable" : "disable");
    return 0;
}

s32 tpd_ps_operate(void *self, u32 command, void *buff_in, s32 size_in,
                   void *buff_out, s32 size_out, s32 *actualout)
{
    s32 err = 0;
    s32 value;
    hwm_sensor_data *sensor_data;

    switch (command)
    {
        case SENSOR_DELAY:
            if ((buff_in == NULL) || (size_in < sizeof(int)))
            {
                GTP_ERROR("Set delay parameter error!");
                err = -EINVAL;
            }

            // Do nothing
            break;

        case SENSOR_ENABLE:
            if ((buff_in == NULL) || (size_in < sizeof(int)))
            {
                GTP_ERROR("Enable sensor parameter error!");
                err = -EINVAL;
            }
            else
            {
                value = *(int *)buff_in;
                err = tpd_enable_ps(value);
            }

            break;

        case SENSOR_GET_DATA:
            if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data)))
            {
                GTP_ERROR("Get sensor data parameter error!");
                err = -EINVAL;
            }
            else
            {
                sensor_data = (hwm_sensor_data *)buff_out;
                sensor_data->values[0] = tpd_get_ps_value();
                sensor_data->value_divide = 1;
                sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
            }

            break;

        default:
            GTP_ERROR("proxmy sensor operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }

    return err;
}
#endif

/*add by prize, eileen*/
//static int gt91xx_config_read_proc(struct file *file, char *buffer, size_t count, loff_t *ppos)
/*add by prize, eileen*/
static ssize_t gt91xx_config_read_proc(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
    char *page = NULL;
    char *ptr = NULL;
    char temp_data[GTP_CONFIG_MAX_LENGTH + 2] = {0};
    int i, len, err = -1;

	  page = kmalloc(PAGE_SIZE, GFP_KERNEL);	
	  if (!page) 
	  {		
		kfree(page);		
		return -ENOMEM;	
	  }

    ptr = page; 
    ptr += sprintf(ptr, "==== GT9XX config init value====\n");

    for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++)
    {
        ptr += sprintf(ptr, "0x%02X ", config[i + 2]);

        if (i % 8 == 7)
            ptr += sprintf(ptr, "\n");
    }

    ptr += sprintf(ptr, "\n");

    ptr += sprintf(ptr, "==== GT9XX config real value====\n");
    i2c_read_bytes(i2c_client_point, GTP_REG_CONFIG_DATA, temp_data, GTP_CONFIG_MAX_LENGTH);

    for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++)
    {
        ptr += sprintf(ptr, "0x%02X ", temp_data[i]);

        if (i % 8 == 7)
            ptr += sprintf(ptr, "\n");
    }
    /* Touch PID & VID */
    ptr += sprintf(ptr, "\n");
    ptr += sprintf(ptr, "==== GT9XX Version ID ====\n");
    i2c_read_bytes(i2c_client_point, GTP_REG_VERSION, temp_data, 6);
    ptr += sprintf(ptr, "Chip PID: %c%c%c%c  VID: 0x%02X%02X\n", temp_data[0], temp_data[1], temp_data[2],temp_data[3], temp_data[5], temp_data[4]);



    i2c_read_bytes(i2c_client_point, 0x41E4, temp_data, 1);
    ptr += sprintf(ptr, "Boot status 0x%X\n", temp_data[0]);
	
    /* Touch Status and Clock Gate */
    ptr += sprintf(ptr, "\n");
    ptr += sprintf(ptr, "==== Touch Status and Clock Gate ====\n");
    ptr += sprintf(ptr, "status: 1: on, 0 :off\n");
    ptr += sprintf(ptr, "status:%d\n", (tpd_halt+1)&0x1);


	  len = ptr - page; 			 	
	  if(*ppos >= len)
	  {		
		  kfree(page); 		
		  return 0; 	
	  }	
	  err = copy_to_user(buffer,(char *)page,len); 			
	  *ppos += len; 	
	  if(err) 
	  {		
	    kfree(page); 		
		  return err; 	
	  }	
	  kfree(page); 	
	  return len;	

    //return (ptr - page);
}

/*add by prize, eileen*/
//static int gt91xx_config_write_proc(struct file *file, const char *buffer, size_t count, loff_t *ppos)
static ssize_t gt91xx_config_write_proc(struct file *file, const char *buffer, size_t count, loff_t *ppos)
/*add by prize, eileen*/
{
    s32 ret = 0;
    char temp[25] = {0}; // for store special format cmd
    char mode_str[15] = {0};
    unsigned int mode; 
    u8 buf[1];
    
    GTP_ERROR("write count %ld\n", (unsigned long)count);

    if (count > GTP_CONFIG_MAX_LENGTH)
    {
        GTP_ERROR("size not match [%d:%ld]", GTP_CONFIG_MAX_LENGTH, (unsigned long)count);
        return -EFAULT;
    }

    /**********************************************/
    /* for store special format cmd  */
    if (copy_from_user(temp, buffer, sizeof(temp)))
		{
        GTP_ERROR("copy from user fail 2");
        return -EFAULT;
    }
    sscanf(temp, "%s %d", (char *)&mode_str, &mode);
    
    /***********POLLING/EINT MODE switch****************/
    if(strcmp(mode_str, "polling") == 0)
    {
    	if(mode>=10&&mode<=200)
    	{
            GTP_INFO("Switch to polling mode, polling time is %d",mode);
            tpd_eint_mode=0;
            tpd_polling_time=mode;
            tpd_flag = 1;
            wake_up_interruptible(&waiter);
         }
         else
         {
            GTP_INFO("Wrong polling time, please set between 10~200ms");	    
         }
        return count;
    }
    if(strcmp(mode_str, "eint") == 0)
    {
        GTP_INFO("Switch to eint mode");
        tpd_eint_mode=1;
        return count;
    }
    /**********************************************/
    if(strcmp(mode_str, "switch") == 0)
    {
        if(mode == 0)// turn off
            tpd_off();
        else if(mode == 1)//turn on
            tpd_on();
        else
            GTP_ERROR("error mode :%d", mode);  
        return count;              	
    }
    //force clear config
    if(strcmp(mode_str, "clear_config") == 0)
    {
        GTP_INFO("Force clear config");
        buf[0] = 0x10;
        ret = i2c_write_bytes(i2c_client_point, GTP_REG_SLEEP, buf, 1);
        return count;              	
    }
   
    if (copy_from_user(&config[2], buffer, count))
    {
        GTP_ERROR("copy from user fail\n");
        return -EFAULT;
    }
    
    /***********clk operate reseved****************/
    /**********************************************/
    ret = gtp_send_cfg(i2c_client_point);
    abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
    abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
    int_type = (config[TRIGGER_LOC]) & 0x03;

    if (ret < 0)
    {
        GTP_ERROR("send config failed.");
    }

    return count;
}

#if GTP_SUPPORT_I2C_DMA
s32 i2c_dma_read(struct i2c_client *client, u16 addr, u8 *rxbuf, s32 len)
{
    int ret;
    s32 retry = 0;
    u8 buffer[2];

    struct i2c_msg msg[2] =
    {
        {
            .addr = (client->addr & I2C_MASK_FLAG),
            .flags = 0,
            .buf = buffer,
            .len = 2,
            .timing = I2C_MASTER_CLOCK
        },
        {
            .addr = (client->addr & I2C_MASK_FLAG),
            .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
            .flags = I2C_M_RD,
            .buf = (u8*)gpDMABuf_pa,     
            .len = len,
            .timing = I2C_MASTER_CLOCK
        },
    };
    
    buffer[0] = (addr >> 8) & 0xFF;
    buffer[1] = addr & 0xFF;

    if (rxbuf == NULL)
        return -1;

    //GTP_ERROR("dma i2c read: 0x%04X, %d bytes(s)", addr, len);
    for (retry = 0; retry < 5; ++retry)
    {
        ret = i2c_transfer(client->adapter, &msg[0], 2);
        if (ret < 0)
        {
            continue;
        }
        memcpy(rxbuf, gpDMABuf_va, len);
        return 0;
    }
    GTP_ERROR("Dma I2C Read Error: 0x%04X, %d byte(s), err-code: %d", addr, len, ret);
    return ret;
}


s32 i2c_dma_write(struct i2c_client *client, u16 addr, u8 *txbuf, s32 len)
{
    int ret;
    s32 retry = 0;
    u8 *wr_buf = gpDMABuf_va;
    
    struct i2c_msg msg =
    {
        .addr = (client->addr & I2C_MASK_FLAG),
        .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
        .flags = 0,
        .buf = (u8*)gpDMABuf_pa,
        .len = 2 + len,
        .timing = I2C_MASTER_CLOCK
    };
    
    wr_buf[0] = (u8)((addr >> 8) & 0xFF);
    wr_buf[1] = (u8)(addr & 0xFF);

    if (txbuf == NULL)
        return -1;
    
    //GTP_ERROR("dma i2c write: 0x%04X, %d bytes(s)", addr, len);
    memcpy(wr_buf+2, txbuf, len);
    for (retry = 0; retry < 5; ++retry)
    {
        ret = i2c_transfer(client->adapter, &msg, 1);
        if (ret < 0)
        {
            continue;
        }
        return 0;
    }
    GTP_ERROR("Dma I2C Write Error: 0x%04X, %d byte(s), err-code: %d", addr, len, ret);
    return ret;
}

s32 i2c_read_bytes_dma(struct i2c_client *client, u16 addr, u8 *rxbuf, s32 len)
{
    s32 left = len;
    s32 read_len = 0;
    u8 *rd_buf = rxbuf;
    s32 ret = 0;    
    
    //GTP_ERROR("Read bytes dma: 0x%04X, %d byte(s)", addr, len);
    while (left > 0)
    {
        if (left > GTP_DMA_MAX_TRANSACTION_LENGTH)
        {
            read_len = GTP_DMA_MAX_TRANSACTION_LENGTH;
        }
        else
        {
            read_len = left;
        }
        ret = i2c_dma_read(client, addr, rd_buf, read_len);
        if (ret < 0)
        {
            GTP_ERROR("dma read failed");
            return -1;
        }
        
        left -= read_len;
        addr += read_len;
        rd_buf += read_len;
    }
    return 0;
}

s32 i2c_write_bytes_dma(struct i2c_client *client, u16 addr, u8 *txbuf, s32 len)
{

    s32 ret = 0;
    s32 write_len = 0;
    s32 left = len;
    u8 *wr_buf = txbuf;
    
    //GTP_ERROR("Write bytes dma: 0x%04X, %d byte(s)", addr, len);
    while (left > 0)
    {
        if (left > GTP_DMA_MAX_I2C_TRANSFER_SIZE)
        {
            write_len = GTP_DMA_MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            write_len = left;
        }
        ret = i2c_dma_write(client, addr, wr_buf, write_len);
        
        if (ret < 0)
        {
            GTP_ERROR("dma i2c write failed!");
            return -1;
        }
        
        left -= write_len;
        addr += write_len;
        wr_buf += write_len;
    }
    return 0;
}
#endif


int i2c_read_bytes_non_dma(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
    u8 buffer[GTP_ADDR_LENGTH];
    u8 retry;
    u16 left = len;
    u16 offset = 0;

    struct i2c_msg msg[2] =
    {
        {
            //.addr = ((client->addr &I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
            .addr = client->addr,
            .flags = 0,
            .buf = buffer,
            .len = GTP_ADDR_LENGTH,
            //.timing = I2C_MASTER_CLOCK
        },
        {
            //.addr = ((client->addr &I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
            .addr = client->addr,
            .flags = I2C_M_RD,
            //.timing = I2C_MASTER_CLOCK
        },
    };

    if (rxbuf == NULL)
        return -1;

    GTP_ERROR("i2c_read_bytes to device %02X address %04X len %d", client->addr, addr, len);

    while (left > 0)
    {
        buffer[0] = ((addr + offset) >> 8) & 0xFF;
        buffer[1] = (addr + offset) & 0xFF;

        msg[1].buf = &rxbuf[offset];

        if (left > MAX_TRANSACTION_LENGTH)
        {
            msg[1].len = MAX_TRANSACTION_LENGTH;
            left -= MAX_TRANSACTION_LENGTH;
            offset += MAX_TRANSACTION_LENGTH;
        }
        else
        {
            msg[1].len = left;
            left = 0;
        }

        retry = 0;

        while (i2c_transfer(client->adapter, &msg[0], 2) != 2)
        {
            retry++;

            if (retry == 5)
            {
                GTP_ERROR("I2C read 0x%X length=%d failed\n", addr + offset, len);
                return -1;
            }
        }
    }

    return 0;
}


int i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
#if GTP_SUPPORT_I2C_DMA
    return i2c_read_bytes_dma(client, addr, rxbuf, len);
#else
    return i2c_read_bytes_non_dma(client, addr, rxbuf, len);
#endif
}

s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
    s32 ret = -1;
    u16 addr = (buf[0] << 8) + buf[1];

    ret = i2c_read_bytes_non_dma(client, addr, &buf[2], len - 2);

    if (!ret)
    {
        return 2;
    }
    else
    {
    #if GTP_GESTURE_WAKEUP
        if (DOZE_ENABLED == doze_status)
        {
            return ret;
        }
    #endif
    #if GTP_COMPATIBLE_MODE
        if (CHIP_TYPE_GT9F == gtp_chip_type)
        {
            gtp_recovery_reset(client);
        }
        else
    #endif
        {
            gtp_reset_guitar(client, 20);
        }
        return ret;
    }
}


s32 gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
    u8 buf[16] = {0};
    u8 confirm_buf[16] = {0};
    u8 retry = 0;
    
    while (retry++ < 3)
    {
        memset(buf, 0xAA, 16);
        buf[0] = (u8)(addr >> 8);
        buf[1] = (u8)(addr & 0xFF);
        gtp_i2c_read(client, buf, len + 2);
        
        memset(confirm_buf, 0xAB, 16);
        confirm_buf[0] = (u8)(addr >> 8);
        confirm_buf[1] = (u8)(addr & 0xFF);
        gtp_i2c_read(client, confirm_buf, len + 2);
        
        if (!memcmp(buf, confirm_buf, len+2))
        {
            memcpy(rxbuf, confirm_buf+2, len);
            return SUCCESS;
        }
    }    
    GTP_ERROR("i2c read 0x%04X, %d bytes, double check failed!", addr, len);
    return FAIL;
}

int i2c_write_bytes_non_dma(struct i2c_client *client, u16 addr, u8 *txbuf, int len)
{
    u8 buffer[MAX_TRANSACTION_LENGTH];
    u16 left = len;
    u16 offset = 0;
    u8 retry = 0;

    struct i2c_msg msg =
    {
        //.addr = ((client->addr &I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
        .addr = client->addr,
        .flags = 0,
        .buf = buffer,
        //.timing = I2C_MASTER_CLOCK,
    };


    if (txbuf == NULL)
        return -1;

    GTP_ERROR("i2c_write_bytes to device %02X address %04X len %d", client->addr, addr, len);

    while (left > 0)
    {
        retry = 0;

        buffer[0] = ((addr + offset) >> 8) & 0xFF;
        buffer[1] = (addr + offset) & 0xFF;

        if (left > MAX_I2C_TRANSFER_SIZE)
        {
            memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], MAX_I2C_TRANSFER_SIZE);
            msg.len = MAX_TRANSACTION_LENGTH;
            left -= MAX_I2C_TRANSFER_SIZE;
            offset += MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], left);
            msg.len = left + GTP_ADDR_LENGTH;
            left = 0;
        }

        //GTP_ERROR("byte left %d offset %d\n", left, offset);

        while (i2c_transfer(client->adapter, &msg, 1) != 1)
        {
            retry++;

            if (retry == 5)
            {
                GTP_ERROR("I2C write 0x%X%X length=%d failed\n", buffer[0], buffer[1], len);
                return -1;
            }
        }
    }

    return 0;
}

int i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len)
{
#if GTP_SUPPORT_I2C_DMA
    return i2c_write_bytes_dma(client, addr, txbuf, len);
#else
    return i2c_write_bytes_non_dma(client, addr, txbuf, len);
#endif
}

s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
    s32 ret = -1;
    u16 addr = (buf[0] << 8) + buf[1];

    ret = i2c_write_bytes_non_dma(client, addr, &buf[2], len - 2);

    if (!ret)
    {
        return 1;
    }
    else
    {
    #if GTP_GESTURE_WAKEUP
        if (DOZE_ENABLED == doze_status)
        {
            return ret;
        }
    #endif
    #if GTP_COMPATIBLE_MODE
        if (CHIP_TYPE_GT9F == gtp_chip_type)
        {
            gtp_recovery_reset(client);
        }
        else
    #endif
        {
            gtp_reset_guitar(client, 20);
        }
        return ret;
    }
}



/*add by prize*/
#if GTP_GESTURE_WAKEUP
/*d.j add 2015.03.10*/
/* Node: gt9xx_enable --->flag for gesture ON or OFF */
static ssize_t gt91xx_gesenable_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
    char *ptr = page;    
    GTP_ERROR("func:%s,line:%d\n",__func__, __LINE__);      
    if (*ppos)
    {
        return 0;
    }    
    ptr += sprintf(ptr, "0x%02x", gesenable_user);
    ptr += sprintf(ptr, "\n");
    *ppos += ptr - page;
    return (ptr - page);
}

static ssize_t gt91xx_gesenable_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	  int i;
    s32 ret = 0;
    char char_gesen[3]={0};
    unsigned long enable;
    GTP_ERROR("%s,%d write count=%d\n",__func__, __LINE__, (int)count);
    if(count>3)
    {
    		count = 3;
    }
	
    if(copy_from_user(char_gesen, buffer, count))
    {
    	  GTP_ERROR("%s,%d: copy from user fail\n",__func__, __LINE__);
        return -EFAULT;
    }
    
    char_gesen[count-1] = '\0';
    
    for(i=0; i<3; i++)
    {
    	  GTP_ERROR("%s:char_gesen[i]=%x\n", __func__, char_gesen[i]);
    }
    
    ret = kstrtoul(char_gesen, 16, &enable);
    if(ret<0)
    {
			  GTP_ERROR("%s kstrtoul error!!\n",__func__);
			  return -EINVAL;
    }
    GTP_ERROR("print hex data gt9xx_enable=0x%x\n", (u8)enable); 
    gesenable_user = (u8)enable;
	printk("func:%s,line:%d, gesenable_user:%d\n",__func__, __LINE__, gesenable_user);
	  return count; 
}



/* Node: gt9xx_gesture--->save the value for gesture*/
static ssize_t gt91xx_gesval_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	  char *ptr = page;
	  if (*ppos)
    {
        return 0;
    }
    GTP_ERROR("func:%s,line:%d\n",__func__, __LINE__);    
	  ptr += sprintf(ptr, "0x%02x", gesval);
	  ptr += sprintf(ptr, "\n");
	  *ppos += ptr - page;
	  return (ptr-page);
}

static ssize_t gt91xx_gesval_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	  int i;
    s32 ret = 0;
    char char_gesval[3]={0};	
    unsigned long val;
    GTP_ERROR("%s,%d write count=%d\n",__func__, __LINE__, (int)count);
    if(count>3)
    {
    		count = 3;
    }
    
    if(copy_from_user(char_gesval, buffer, count))
    {
    	  GTP_ERROR("%s,%d: copy from user fail\n",__func__, __LINE__);
        return -EFAULT;
    }
    
    char_gesval[count-1] = '\0';
    
    for(i=0; i<3; i++)
    {
    	  GTP_ERROR("%s:char_gesval[i]=%x\n", __func__, char_gesval[i]);
    }
    
    ret = kstrtoul(char_gesval, 16, &val);
    if(ret<0)
    {
			  GTP_ERROR("%s kstrtoul error!!\n",__func__);
			  return -EINVAL;
    }
    
    GTP_ERROR("print hex data val=0x%x\n", (u8)val);    
    gesval = (u8)val;
    GTP_ERROR("print hex data gesval=0x%x\n", gesval);
          
    return count;
}
/*d.j end 2015.03.10*/
#endif
/*add by prize*/

/*******************************************************
Function:
    Send config Function.

Input:
    client: i2c client.

Output:
    Executive outcomes.0--success,non-0--fail.
*******************************************************/
s32 gtp_send_cfg(struct i2c_client *client)
{
    s32 ret = 1;
#if GTP_DRIVER_SEND_CFG
    s32 retry = 0;

    if (pnl_init_error)
    {
        GTP_INFO("Error occurred in init_panel, no config sent!");
        return 0;
    }
    
	GTP_ERROR("Driver Send Config");
    for (retry = 0; retry < 5; retry++)
    {
        ret = gtp_i2c_write(client, config, GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);

        if (ret > 0)
        {
            break;
        }
    }
#endif
    return ret;
}

#if GTP_CHARGER_SWITCH
static int gtp_send_chr_cfg(struct i2c_client *client)
{
	s32 ret = 1;
#if GTP_DRIVER_SEND_CFG
    s32 retry = 0;

	if (pnl_init_error) {
        GTP_INFO("Error occurred in init_panel, no config sent!");
        return 0;
    }
    
    GTP_INFO("Driver Send Config");
    for (retry = 0; retry < 5; retry++) {
        ret = gtp_i2c_write(client, gtp_charger_config, GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
        if (ret > 0) {
            break;
        }
    }
#endif	
	return ret;
}
#endif
/*******************************************************
Function:
    Read goodix touchscreen version function.

Input:
    client: i2c client struct.
    version:address to store version info

Output:
    Executive outcomes.0---succeed.
*******************************************************/
s32 gtp_read_version(struct i2c_client *client, u16 *version)
{
    s32 ret = -1;
    s32 i;
    u8 buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};
    u8 buf_cfg[1] = {0x00};

    // //GTP_ERROR_FUNC();

    ret = gtp_i2c_read(client, buf, sizeof(buf));

    if (ret < 0)
    {
        GTP_ERROR("GTP read version failed");
        return ret;
    }

    if (version)
    {
        *version = (buf[7] << 8) | buf[6];
    }

    tpd_info.vid = *version;
    tpd_info.pid = 0x00;

    for (i = 0; i < 4; i++)
    {
        if (buf[i + 2] < 0x30)break;

        tpd_info.pid |= ((buf[i + 2] - 0x30) << ((3 - i) * 4));
    }
    
    
    
    i2c_read_bytes(client, GTP_REG_VERSION, buf, 6);
    i2c_read_bytes(client, GTP_REG_CONFIG_DATA, buf_cfg, 1);   

    if (buf[5] == 0x00)
    {        
        GTP_INFO("IC VERSION: %c%c%c_%02x%02x",
             buf[2], buf[3], buf[4], buf[7], buf[6]);  
       //prize-lixuefeng-20150512-start
       #if defined(CONFIG_PRIZE_HARDWARE_INFO)
	   sprintf(current_tp_info.chip, "Goodix_%c%c%c\n  fw:0x%02x%02x\n  cfg:0x%02x", buf[0], buf[1], buf[2], buf[5], buf[4],buf_cfg[0]);
       #endif
      //prize-lixuefeng-20150512-end
    }
    else
    {
        if (buf[5] == 'S' || buf[5] == 's')
        {
            chip_gt9xxs = 1;
        }
        GTP_INFO("IC VERSION:%c%c%c%c_%02x%02x",
             buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
        //prize-lixuefeng-20150512-start
       	#if defined(CONFIG_PRIZE_HARDWARE_INFO)
		sprintf(current_tp_info.chip, "Goodix_%c%c%c%c\n  fw:0x%02X%02X\n  cfg:0x%02X", buf[0], buf[1], buf[2],buf[3], buf[5], buf[4],buf_cfg[0]);
		#endif
	//prize-lixuefeng-20150512-end
    }
    
    return ret;
}


/*******************************************************
Function:
    GTP initialize function.

Input:
    client: i2c client private struct.

Output:
    Executive outcomes.0---succeed.
*******************************************************/
static s32 gtp_init_panel(struct i2c_client *client)
{
    s32 ret = 0;

#if GTP_DRIVER_SEND_CFG
    s32 i;
    u8 check_sum = 0;
    u8 opr_buf[16];
    u8 sensor_id = 0;
    u8 retry = 0;
	u8 flash_cfg_version;
	u8 drv_cfg_version;

    u8 cfg_info_group0[] = CTP_CFG_GROUP0;
    u8 cfg_info_group1[] = CTP_CFG_GROUP1;
    u8 cfg_info_group2[] = CTP_CFG_GROUP2;
    u8 cfg_info_group3[] = CTP_CFG_GROUP3;
    u8 cfg_info_group4[] = CTP_CFG_GROUP4;
    u8 cfg_info_group5[] = CTP_CFG_GROUP5;
    u8 *send_cfg_buf[] = {cfg_info_group0, cfg_info_group1, cfg_info_group2,
                        cfg_info_group3, cfg_info_group4, cfg_info_group5};
    u8 cfg_info_len[] = { CFG_GROUP_LEN(cfg_info_group0), 
                          CFG_GROUP_LEN(cfg_info_group1),
                          CFG_GROUP_LEN(cfg_info_group2),
                          CFG_GROUP_LEN(cfg_info_group3), 
                          CFG_GROUP_LEN(cfg_info_group4),
                          CFG_GROUP_LEN(cfg_info_group5)};

#if GTP_CHARGER_SWITCH
		const u8 cfg_grp0_charger[] = GTP_CFG_GROUP0_CHARGER;
		const u8 cfg_grp1_charger[] = GTP_CFG_GROUP1_CHARGER;
		const u8 cfg_grp2_charger[] = GTP_CFG_GROUP2_CHARGER;
		const u8 cfg_grp3_charger[] = GTP_CFG_GROUP3_CHARGER;
		const u8 cfg_grp4_charger[] = GTP_CFG_GROUP4_CHARGER;
		const u8 cfg_grp5_charger[] = GTP_CFG_GROUP5_CHARGER;
		const u8 *cfgs_charger[] = {
			cfg_grp0_charger, cfg_grp1_charger, cfg_grp2_charger,
			cfg_grp3_charger, cfg_grp4_charger, cfg_grp5_charger
		};
		u8 cfg_lens_charger[] = {
							CFG_GROUP_LEN(cfg_grp0_charger),
							CFG_GROUP_LEN(cfg_grp1_charger),
							CFG_GROUP_LEN(cfg_grp2_charger),
							CFG_GROUP_LEN(cfg_grp3_charger),
							CFG_GROUP_LEN(cfg_grp4_charger),
							CFG_GROUP_LEN(cfg_grp5_charger)};
#endif

    GTP_ERROR("Config Groups\' Lengths: %d, %d, %d, %d, %d, %d", 
        cfg_info_len[0], cfg_info_len[1], cfg_info_len[2], cfg_info_len[3],
        cfg_info_len[4], cfg_info_len[5]);

    if ((!cfg_info_len[1]) && (!cfg_info_len[2]) && 
        (!cfg_info_len[3]) && (!cfg_info_len[4]) && 
        (!cfg_info_len[5]))
    {
        sensor_id = 0; 
    }
    else
    {
    #if GTP_COMPATIBLE_MODE
        if (CHIP_TYPE_GT9F == gtp_chip_type)
        {
            msleep(50);
        }
    #endif
        ret = gtp_i2c_read_dbl_check(client, GTP_REG_SENSOR_ID, &sensor_id, 1);
        if (SUCCESS == ret)
        {
        		
    		while((sensor_id == 0xff)&&(retry++ < 3))
			{
				msleep(100);
    		ret = gtp_i2c_read_dbl_check(client, GTP_REG_SENSOR_ID, &sensor_id, 1);
   			GTP_ERROR("GTP sensor_ID read failed time %d.",retry);
    		
			}
			
            if (sensor_id >= 0x06)
            {
                GTP_ERROR("Invalid sensor_id(0x%02X), No Config Sent!", sensor_id);
                pnl_init_error = 1;
                return -1;
            }
        }
        else
        {
            GTP_ERROR("Failed to get sensor_id, No config sent!");
            pnl_init_error = 1;
            return -1;
        }
        GTP_INFO("Sensor_ID: %d", sensor_id);
    }
    
    cfg_len = cfg_info_len[sensor_id];
    
    GTP_INFO("CTP_CONFIG_GROUP%d used, config length: %d", sensor_id, cfg_len);
    
    if (cfg_len < GTP_CONFIG_MIN_LENGTH)
    {
        GTP_ERROR("CTP_CONFIG_GROUP%d is INVALID CONFIG GROUP! NO Config Sent! You need to check you header file CFG_GROUP section!", sensor_id);
        pnl_init_error = 1;
        return -1;
    }
    
#if GTP_COMPATIBLE_MODE
	if (gtp_chip_type != CHIP_TYPE_GT9F)
#endif
	{
	    ret = gtp_i2c_read_dbl_check(client, GTP_REG_CONFIG_DATA, &opr_buf[0], 1);
	    if (ret == SUCCESS)
	    {
	        GTP_ERROR("CFG_CONFIG_GROUP%d Config Version: %d, 0x%02X; IC Config Version: %d, 0x%02X", sensor_id, 
	                    send_cfg_buf[sensor_id][0], send_cfg_buf[sensor_id][0], opr_buf[0], opr_buf[0]);
			
	        flash_cfg_version = opr_buf[0];
			drv_cfg_version = send_cfg_buf[sensor_id][0];       // backup  config version
	        if (flash_cfg_version < 90 && flash_cfg_version > drv_cfg_version) {
	            send_cfg_buf[sensor_id][0] = 0x00;
	        }
	    }
	    else
	    {
	        GTP_ERROR("Failed to get ic config version!No config sent!");
	        return -1;
	    }
	}
    
    memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
    memcpy(&config[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id], cfg_len);

#if GTP_CUSTOM_CFG
    config[RESOLUTION_LOC]     = (u8)GTP_MAX_WIDTH;
    config[RESOLUTION_LOC + 1] = (u8)(GTP_MAX_WIDTH>>8);
    config[RESOLUTION_LOC + 2] = (u8)GTP_MAX_HEIGHT;
    config[RESOLUTION_LOC + 3] = (u8)(GTP_MAX_HEIGHT>>8);
    
    if (GTP_INT_TRIGGER == 0)  //RISING
    {
        config[TRIGGER_LOC] &= 0xfe; 
    }
    else if (GTP_INT_TRIGGER == 1)  //FALLING
    {
        config[TRIGGER_LOC] |= 0x01;
    }
#endif  // GTP_CUSTOM_CFG
    
    check_sum = 0;
    for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
    {
        check_sum += config[i];
    }
    config[cfg_len] = (~check_sum) + 1;

#if GTP_CHARGER_SWITCH
	GTP_ERROR("Charger Config Groups Length: %d, %d, %d, %d, %d, %d", cfg_lens_charger[0],
		  cfg_lens_charger[1], cfg_lens_charger[2], cfg_lens_charger[3], cfg_lens_charger[4], cfg_lens_charger[5]);

	memset(&gtp_charger_config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	if (cfg_lens_charger[sensor_id] == cfg_len) 
		memcpy(&gtp_charger_config[GTP_ADDR_LENGTH], cfgs_charger[sensor_id], cfg_len);

#if GTP_CUSTOM_CFG
	gtp_charger_config[RESOLUTION_LOC] = (u8) GTP_MAX_WIDTH;
	gtp_charger_config[RESOLUTION_LOC + 1] = (u8) (GTP_MAX_WIDTH >> 8);
	gtp_charger_config[RESOLUTION_LOC + 2] = (u8) GTP_MAX_HEIGHT;
	gtp_charger_config[RESOLUTION_LOC + 3] = (u8) (GTP_MAX_HEIGHT >> 8);

	if (GTP_INT_TRIGGER == 0) 	/* RISING  */
		gtp_charger_config[TRIGGER_LOC] &= 0xfe;
	else if (GTP_INT_TRIGGER == 1) /* FALLING */
		gtp_charger_config[TRIGGER_LOC] |= 0x01;
#endif /* END GTP_CUSTOM_CFG */
	if (cfg_lens_charger[sensor_id] != cfg_len) 
		memset(&gtp_charger_config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);	
	
	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
	{
		check_sum += gtp_charger_config[i];
	}
	gtp_charger_config[cfg_len] = (~check_sum) + 1;

#endif /* END GTP_CHARGER_SWITCH */    

#else // DRIVER NOT SEND CONFIG
    cfg_len = GTP_CONFIG_MAX_LENGTH;
    ret = gtp_i2c_read(client, config, cfg_len + GTP_ADDR_LENGTH);
    if (ret < 0)
    {
        GTP_ERROR("Read Config Failed, Using DEFAULT Resolution & INT Trigger!");
        abs_x_max = GTP_MAX_WIDTH;
        abs_y_max = GTP_MAX_HEIGHT;
        int_type = GTP_INT_TRIGGER;
    }
#endif // GTP_DRIVER_SEND_CFG

    //GTP_ERROR_FUNC();
    if ((abs_x_max == 0) && (abs_y_max == 0))
    {
        abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
        abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
        int_type = (config[TRIGGER_LOC]) & 0x03; 
    }
    
#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        u8 have_key = 0;
        if (!memcmp(&gtp_hotknot_calibration_section0[4], "950", 3))
        {
            driver_num = config[GTP_REG_MATRIX_DRVNUM - GTP_REG_CONFIG_DATA + 2];
            sensor_num = config[GTP_REG_MATRIX_SENNUM - GTP_REG_CONFIG_DATA + 2];
        }
        else
        {
            driver_num = (config[CFG_LOC_DRVA_NUM]&0x1F) + (config[CFG_LOC_DRVB_NUM]&0x1F);
            sensor_num = (config[CFG_LOC_SENS_NUM]&0x0F) + ((config[CFG_LOC_SENS_NUM]>>4)&0x0F);
        }
        
        have_key = config[GTP_REG_HAVE_KEY - GTP_REG_CONFIG_DATA + 2] & 0x01;  // have key or not
        if (1 == have_key)
        {
            driver_num--;
        }
        
        GTP_INFO("Driver * Sensor: %d * %d(Key: %d), X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x",
            driver_num, sensor_num, have_key, abs_x_max,abs_y_max,int_type);
    }
    else
#endif
    {
#if GTP_DRIVER_SEND_CFG
        ret = gtp_send_cfg(client);
        if (ret < 0)
        {
            GTP_ERROR("Send config error.");
        }
		
#if GTP_COMPATIBLE_MODE
	if (gtp_chip_type != CHIP_TYPE_GT9F)
#endif
	{
		/* for resume to send config */
		if (flash_cfg_version < 90 && flash_cfg_version > drv_cfg_version) {
			config[GTP_ADDR_LENGTH] = drv_cfg_version;
			check_sum = 0;
			for (i = GTP_ADDR_LENGTH; i < cfg_len; i++) { 
				check_sum += config[i];	        
			}
			config[cfg_len] = (~check_sum) + 1;		
		}
	}
#endif
        GTP_INFO("X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x",
           abs_x_max,abs_y_max,int_type);
    }
     //prize-lixuefeng-20150512-start
    #if defined(CONFIG_PRIZE_HARDWARE_INFO)
    sprintf(current_tp_info.more,"touchpanel"/*,abs_y_max,abs_x_max*/);
    #endif
    //prize-lixuefeng-20150512-end
    
    msleep(10);
    return 0;
}

static s8 gtp_i2c_test(struct i2c_client *client)
{

    u8 retry = 0;
    s8 ret = -1;
    u32 hw_info = 0;

    //GTP_ERROR_FUNC();

    while (retry++ < 5)
    {
        ret = i2c_read_bytes(client, GTP_REG_HW_INFO, (u8 *)&hw_info, sizeof(hw_info));

        if ((!ret) && (hw_info == 0x00900600))              //20121212
        {
            return ret;
        }

        GTP_ERROR("GTP_REG_HW_INFO : %08X", hw_info);
        GTP_ERROR("GTP i2c test failed time %d.", retry);
        msleep(10);
    }

    return -1;
}



/*******************************************************
Function:
    Set INT pin  as input for FW sync.

Note:
  If the INT is high, It means there is pull up resistor attached on the INT pin.
  Pull low the INT pin manaully for FW sync.
*******************************************************/
void gtp_int_sync(s32 ms)
{
    GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
    msleep(ms);
    GTP_GPIO_AS_INT(GTP_INT_PORT);
}

void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
    GTP_INFO("GTP RESET!\n");
    GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);
    msleep(ms);
    GTP_GPIO_OUTPUT(GTP_INT_PORT, client->addr == 0x14);

    msleep(2);
    GTP_GPIO_OUTPUT(GTP_RST_PORT, 1);

    msleep(6);                      //must >= 6ms

#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        return;
    }
#endif

    gtp_int_sync(100);  // for dbl-system
#if GTP_ESD_PROTECT
    gtp_init_ext_watchdog(i2c_client_point);
#endif
}

static int tpd_power_on(struct i2c_client *client)
{
    int ret = 0;
    int reset_count = 0;

//reset_proc:
	GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);
	GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);

    msleep(10);

#ifdef MT6573
    // power on CTP
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
#elif defined(CONFIG_MACH_MT6580)
	ret = regulator_set_voltage(tpd->reg, 2800000, 2800000);	/* set 2.8v */
	if (ret)
		GTP_ERROR("regulator_set_voltage() failed!\n");
	ret = regulator_enable(tpd->reg);	/* enable regulator */
	if (ret)
		GTP_ERROR("regulator_enable() failed!\n");
#else// ( defined(MT6575) || defined(MT6577) || defined(MT6589) )

    #ifdef TPD_POWER_SOURCE_CUSTOM
        // set 2.8v
        if (regulator_set_voltage(tpd->reg, 2800000, 2800000))   
			GTP_ERROR("regulator_set_voltage() failed!");
        //enable regulator
        if (regulator_enable(tpd->reg))
            GTP_ERROR("regulator_enable() failed!");
    #else
        hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
    #endif
    #ifdef TPD_POWER_SOURCE_1800
        hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
    #endif

#endif

    gtp_reset_guitar(client, 20);

#if GTP_COMPATIBLE_MODE
    gtp_get_chip_type(client);
    
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        ret = (int)gup_load_calibration0(NULL);
        if(FAIL == ret)
        {
            GTP_ERROR("[tpd_power_on]Download fw failed.");
            if(reset_count++ < TPD_MAX_RESET_COUNT)
            {
                ;//goto reset_proc;
            }
            else
            {
                return ret;
            }
        }
    }
    else  
#endif
    {
        ret = gtp_i2c_test(client);
    
        if (ret < 0)
        {
            GTP_ERROR("I2C communication ERROR!");
    
            if (reset_count < TPD_MAX_RESET_COUNT)
            {
                reset_count++;
            //    goto reset_proc;
            }
        }
    }
    
    return ret;
}

//**************** For GT9XXF Start ********************//
#if GTP_COMPATIBLE_MODE

void gtp_get_chip_type(struct i2c_client *client)
{
    u8 opr_buf[10] = {0x00};
    s32 ret = 0;
    
    msleep(10);
    
    ret = gtp_i2c_read_dbl_check(client, GTP_REG_CHIP_TYPE, opr_buf, 10);
    
    if (FAIL == ret)
    {
        GTP_ERROR("Failed to get chip-type, set chip type default: GOODIX_GT9");
        gtp_chip_type = CHIP_TYPE_GT9;
        return;
    }
    
    if (!memcmp(opr_buf, "GOODIX_GT9", 10))
    {
		GTP_INFO("Chip Type: %s", (gtp_chip_type == CHIP_TYPE_GT9) ? "GOODIX_GT9" : "GOODIX_GT9F");
		gtp_chip_type = CHIP_TYPE_GT9;
    }
    else // GT9XXF
    {
        gtp_chip_type = CHIP_TYPE_GT9F;
		GTP_INFO("Chip Type: %s", (gtp_chip_type == CHIP_TYPE_GT9) ? "GOODIX_GT9" : "GOODIX_GT9F");
    }
    gtp_chip_type = CHIP_TYPE_GT9; // for test
    GTP_INFO("Chip Type: %s", (gtp_chip_type == CHIP_TYPE_GT9) ? "GOODIX_GT9" : "GOODIX_GT9F");
}

static u8 gtp_bak_ref_proc(struct i2c_client *client, u8 mode)
{
    s32 i = 0;
    s32 j = 0;
    s32 ret = 0;
    struct file *flp = NULL;
    u8 *refp = NULL;
    u32 ref_len = 0;
    u32 ref_seg_len = 0;
    s32 ref_grps = 0;
    s32 ref_chksum = 0;
    u16 tmp = 0;
    
    GTP_ERROR("[gtp_bak_ref_proc]Driver:%d,Sensor:%d.", driver_num, sensor_num);

    //check file-system mounted 
    GTP_ERROR("[gtp_bak_ref_proc]Waiting for FS %d", gtp_ref_retries);        
    if (gup_check_fs_mounted("/data") == FAIL)        
    {               
        GTP_ERROR("[gtp_bak_ref_proc]/data not mounted");
        if(gtp_ref_retries++ < GTP_CHK_FS_MNT_MAX)
        {
            return FAIL;
        }
    }
    else
    {
        GTP_ERROR("[gtp_bak_ref_proc]/data mounted !!!!");
    }
    
    if (!memcmp(&gtp_hotknot_calibration_section0[4], "950", 3))
    {
        ref_seg_len = (driver_num * (sensor_num - 1) + 2) * 2;
        ref_grps = 6;
        ref_len =  ref_seg_len * 6;  // for GT950, backup-reference for six segments
    }
    else
    {
        ref_len = driver_num*(sensor_num-2)*2 + 4;
        ref_seg_len = ref_len;
        ref_grps = 1;
    }
    
    refp = (u8 *)kzalloc(ref_len, GFP_KERNEL);
    if(refp == NULL)
    {
        GTP_ERROR("[gtp_bak_ref_proc]Alloc memory for ref failed.use default ref");
        return FAIL;
    }
    memset(refp, 0, ref_len);
    if(gtp_ref_retries >= GTP_CHK_FS_MNT_MAX)
    {
        for (j = 0; j < ref_grps; ++j)
        {
            refp[ref_seg_len + j * ref_seg_len -1] = 0x01;
        }
        ret = i2c_write_bytes(client, 0x99D0, refp, ref_len);
        if(-1 == ret)
        {
            GTP_ERROR("[gtp_bak_ref_proc]Write ref i2c error.");
            ret = FAIL;
        }

        GTP_ERROR("[gtp_bak_ref_proc]Bak file or path is not exist,send default ref.");
        ret = SUCCESS;
        goto exit_ref_proc;
    }

    //get ref file data
    flp = filp_open(GTP_BAK_REF_PATH, O_RDWR | O_CREAT, 0666);
    if (IS_ERR(flp))
    {
        GTP_ERROR("[gtp_bak_ref_proc]Ref File not found!Creat ref file.");
        //flp->f_op->llseek(flp, 0, SEEK_SET);
        //flp->f_op->write(flp, (char *)refp, ref_len, &flp->f_pos);
        gtp_ref_retries++;
        ret = FAIL;
        goto exit_ref_proc;
    }
    else if(GTP_BAK_REF_SEND == mode)
    {
        flp->f_op->llseek(flp, 0, SEEK_SET);
        ret = flp->f_op->read(flp, (char *)refp, ref_len, &flp->f_pos);
        if(ret < 0)
        {
            GTP_ERROR("[gtp_bak_ref_proc]Read ref file failed.");
            memset(refp, 0, ref_len);
        }
    }

    if(GTP_BAK_REF_STORE == mode)
    {
        ret = i2c_read_bytes(client, 0x99D0, refp, ref_len);
        if(-1 == ret)
        {
            GTP_ERROR("[gtp_bak_ref_proc]Read ref i2c error.");
            ret = FAIL;
            goto exit_ref_proc;
        }
        flp->f_op->llseek(flp, 0, SEEK_SET);
        flp->f_op->write(flp, (char *)refp, ref_len, &flp->f_pos);
    }
    else
    {
        //checksum ref file
        for (j = 0; j < ref_grps; ++j)
        {
            ref_chksum = 0;
            for(i=0; i<ref_seg_len-2; i+=2)
            {
                ref_chksum += ((refp[i + j * ref_seg_len]<<8) + refp[i + 1 + j * ref_seg_len]);
            }
        
            GTP_ERROR("[gtp_bak_ref_proc]Calc ref chksum:0x%04X", ref_chksum&0xFF);
            tmp = ref_chksum + (refp[ref_seg_len + j * ref_seg_len -2]<<8) + refp[ref_seg_len + j * ref_seg_len -1];
            if(1 != tmp)
            {
                GTP_ERROR("[gtp_bak_ref_proc]Ref file chksum error,use default ref");
                memset(&refp[j * ref_seg_len], 0, ref_seg_len);
                refp[ref_seg_len - 1 + j * ref_seg_len] = 0x01;
            }
            else
            {
                if (j == (ref_grps - 1))
                {
                    GTP_ERROR("[gtp_bak_ref_proc]Ref file chksum success.");
                }
            }
          
        }

        ret = i2c_write_bytes(client, 0x99D0, refp, ref_len);
        if(-1 == ret)
        {
            GTP_ERROR("[gtp_bak_ref_proc]Write ref i2c error.");
            ret = FAIL;
            goto exit_ref_proc;
        }
    }
    
    ret = SUCCESS;

exit_ref_proc:
    if(refp)
    {
        kfree(refp);
    }
    if (flp && !IS_ERR(flp))
    {
        filp_close(flp, NULL);
    }
    return ret;
}

u8 gtp_fw_startup(struct i2c_client *client)
{
    u8 wr_buf[4];
    s32 ret = 0;
    
    //init sw WDT
    wr_buf[0] = 0xAA;
    ret = i2c_write_bytes(client, 0x8041, wr_buf, 1);
    if (ret < 0)
    {
        GTP_ERROR("I2C error to firmware startup.");
        return FAIL;
    }
    //release SS51 & DSP
    wr_buf[0] = 0x00;
    i2c_write_bytes(client, 0x4180, wr_buf, 1);
    
    //int sync
    gtp_int_sync(20);
    
    //check fw run status
    i2c_read_bytes(client, 0x8041, wr_buf, 1);
    if(0xAA == wr_buf[0])
    {
        GTP_ERROR("IC works abnormally,startup failed.");
        return FAIL;
    }
    else
    {
        GTP_ERROR("IC works normally,Startup success.");
        wr_buf[0] = 0xAA;
        i2c_write_bytes(client, 0x8041, wr_buf, 1);
        return SUCCESS;
    }
}

static void gtp_recovery_reset(struct i2c_client *client)
{
    mutex_lock(&i2c_access);
    if(tpd_halt == 0)
    {
#if GTP_ESD_PROTECT
    	gtp_esd_switch(client, SWITCH_OFF);
#endif
    	force_reset_guitar();
#if GTP_ESD_PROTECT
    	gtp_esd_switch(client, SWITCH_ON);
#endif
    }
    mutex_unlock(&i2c_access);
}

static u8 gtp_check_clk_legality(void)
{
    u8 i = 0;
    u8 clk_chksum = gtp_clk_buf[5];
    
    for(i = 0; i < 5; i++)
    {
        if((gtp_clk_buf[i] < 50) || (gtp_clk_buf[i] > 120) ||
            (gtp_clk_buf[i] != gtp_clk_buf[0]))
        {
            break;
        }
        clk_chksum += gtp_clk_buf[i];
    }
    
    if((i == 5) && (clk_chksum == 0))
    {
        GTP_INFO("Clk ram legality check success");
        return SUCCESS;
    }
    GTP_ERROR("main clock freq in clock buf is wrong");
    return FAIL;
}

static u8 gtp_main_clk_proc(struct i2c_client *client)
{
    s32 ret = 0;
    u8  i = 0;
    u8  clk_cal_result = 0;
    u8  clk_chksum = 0;
    struct file *flp = NULL;
    
    //check clk legality
    ret = gtp_check_clk_legality();
    if(SUCCESS == ret)
    {
        goto send_main_clk;
    }
    
    GTP_ERROR("[gtp_main_clk_proc]Waiting for FS %d", gtp_ref_retries);        
    if (gup_check_fs_mounted("/data") == FAIL)        
    {            
        GTP_ERROR("[gtp_main_clk_proc]/data not mounted");
        if(gtp_clk_retries++ < GTP_CHK_FS_MNT_MAX)
        {
            return FAIL;
        }
        else
        {
            GTP_ERROR("[gtp_main_clk_proc]Wait for file system timeout,need cal clk");
        }
    }
    else
    {
        GTP_ERROR("[gtp_main_clk_proc]/data mounted !!!!");
        flp = filp_open(GTP_MAIN_CLK_PATH, O_RDWR | O_CREAT, 0666);
        if (!IS_ERR(flp))
        {
            flp->f_op->llseek(flp, 0, SEEK_SET);
            ret = flp->f_op->read(flp, (char *)gtp_clk_buf, 6, &flp->f_pos);
            if(ret > 0)
            {
                ret = gtp_check_clk_legality();
                if(SUCCESS == ret)
                {
                    GTP_ERROR("[gtp_main_clk_proc]Open & read & check clk file success.");
                    goto send_main_clk;
                }
            }
        }
        GTP_ERROR("[gtp_main_clk_proc]Check clk file failed,need cal clk");
    }
    
    //cal clk
#if GTP_ESD_PROTECT
    gtp_esd_switch(client, SWITCH_OFF);
#endif
    clk_cal_result = gup_clk_calibration();
    force_reset_guitar();
    GTP_ERROR("&&&&&&&&&&clk cal result:%d", clk_cal_result);
    
#if GTP_ESD_PROTECT
    gtp_esd_switch(client, SWITCH_ON);
#endif  

    if(clk_cal_result < 50 || clk_cal_result > 120)
    {
        GTP_ERROR("[gtp_main_clk_proc]cal clk result is illegitimate");
        ret = FAIL;
        goto exit_clk_proc;
    }
    
    for(i = 0;i < 5; i++)
    {
        gtp_clk_buf[i] = clk_cal_result;
        clk_chksum += gtp_clk_buf[i];
    }
    gtp_clk_buf[5] = 0 - clk_chksum;
    
    if (IS_ERR(flp))
    {
        flp = filp_open(GTP_MAIN_CLK_PATH, O_RDWR | O_CREAT, 0666);
    }
    else 
    {
        flp->f_op->llseek(flp, 0, SEEK_SET);
        flp->f_op->write(flp, (char *)gtp_clk_buf, 6, &flp->f_pos);
    }
    

send_main_clk:
    
    ret = i2c_write_bytes(client, 0x8020, gtp_clk_buf, 6);
    if(-1 == ret)
    {
        GTP_ERROR("[gtp_main_clk_proc]send main clk i2c error!");
        ret = FAIL;
        goto exit_clk_proc;
    }
    
    ret = SUCCESS;
    
exit_clk_proc:
    if (flp && !IS_ERR(flp))
    {
        filp_close(flp, NULL);
    }
    return ret;
}

#endif
//************* For GT9XXF End **********************//

static const struct file_operations gt_upgrade_proc_fops = { 
    .write = gt91xx_config_write_proc,
    .read = gt91xx_config_read_proc
};

static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	// u32 ints[2] = {0,0};
	tpd_gpio_as_int(1);
	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		// of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		// gpio_set_debounce(ints[0], ints[1]);
		
		touch_irq = irq_of_parse_and_map(node, 0);
		TPD_DMESG("tpd request_irq IRQ ft_touch_irq=%d 1\n",touch_irq);
		ret = request_irq(touch_irq, tpd_eint_interrupt_handler,
					IRQF_TRIGGER_RISING, TPD_DEVICE, NULL);//IRQF_TRIGGER_RISING IRQF_TRIGGER_FALLING
			if (ret > 0)
				TPD_DMESG("tpd request_irq IRQ LINE NOT AVAILABLE!.");
	} else {
		TPD_DMESG("[%s] tpd request_irq can not find touch eint device node!.", __func__);
	}
	return 0;
}

static s32 tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    s32 err = 0;
    s32 ret = 0;

    u16 version_info;
#if GTP_HAVE_TOUCH_KEY
    s32 idx = 0;
#endif
#ifdef TPD_PROXIMITY
    struct hwmsen_object obj_ps;
#endif
printk("cxw tpd_i2c_probe\n");
	GTP_ERROR("Goodix gt9xx tpd_i2c_probe!");
    GTP_GPIO_OUTPUT(GTP_RST_PORT, 1);   
    GTP_GPIO_OUTPUT(GTP_INT_PORT, 1);
	client->addr = 0x5d;//test..
    i2c_client_point = client;
    ret = tpd_power_on(client);

    if (ret < 0)
    {
        GTP_ERROR("I2C communication ERROR!");
      //  return -1;//prize-huangjianlong-20150727
    }
    //prize-lixuefeng-20150512-start
    #if defined(CONFIG_PRIZE_HARDWARE_INFO)
	 sprintf(current_tp_info.id,"0x%04x",client->addr);
    #endif
    //prize-lixuefeng-20150512-end
//#ifdef VELOCITY_CUSTOM
 #if 0

    if ((err = misc_register(&tpd_misc_device)))
    {
        printk("mtk_tpd: tpd_misc_device register failed\n");
    }

#endif
#ifdef VELOCITY_CUSTOM
	tpd_v_magnify_x = TPD_VELOCITY_CUSTOM_X;
	tpd_v_magnify_y = TPD_VELOCITY_CUSTOM_Y;

#endif

    ret = gtp_read_version(client, &version_info);

    if (ret < 0)
    {
        GTP_ERROR("Read version failed.");
		tpd_load_status = 0;
		return ret; //wangyunqing 20151211 TP compatibility 
    }    
    
    ret = gtp_init_panel(client);

    if (ret < 0)
    {
        GTP_ERROR("GTP init panel failed.");
		tpd_load_status = 0;
		return ret;
    }
    
    // Create proc file system
    gt91xx_config_proc = proc_create(GT91XX_CONFIG_PROC_FILE, 0660, NULL, &gt_upgrade_proc_fops);
    if (gt91xx_config_proc == NULL)
    {
        GTP_ERROR("create_proc_entry %s failed\n", GT91XX_CONFIG_PROC_FILE);
    }

/*add by prize*/
    /*d.j add 2015.03.10*/
#if GTP_GESTURE_WAKEUP
    pgt91xx_gesenable_proc = proc_create(GT9XX_GESENABLE_PROC_NAME, 0666, NULL, &gesenable_proc_ops);
    if ( NULL == pgt91xx_gesenable_proc)
    {
        GTP_ERROR("create_proc_entry %s failed\n", GT9XX_GESENABLE_PROC_NAME);
    }
    else
    {
        GTP_ERROR("create proc entry %s success", GT9XX_GESENABLE_PROC_NAME);
    }    

    pgt91xx_gesval_proc = proc_create(GT9XX_GESVAL_PROC_NAME, 0666, NULL, &gesval_proc_ops);
    if ( NULL == pgt91xx_gesval_proc)
    {
        GTP_ERROR("create_proc_entry %s failed\n", GT9XX_GESVAL_PROC_NAME);
    }
    else
    {
        GTP_ERROR("create proc entry %s success", GT9XX_GESVAL_PROC_NAME);
    }
#endif        
/*add by prize*/

#if GTP_CREATE_WR_NODE
    init_wr_node(client);
#endif

    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);

    if (IS_ERR(thread))
    {
        err = PTR_ERR(thread);
        GTP_INFO(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }
    
/*add by prize*/
#if GTP_ICS_SLOT_REPORT
    input_mt_init_slots(tpd->dev, 5, 0);     // in case of "out of memory"
#endif 
/*add by prize*/
    
#if GTP_HAVE_TOUCH_KEY

    for (idx = 0; idx < GTP_MAX_KEY_NUM; idx++)
    {
        input_set_capability(tpd->dev, EV_KEY, touch_key_array[idx]);
    }

#endif
#if GTP_GESTURE_WAKEUP
    input_set_capability(tpd->dev, EV_KEY, KEY_POWER);
	input_set_capability(tpd->dev, EV_KEY, KEY_GESTURE_EVENT); //add end by yangsonglin 20150317
#endif
    
#if GTP_WITH_PEN
    // pen support
    __set_bit(BTN_TOOL_PEN, tpd->dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, tpd->dev->propbit);
    //__set_bit(INPUT_PROP_POINTER, tpd->dev->propbit); // 20130722
#endif

#if 0 //wangyunqing
    // set INT mode
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_DISABLE);

    msleep(50);

#ifndef MT6589
    if (!int_type)	//EINTF_TRIGGER
    {
        mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_RISING, tpd_eint_interrupt_handler, 1);
    }
    else
    {
        mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 1);
    }
    
#else
    mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);

    if (!int_type)
    {
        mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_HIGH, tpd_eint_interrupt_handler, 1);
    }
    else
    {
        mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, tpd_eint_interrupt_handler, 1);
    }
#endif

    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#else
	//gpio_direction_input(tpd_int_gpio_number);

	msleep(50);

	/* EINT device tree, default EINT enable */
	tpd_irq_registration();
#endif

#if GTP_ESD_PROTECT
    gtp_esd_switch(client, SWITCH_ON);
#endif

#if GTP_AUTO_UPDATE
    ret = gup_init_update_proc(client);
    if (ret < 0)
    {
        GTP_ERROR("Create update thread error.");
    }
#endif

#ifdef TPD_PROXIMITY
    //obj_ps.self = cm3623_obj;
    obj_ps.polling = 0;         //0--interrupt mode;1--polling mode;
    obj_ps.sensor_operate = tpd_ps_operate;

    if ((err = hwmsen_attach(ID_PROXIMITY, &obj_ps)))
    {
        GTP_ERROR("hwmsen attach fail, return:%d.", err);
    }
#endif

    tpd_load_status = 1;
    return 0;
}

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
    TPD_DEBUG_PRINT_INT;
    
    tpd_flag = 1;
    
    wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}

static int tpd_i2c_remove(struct i2c_client *client)
{
#if GTP_CREATE_WR_NODE
    uninit_wr_node();
#endif

#if GTP_ESD_PROTECT
    destroy_workqueue(gtp_esd_check_workqueue);
#endif

    return 0;
}
#if (GTP_ESD_PROTECT || GTP_COMPATIBLE_MODE)
void force_reset_guitar(void)
{
    s32 i = 0;
    s32 ret = 0;
	//static u8 gtp_resetting = 0;

	if(gtp_resetting || (gtp_loading_fw == 1))
	{
		return;
	}
    GTP_INFO("force_reset_guitar");
	gtp_resetting = 1;
    /* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	disable_irq(touch_irq);

    GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);   
    GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
#ifdef MT6573
    //Power off TP
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);  
    msleep(30);
    //Power on TP
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
    msleep(30);
#elif defined(CONFIG_MACH_MT6580)
	/* Power off TP */
	ret = regulator_disable(tpd->reg);
	if (ret)
		GTP_ERROR("regulator_disable() failed!\n");
	msleep(30);

	/* Power on TP */
	ret = regulator_set_voltage(tpd->reg,
						2800000, 2800000);
	if (ret)
		GTP_ERROR("regulator_set_voltage() failed!\n");
	ret = regulator_enable(tpd->reg);	/* enable regulator */
	if (ret)
		GTP_ERROR("regulator_enable() failed!\n");
	msleep(30);


#else           // ( defined(MT6575) || defined(MT6577) || defined(MT6589) )
    // Power off TP
    #ifdef TPD_POWER_SOURCE_CUSTOM
        //disable regulator
        if (regulator_disable(tpd->reg)){
			GTP_ERROR("regulator_disable() failed!");
		}
    #else
        hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
    #endif
	#ifdef TPD_POWER_SOURCE_1800
		hwPowerDown(TPD_POWER_SOURCE_1800, "TP");
	#endif
        msleep(30); 

    // Power on TP
    #ifdef TPD_POWER_SOURCE_CUSTOM
        // set 2.8v
        if (regulator_set_voltage(tpd->reg, 2800000, 2800000))   
            GTP_ERROR("regulator_set_voltage() failed!");
        //enable regulator
        if (regulator_enable(tpd->reg))
            GTP_ERROR("regulator_enable() failed!");
    #else
        hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
    #endif
	#ifdef TPD_POWER_SOURCE_1800
		hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
	#endif
        msleep(30);

#endif

    /* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	enable_irq(touch_irq);
    
    for (i = 0; i < 5; i++)
    {
        //Reset Guitar
        gtp_reset_guitar(i2c_client_point, 20);

    #if GTP_COMPATIBLE_MODE
        if (CHIP_TYPE_GT9F == gtp_chip_type)
        {
            //check code ram
            ret = gup_load_calibration0(NULL);
            if(FAIL == ret)
            {
                GTP_ERROR("[force_reset_guitar]Check & repair fw failed.");
                continue;
            }
        }
        else
    #endif
        {
            //Send config
            ret = gtp_send_cfg(i2c_client_point);
    
            if (ret < 0)
            {
                continue;
            }
        }
        break;
    }
	gtp_resetting = 0;
    
}
#endif

#if GTP_ESD_PROTECT
static s32 gtp_init_ext_watchdog(struct i2c_client *client)
{
    u8 opr_buffer[2] = {0xAA};
    GTP_ERROR("Init external watchdog.");
    return i2c_write_bytes(client, 0x8041, opr_buffer, 1);
}

void gtp_esd_switch(struct i2c_client *client, s32 on)
{
    spin_lock(&esd_lock);     
    if (SWITCH_ON == on)     // switch on esd 
    {
        if (!esd_running)
        {
            esd_running = 1;
            spin_unlock(&esd_lock);
            GTP_INFO("Esd started");
            queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, clk_tick_cnt);
        }
        else
        {
            spin_unlock(&esd_lock);
        }
    }
    else    // switch off esd
    {
        if (esd_running)
        {
            esd_running = 0;
            spin_unlock(&esd_lock);
            GTP_INFO("Esd cancelled");
			cancel_delayed_work(&gtp_esd_check_work);
        }
        else
        {
            spin_unlock(&esd_lock);
        }
    }
}


static void gtp_esd_check_func(struct work_struct *work)
{
    s32 i = 0;
    s32 ret = -1;
    
    u8 esd_buf[3] = {0x00};

    if (tpd_halt)
    {
        GTP_INFO("Esd suspended!");
        return;
    }
    if(1 == gtp_loading_fw)
    {
        GTP_INFO("Load FW process is runing");
        return;
    }
    for (i = 0; i < 3; i++)
    {
        ret = i2c_read_bytes_non_dma(i2c_client_point, 0x8040, esd_buf, 2);
        
        GTP_ERROR("[Esd]0x8040 = 0x%02X, 0x8041 = 0x%02X", esd_buf[0], esd_buf[1]);
        if (ret < 0)
        {
            // IIC communication problem
            continue;
        }
        else 
        {
            if ((esd_buf[0] == 0xAA) || (esd_buf[1] != 0xAA))
            {
                u8 chk_buf[2] = {0x00};
                i2c_read_bytes_non_dma(i2c_client_point, 0x8040, chk_buf, 2);
                
                GTP_ERROR("[Check]0x8040 = 0x%02X, 0x8041 = 0x%02X", chk_buf[0], chk_buf[1]);
                
                if ( (chk_buf[0] == 0xAA) || (chk_buf[1] != 0xAA) )
                {
                    i = 3;          // jump to reset guitar
                    break;
                }
                else
                {
                    continue;
                }
            }
            else
            {
                // IC works normally, Write 0x8040 0xAA, feed the watchdog
                esd_buf[0] = 0xAA;
                i2c_write_bytes_non_dma(i2c_client_point, 0x8040, esd_buf, 1);
                
                break;
            }
        }
    }

    if (i >= 3)
    {   
    #if GTP_COMPATIBLE_MODE
        if ((CHIP_TYPE_GT9F == gtp_chip_type) && (1 == rqst_processing))
        {
            GTP_INFO("Request Processing, no reset guitar.");
        }
        else
    #endif
        {
            GTP_INFO("IC works abnormally! Process reset guitar.");
			memset(esd_buf, 0x01, sizeof(esd_buf));
            i2c_write_bytes(i2c_client_point, 0x4226, esd_buf, sizeof(esd_buf));  
            msleep(50);
            force_reset_guitar();
        }
    }

#if FLASHLESS_FLASH_WORKROUND
	{
		u8 versionBuff[6];
		int retry =  0;
		u8 temp = 0;
		
		while(retry++ < 3)
		{
			ret = i2c_read_bytes_non_dma(i2c_client_point, 0x8140, versionBuff, sizeof(versionBuff));
			if(ret < 0)
			{
				continue;
			}
			//match pid
			if( memcmp(versionBuff, &gtp_hotknot_calibration_section0[4], 4) !=0 )
			{
				continue;
			}
			temp = versionBuff[5];
			versionBuff[5] = versionBuff[4];
			versionBuff[4] = temp;
			//match vid
			if( memcmp(&versionBuff[4], &gtp_hotknot_calibration_section0[12], 2) !=0 )
			{
				continue;
			}
			break;
		}
		if(retry>=3)
		{
		    GTP_INFO("IC version error., force reset!");
            force_reset_guitar();
		}
	}
#endif

    if (!tpd_halt)
    {
        queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, clk_tick_cnt);
    }
    else
    {
        GTP_INFO("Esd suspended!");
    }

    return;
}
#endif
static int tpd_history_x=0, tpd_history_y=0;
static void tpd_down(s32 x, s32 y, s32 size, s32 id)
{
/*add by prize*/
#if GTP_ICS_SLOT_REPORT
		input_mt_slot(tpd->dev, id);
    input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, 1);
    input_report_key(tpd->dev,BTN_TOUCH, 1);
    input_report_key(tpd->dev,BTN_TOOL_FINGER, 1);
    
    //input_mt_report_slot_state(mtk_dev, ABS_MT_TRACKING_ID, id);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, (GTP_MAX_WIDTH-x-1));
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, (GTP_MAX_HEIGHT-y-1));
    input_report_abs(tpd->dev,ABS_MT_TOUCH_MAJOR, 20);// for test
    input_report_abs(tpd->dev,ABS_MT_TOUCH_MINOR, 20);// for test 
#else
    if ((!size) && (!id))
    {
        input_report_abs(tpd->dev, ABS_MT_PRESSURE, 100);
        input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 100);
    }
    else
    {
        input_report_abs(tpd->dev, ABS_MT_PRESSURE, size);
        input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, size);
        /* track id Start 0 */
        input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
    }

    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, (GTP_MAX_WIDTH-x-1));
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, (GTP_MAX_HEIGHT-y-1));
    input_mt_sync(tpd->dev);
#endif
/*add by prize*/
    TPD_DEBUG_SET_TIME;
    TPD_EM_PRINT(x, y, x, y, id, 1);
    tpd_history_x=x;
    tpd_history_y=y;

    //MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 1, x+y);
#if 0//def TPD_HAVE_BUTTON

    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
    {
        tpd_button(x, y, 1);
    }

#endif
}

static void tpd_up(s32 x, s32 y, s32 id)
{
/*add by prize*/
#if GTP_ICS_SLOT_REPORT
   input_mt_slot(tpd->dev, id);
   input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, 0);
    //input_report_key(mtk_dev,BTN_TOUCH, 0);
    //input_report_key(mtk_dev,BTN_TOOL_FINGER, 0);   
#else
    //input_report_abs(tpd->dev, ABS_MT_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    input_mt_sync(tpd->dev);
#endif
/*add by prize*/
    TPD_DEBUG_SET_TIME;
    TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y, id, 0);    
    tpd_history_x=0;
    tpd_history_y=0;
    //MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 0, x+y);

#if 0//def TPD_HAVE_BUTTON

    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
    {
        tpd_button(x, y, 0);
    }

#endif
}
#if GTP_CHARGER_SWITCH
static void gtp_charger_switch(s32 dir_update)
{
    u32 chr_status = 0;
    u8 chr_cmd[3] = {0x80, 0x40};
    static u8 chr_pluggedin = 0;
    int ret = 0;
    
#ifdef MT6573
    chr_status = *(volatile u32 *)CHR_CON0;
    chr_status &= (1 << 13);
#else   // ( defined(MT6575) || defined(MT6577) || defined(MT6589) )
    chr_status = upmu_is_chr_det();
#endif
    
    if (chr_status)     // charger plugged in
    {
        if (!chr_pluggedin || dir_update)
        {
            chr_cmd[2] = 6;
            ret = gtp_i2c_write(i2c_client_point, chr_cmd, 3);
            if (ret > 0)
            {
                GTP_INFO("Update status for Charger Plugin");
				if (gtp_send_chr_cfg(i2c_client_point) < 0) {
					GTP_ERROR("Send charger config failed.");
				} else {
					GTP_ERROR("Send charger config.");
				}
            }
            chr_pluggedin = 1;
        }
    }
    else            // charger plugged out
    {
        if (chr_pluggedin || dir_update)
        {
            chr_cmd[2] = 7;
            ret = gtp_i2c_write(i2c_client_point, chr_cmd, 3);
            if (ret > 0)
            {
                GTP_INFO("Update status for Charger Plugout");
				if (gtp_send_cfg(i2c_client_point) < 0) {
					GTP_ERROR("Send normal config failed.");
				} else {
					GTP_ERROR("Send normal config.");
				}
            }
            chr_pluggedin = 0;
        }
    }
}
#endif

static int touch_event_handler(void *unused)
{
//    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
    u8  end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};
    u8  point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF};
    u8  touch_num = 0;
    u8  finger = 0;
    static u8 pre_touch = 0;
    static u8 pre_key = 0;
#if GTP_WITH_PEN
    static u8 pre_pen = 0;
#endif
    u8  key_value = 0;
    u8 *coor_data = NULL;
    s32 input_x = 0;
    s32 input_y = 0;
    s32 input_w = 0;
    s32 id = 0;
    s32 i  = 0;
    s32 ret = -1;
/*add by prize*/
#if GTP_ICS_SLOT_REPORT
    u8  temp = 0;
#endif
/*add by prize*/
    
    
#if GTP_COMPATIBLE_MODE
   u8  rqst_data[3] = {(u8)(GTP_REG_RQST >> 8), (u8)(GTP_REG_RQST & 0xFF), 0};
#endif

#if HOTKNOT_BLOCK_RW
    u8 hn_pxy_state = 0;
    u8 hn_pxy_state_bak = 0;
    u8 hn_paired_cnt = 0;
    u8 hn_state_buf[10] = {(u8)(GTP_REG_HN_STATE>> 8), (u8)(GTP_REG_HN_STATE & 0xFF), 0};
#endif

#ifdef TPD_PROXIMITY
    s32 err = 0;
    hwm_sensor_data sensor_data;
    u8 proximity_status;
#endif

#if GTP_GESTURE_WAKEUP
    u8 doze_buf[3] = {0x81, 0x4B};
#endif

//    sched_setscheduler(current, SCHED_RR, &param);
    do
    {
        set_current_state(TASK_INTERRUPTIBLE);
/*add by prize*/
        while (tpd_halt)
        {
        #if GTP_GESTURE_WAKEUP
            if (DOZE_ENABLED == doze_status)
            {
                break;
            }
        #endif
            tpd_flag = 0;
            msleep(20);
        }
/*add by prize*/

	      if(tpd_eint_mode)
	      {
        	wait_event_interruptible(waiter, tpd_flag != 0);
        	tpd_flag = 0;
	      }
	      else
	      {
		      msleep(tpd_polling_time);
	      }    	
        
        set_current_state(TASK_RUNNING);
    	  mutex_lock(&i2c_access);
    #if GTP_CHARGER_SWITCH
        gtp_charger_switch(0);
    #endif

    #if GTP_GESTURE_WAKEUP
/*add by prize, eileen*/
        if (gesenable)
	{
/*add by prize, eileen*/
        if (DOZE_ENABLED == doze_status)
        {
            ret = gtp_i2c_read(i2c_client_point, doze_buf, 3);
            GTP_ERROR("0x814B = 0x%02X", doze_buf[2]);
            if (ret > 0)
            {     
                if ((doze_buf[2] == 'a') || (doze_buf[2] == 'b') || (doze_buf[2] == 'c') ||
                    (doze_buf[2] == 'd') || (doze_buf[2] == 'e') || (doze_buf[2] == 'g') || 
                    (doze_buf[2] == 'h') || (doze_buf[2] == 'm') || (doze_buf[2] == 'o') ||
                    (doze_buf[2] == 'q') || (doze_buf[2] == 's') || (doze_buf[2] == 'v') || 
                    (doze_buf[2] == 'w') || (doze_buf[2] == 'y') || (doze_buf[2] == 'z') ||
                    (doze_buf[2] == 0x5E) /* ^ */
                    )
                {
                    if (doze_buf[2] != 0x5E)
                    {
                        GTP_INFO("Wakeup by gesture(%c), light up the screen!", doze_buf[2]);
                    }
                    else
                    {
                        GTP_INFO("Wakeup by gesture(^), light up the screen!");
                    }
					/*add by prize*/
                    gesval = doze_buf[2]; /*d.j add 2015.03.09*/
					/*add by prize*/
                    
			 doze_status = DOZE_WAKEUP;
                    //add by yangsonglin 20150317
				input_report_key(tpd->dev, KEY_GESTURE_EVENT, 1);
                    input_sync(tpd->dev);
                    input_report_key(tpd->dev, KEY_GESTURE_EVENT, 0);
                    input_sync(tpd->dev);
				//add end by yangsonglin 20150317
                    // clear 0x814B
                    doze_buf[2] = 0x00;
                    gtp_i2c_write(i2c_client_point, doze_buf, 3);
                }
                else if ( (doze_buf[2] == 0xAA) || (doze_buf[2] == 0xBB) ||
                    (doze_buf[2] == 0xAB) || (doze_buf[2] == 0xBA) )
                {
                    char *direction[4] = {"Right", "Down", "Up", "Left"};
                    u8 type = ((doze_buf[2] & 0x0F) - 0x0A) + (((doze_buf[2] >> 4) & 0x0F) - 0x0A) * 2;
                    
                    GTP_INFO("%s slide to light up the screen!", direction[type]);
				/*add by prize*/
                    gesval = doze_buf[2]; /*d.j add 2015.03.09*/
				/*add by prize*/
                    
			 doze_status = DOZE_WAKEUP;
				//add by yangsonglin 20150317
				input_report_key(tpd->dev, KEY_GESTURE_EVENT, 1);
                    input_sync(tpd->dev);
                    input_report_key(tpd->dev, KEY_GESTURE_EVENT, 0);
                    input_sync(tpd->dev);
				//add end by yangsonglin 20150317
                    // clear 0x814B
                    doze_buf[2] = 0x00;
                    gtp_i2c_write(i2c_client_point, doze_buf, 3);
                }
                else if (0xCC == doze_buf[2])
                {
                    GTP_INFO("Double click to light up the screen!");
				/*add by prize*/
                    gesval = doze_buf[2]; /*d.j add 2015.03.09*/
				/*add by prize*/
                    
			 doze_status = DOZE_WAKEUP;
				//add by yangsonglin 20150317
				input_report_key(tpd->dev, KEY_GESTURE_EVENT, 1);
                    input_sync(tpd->dev);
                    input_report_key(tpd->dev, KEY_GESTURE_EVENT, 0);
                    input_sync(tpd->dev);
				//add end by yangsonglin 20150317
                    // clear 0x814B
                    doze_buf[2] = 0x00;
                    gtp_i2c_write(i2c_client_point, doze_buf, 3);
                }
                else
                {
                    // clear 0x814B
					GTP_INFO("***************************");
                    doze_buf[2] = 0x00;
                    gtp_i2c_write(i2c_client_point, doze_buf, 3);
                    gtp_enter_doze(i2c_client_point);
                }
            }
	    //prize-lixuefeng-20150519-start
	    #ifndef PRIZE_GESTURE_TEST
	    GTP_INFO("***************************");
      	    doze_buf[2] = 0x00;
      	    gtp_i2c_write(i2c_client_point, doze_buf, 3);
      	    gtp_enter_doze(i2c_client_point);
	    #endif
	    //prize-lixuefeng-20150519-end
		mutex_unlock(&i2c_access);
			mutex_unlock(&i2c_access);
            continue;
        }
/*add by prize, eileen*/
        }
/*add by prize, eileen*/
    #endif
    		
        if(tpd_halt||(gtp_resetting == 1) ||(gtp_loading_fw == 1))
        {
            mutex_unlock(&i2c_access);
            GTP_ERROR("return for interrupt after suspend...  ");
            continue;
        }
        ret = gtp_i2c_read(i2c_client_point, point_data, 12);
        if (ret < 0)
        {
            GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
            goto exit_work_func;
        }
        finger = point_data[GTP_ADDR_LENGTH];
        
    #if GTP_COMPATIBLE_MODE
        if ((finger == 0x00) && (CHIP_TYPE_GT9F == gtp_chip_type))
        {
            ret = gtp_i2c_read(i2c_client_point, rqst_data, 3);

            if(ret < 0)
            {
                GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
                goto exit_work_func;
            }
            switch(rqst_data[2]&0x0F)
            {
                case GTP_RQST_BAK_REF:
                    GTP_INFO("Request Ref.");
                    ret = gtp_bak_ref_proc(i2c_client_point, GTP_BAK_REF_SEND);
                    if(SUCCESS == ret)
                    {
                        GTP_INFO("Send ref success.");
                        rqst_data[2] = GTP_RQST_RESPONDED;
                        gtp_i2c_write(i2c_client_point, rqst_data, 3);
                    }
                    goto exit_work_func;
                case GTP_RQST_CONFIG:
                    GTP_INFO("Request Config.");
                    ret = gtp_send_cfg(i2c_client_point);
                    if (ret < 0)
                    {
                        GTP_ERROR("Send config error.");
                    }
                    else 
                    {
                        GTP_INFO("Send config success.");
                        rqst_data[2] = GTP_RQST_RESPONDED;
                        gtp_i2c_write(i2c_client_point, rqst_data, 3);
                    }
                    goto exit_work_func;
                case GTP_RQST_MAIN_CLOCK:
                    GTP_INFO("Request main clock.");
                    rqst_processing = 1;
                    ret = gtp_main_clk_proc(i2c_client_point);
                    if(SUCCESS == ret)
                    {
                        GTP_INFO("Send main clk success.");
                        rqst_data[2] = GTP_RQST_RESPONDED;
                        gtp_i2c_write(i2c_client_point, rqst_data, 3);
                        rqst_processing = 0;
                    }
                    goto exit_work_func;
                case GTP_RQST_RESET:
                    GTP_INFO("Request Reset.");
					mutex_unlock(&i2c_access);
                    gtp_recovery_reset(i2c_client_point);
                    goto exit_work_func;
				case GTP_RQST_HOTKNOT_CODE:
					GTP_INFO("Request HotKnot Code.");
					gup_load_calibration1();
					goto exit_work_func;
                default:
                    break;
            }
        }
    #endif

        if ((finger & 0x80) == 0)
        {
		#if HOTKNOT_BLOCK_RW
/*add by prize, eileen*/
		if (gtp_hotknot_enabled)
/*add by prize, eileen*/
		    if(!hotknot_paired_flag)
		#endif
			{
	            /*mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);*/
				enable_irq(touch_irq);
	            mutex_unlock(&i2c_access);
	            GTP_ERROR("buffer not ready");
	            continue;
			}
        }
	/*goodix add 20150709 for discard nouse frame*/
 
		if (finger  == 0)
		{
 
			goto exit_work_func;
			
 		}

  
    #if HOTKNOT_BLOCK_RW
/*add by prize, eileen*/
	if (gtp_hotknot_enabled)
	{
/*add by prize, eileen*/
        if(!hotknot_paired_flag && (finger&0x0F))
        {
            id = point_data[GTP_ADDR_LENGTH+1];
            hn_pxy_state = point_data[GTP_ADDR_LENGTH+2]&0x80;
            hn_pxy_state_bak = point_data[GTP_ADDR_LENGTH+3]&0x80;
            if((32 == id) && (0x80 == hn_pxy_state) && (0x80 == hn_pxy_state_bak))
            {
                #ifdef HN_DBLCFM_PAIRED
                    if(hn_paired_cnt++ < 2)
                    {
                        goto exit_work_func;
                    }
                #endif
        		GTP_ERROR("HotKnot paired!");
                if(wait_hotknot_state & HN_DEVICE_PAIRED)
                {
                    GTP_ERROR("INT wakeup HN_DEVICE_PAIRED block polling waiter");
        		    got_hotknot_state |= HN_DEVICE_PAIRED;
        		    wake_up_interruptible(&bp_waiter);
                }
                hotknot_paired_flag = 1;
        		goto exit_work_func;
            }
            else
            {
                got_hotknot_state &= (~HN_DEVICE_PAIRED);
                hn_paired_cnt = 0;
            }
            
        }
        
        if(hotknot_paired_flag)
        {
            ret = gtp_i2c_read(i2c_client_point, hn_state_buf, 6);

            if(ret < 0)
            {
                GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
                goto exit_work_func;
            }

            got_hotknot_state = 0;
            
            GTP_ERROR("[0xAB10~0xAB13]=0x%x,0x%x,0x%x,0x%x", hn_state_buf[GTP_ADDR_LENGTH],
                                                             hn_state_buf[GTP_ADDR_LENGTH+1],
                                                             hn_state_buf[GTP_ADDR_LENGTH+2],
                                                             hn_state_buf[GTP_ADDR_LENGTH+3]);

            if(wait_hotknot_state & HN_MASTER_SEND)
            {
                if((0x03 == hn_state_buf[GTP_ADDR_LENGTH]) ||
                   (0x04 == hn_state_buf[GTP_ADDR_LENGTH]) ||
                   (0x07 == hn_state_buf[GTP_ADDR_LENGTH]))
                {
                    GTP_ERROR("Wakeup HN_MASTER_SEND block polling waiter");
                    got_hotknot_state |= HN_MASTER_SEND;
                    got_hotknot_extra_state = hn_state_buf[GTP_ADDR_LENGTH];
    		        wake_up_interruptible(&bp_waiter);
                }
            }
            else if(wait_hotknot_state & HN_SLAVE_RECEIVED)
            {
                if((0x03 == hn_state_buf[GTP_ADDR_LENGTH+1]) ||
                   (0x04 == hn_state_buf[GTP_ADDR_LENGTH+1]) ||
                   (0x07 == hn_state_buf[GTP_ADDR_LENGTH+1]))
                {
                    GTP_ERROR("Wakeup HN_SLAVE_RECEIVED block polling waiter:0x%x", hn_state_buf[GTP_ADDR_LENGTH+1]);
                    got_hotknot_state |= HN_SLAVE_RECEIVED;
                    got_hotknot_extra_state = hn_state_buf[GTP_ADDR_LENGTH+1];
    		        wake_up_interruptible(&bp_waiter);
                }
            }
            else if(wait_hotknot_state & HN_MASTER_DEPARTED)
            {
                if(0x07 == hn_state_buf[GTP_ADDR_LENGTH])
                {
                    GTP_ERROR("Wakeup HN_MASTER_DEPARTED block polling waiter");
                    got_hotknot_state |= HN_MASTER_DEPARTED;
    		        wake_up_interruptible(&bp_waiter);
                }
            }
            else if(wait_hotknot_state & HN_SLAVE_DEPARTED)
            {
                if(0x07 == hn_state_buf[GTP_ADDR_LENGTH+1])
                {
                    GTP_ERROR("Wakeup HN_SLAVE_DEPARTED block polling waiter");
                    got_hotknot_state |= HN_SLAVE_DEPARTED;
    		        wake_up_interruptible(&bp_waiter);
                }
            }
        }
/*add by prize, eileen*/
	}
/*add by prize, eileen*/
    #endif	
        
    #ifdef TPD_PROXIMITY
        if (tpd_proximity_flag == 1)
        {
            proximity_status = point_data[GTP_ADDR_LENGTH];
            GTP_ERROR("REG INDEX[0x814E]:0x%02X\n", proximity_status);

            if (proximity_status & 0x60)                //proximity or large touch detect,enable hwm_sensor.
            {
                tpd_proximity_detect = 0;
                //sensor_data.values[0] = 0;
            }
            else
            {
                tpd_proximity_detect = 1;
                //sensor_data.values[0] = 1;
            }

            //get raw data
            GTP_ERROR(" ps change\n");
            GTP_ERROR("PROXIMITY STATUS:0x%02X\n", tpd_proximity_detect);
            //map and store data to hwm_sensor_data
            sensor_data.values[0] = tpd_get_ps_value();
            sensor_data.value_divide = 1;
            sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
            //report to the up-layer
            ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);

            if (ret)
            {
                GTP_ERROR("Call hwmsen_get_interrupt_data fail = %d\n", err);
            }
        }

    #endif

        touch_num = finger & 0x0f;

        if (touch_num > GTP_MAX_TOUCH)
        {
            GTP_ERROR("Bad number of fingers!");
            goto exit_work_func;
        }

        if (touch_num > 1)
        {
            u8 buf[8 * GTP_MAX_TOUCH] = {(GTP_READ_COOR_ADDR + 10) >> 8, (GTP_READ_COOR_ADDR + 10) & 0xff};

            ret = gtp_i2c_read(i2c_client_point, buf, 2 + 8 * (touch_num - 1));
            memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
        }

#if GTP_HAVE_TOUCH_KEY
        key_value = point_data[3 + 8 * touch_num];

        if (key_value || pre_key)
        {
            for (i = 0; i < TPD_KEY_COUNT; i++)
            {
             //   input_report_key(tpd->dev, touch_key_array[i], key_value & (0x01 << i));
				
			if( key_value&(0x01<<i) ) //key=1 menu ;key=2 home; key =4 back;
			{
				input_x =touch_key_point_maping_array[i].point_x;
				input_y = touch_key_point_maping_array[i].point_y;
				GTP_ERROR("button =%d %d",input_x,input_y);
					   
				tpd_down( input_x, input_y, 0, 0);
			}
			
            }
			
		    if((pre_key!=0)&&(key_value ==0))
		    {
		    	tpd_up( 0, 0, 0);
		    }

            touch_num = 0;
            pre_touch = 0;
        }

#endif
        pre_key = key_value;
        GTP_ERROR("pre_touch:%02x, finger:%02x.", pre_touch, finger);
/*add by prize*/
#if GTP_ICS_SLOT_REPORT
        if (touch_num)
        {
	     //tpd->tpd_state =1;
	     if(pre_touch > touch_num)
	     { 	
	     		temp = pre_touch;
	     }
	     else
	     { 	
			temp = touch_num;
	     }
            for (i = 0; i < temp; i++)
            {
            	  id = -1;
            	  if(i < touch_num)
            	  {
                coor_data = &point_data[i * 8 + 3];

                id = coor_data[0] & 0x0F;      
                input_x  = coor_data[1] | coor_data[2] << 8;
                input_y  = coor_data[3] | coor_data[4] << 8;
                input_w  = coor_data[5] | coor_data[6] << 8;

                input_x = TPD_WARP_X(abs_x_max, input_x);
                input_y = TPD_WARP_Y(abs_y_max, input_y);

            #if GTP_WITH_PEN
                id = coor_data[0];
                if ((id & 0x80))      // pen/stylus is activated
                {
                    GTP_ERROR("Pen touch DOWN!");
                    input_report_key(tpd->dev, BTN_TOOL_PEN, 1);
                    pre_pen = 1;
                    id = 0;
                }
            #endif
                {
					input_x = 1280- input_x;
					input_y = 720 -input_y;
					GTP_ERROR(" %d)(%d, %d)[%d]", id, input_x, input_y, input_w);
					tpd_down(input_x, input_y, input_w, id);
                }
           	}
		if((touch_id[i] != id)&&(pre_touch !=0))
                    tpd_up(0, 0, touch_id[i]);
	        touch_id[i] = id;
            }
        }
        else
        {
            if (pre_touch)
        {
            #if GTP_WITH_PEN
                if (pre_pen)
                {   
                    GTP_ERROR("Pen touch UP!");
                    input_report_key(tpd->dev, BTN_TOOL_PEN, 0);
                    pre_pen = 0;
                }
                else
            #endif
                {
                	for (i = 0; i < GTP_MAX_TOUCH; i++)
            		{
            			tpd_up(0, 0, i);
            			touch_id[i]  = -1;
                	}
                    GTP_ERROR("Touch Release!");
                    //tpd_up(0, 0, 0);
			//yz add
		      //tpd->tpd_state =0;
		     //end			
                }
            }
         }
        pre_touch = touch_num;
#else
        if (touch_num)
        {
            for (i = 0; i < touch_num; i++)
            {
                coor_data = &point_data[i * 8 + 3];
				if(coor_data[0] == 32)
				{
					goto exit_work_func;	
				}
                id = coor_data[0] & 0x0F;      
                input_x  = coor_data[1] | coor_data[2] << 8;
                input_y  = coor_data[3] | coor_data[4] << 8;
                input_w  = coor_data[5] | coor_data[6] << 8;

                input_x = TPD_WARP_X(abs_x_max, input_x);
                input_y = TPD_WARP_Y(abs_y_max, input_y);

            #if GTP_WITH_PEN
                id = coor_data[0];
                if ((id & 0x80))      // pen/stylus is activated
                {
                    GTP_ERROR("Pen touch DOWN!");
                    input_report_key(tpd->dev, BTN_TOOL_PEN, 1);
                    pre_pen = 1;
                    id = 0;
                }
            #endif
				input_x = 720  - input_x;
				input_y = 1280 - input_y;
                GTP_ERROR("liaojie %d)(%d, %d)[%d]", id, input_x, input_y, input_w);

                tpd_down(input_x, input_y, input_w, id);
            }
        }
        else if (pre_touch)
        {
            #if GTP_WITH_PEN
                if (pre_pen)
                {   
                    GTP_ERROR("Pen touch UP!");
                    input_report_key(tpd->dev, BTN_TOOL_PEN, 0);
                    pre_pen = 0;
                }
            #endif
                GTP_ERROR("Touch Release!");
                tpd_up(0, 0, 0);
         }
	     else
	     {
	       GTP_ERROR("Additional Eint!");
	      }
        pre_touch = touch_num;
#endif
/*add by prize*/
 
        if (tpd != NULL && tpd->dev != NULL)
        {
            input_sync(tpd->dev);
        }

exit_work_func:

        if (!gtp_rawdiff_mode)
        {
            ret = gtp_i2c_write(i2c_client_point, end_cmd, 3);

            if (ret < 0)
            {
                GTP_INFO("I2C write end_cmd  error!");
            }
        }
	    /*mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);*/
		enable_irq(touch_irq);
	    if (mutex_is_locked(&i2c_access))
    			mutex_unlock(&i2c_access);

    } while (!kthread_should_stop());

    return 0;
}

static int tpd_local_init(void)
{
	TPD_DMESG("Goodix Gt9xx I2C Touchscreen Driver...\n");
	printk("cxw tpd_local_init....\n");
	
	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	if (IS_ERR(tpd->reg))
		GTP_ERROR("regulator_get() failed!\n");

#if GTP_ESD_PROTECT
    clk_tick_cnt = 2 * HZ;   // HZ: clock ticks in 1 second generated by system
    GTP_ERROR("Clock ticks for an esd cycle: %d", clk_tick_cnt);
    INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
    gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
    spin_lock_init(&esd_lock);          // 2.6.39 & later
    // esd_lock = SPIN_LOCK_UNLOCKED;   // 2.6.39 & before
#endif

#if GTP_SUPPORT_I2C_DMA
    gpDMABuf_va = (u8 *)dma_alloc_coherent(NULL, GTP_DMA_MAX_TRANSACTION_LENGTH, &gpDMABuf_pa, GFP_KERNEL);
    if(!gpDMABuf_va){
        GTP_INFO("[Error] Allocate DMA I2C Buffer failed!\n");
    }
    memset(gpDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);
#endif
	printk("cxw tpd_local_init i2c_add_driver\n");
    if (i2c_add_driver(&tpd_i2c_driver) != 0)
    {
        GTP_INFO("unable to add i2c driver.\n");
        return -1;
    }

    if (tpd_load_status == 0) //if(tpd_load_status == 0) // disable auto load touch driver for linux3.0 porting
    {
        GTP_INFO("add error touch panel driver.\n");
        i2c_del_driver(&tpd_i2c_driver);
        return -1;
    }
    input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (GTP_MAX_TOUCH-1), 0, 0);
#ifdef TPD_HAVE_BUTTON
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
#endif

    // set vendor string
    tpd->dev->id.vendor = 0x00;
    tpd->dev->id.product = tpd_info.pid;
    tpd->dev->id.version = tpd_info.vid;

    GTP_INFO("end %s, %d\n", __FUNCTION__, __LINE__);
    tpd_type_cap = 1;

    return 0;
}

#if GTP_GESTURE_WAKEUP
static s8 gtp_enter_doze(struct i2c_client *client)
{
    s8 ret = -1;
    s8 retry = 0;
    u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8), (u8)GTP_REG_SLEEP, 8};

    //GTP_ERROR_FUNC();


    GTP_ERROR("entering doze mode...");
    while(retry++ < 5)
    {
        i2c_control_buf[0] = 0x80;
        i2c_control_buf[1] = 0x46;
        ret = gtp_i2c_write(client, i2c_control_buf, 3);
        if (ret < 0)
        {
            GTP_ERROR("failed to set doze flag into 0x8046, %d", retry);
            continue;
        }
        i2c_control_buf[0] = 0x80;
        i2c_control_buf[1] = 0x40;
        ret = gtp_i2c_write(client, i2c_control_buf, 3);
        if (ret > 0)
        {
            doze_status = DOZE_ENABLED;
            GTP_ERROR("GTP has been working in doze mode!");
            return ret;
        }
        msleep(10);
    }
    GTP_ERROR("GTP send doze cmd failed.");
    return ret;
}

/*add by prize, eileen*/
//#else
#endif
/*add by prize, eileen*/
/*******************************************************
Function:
    Eter sleep function.

Input:
    client:i2c_client.

Output:
    Executive outcomes.0--success,non-0--fail.
*******************************************************/
static s8 gtp_enter_sleep(struct i2c_client *client)
{
#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        u8 i2c_status_buf[3] = {0x80, 0x44, 0x00};
        s32 ret = 0;
      
        ret = gtp_i2c_read(client, i2c_status_buf, 3);
        if(ret <= 0)
        {
             GTP_ERROR("[gtp_enter_sleep]Read ref status reg error.");
        }
        
        if (i2c_status_buf[2] & 0x80)
        {
            //Store bak ref
            ret = gtp_bak_ref_proc(client, GTP_BAK_REF_STORE);
            if(FAIL == ret)
            {
                GTP_ERROR("[gtp_enter_sleep]Store bak ref failed.");
            }        
        }
    }
#endif

#if GTP_POWER_CTRL_SLEEP

    GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);   
    GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
    msleep(10);

#ifdef MT6573
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);  
    msleep(30);
#elif defined(CONFIG_MACH_MT6580)
	if(regulator_disable(tpd->reg))	/* disable regulator */
		GTP_ERROR("regulator_disable() failed!\n");
#else               // ( defined(MT6575) || defined(MT6577) || defined(MT6589) )

    #ifdef TPD_POWER_SOURCE_1800
        hwPowerDown(TPD_POWER_SOURCE_1800, "TP");
    #endif
    
    #ifdef TPD_POWER_SOURCE_CUSTOM
        hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
    #else
        hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
    #endif
#endif

    GTP_INFO("GTP enter sleep by poweroff!");
    return 0;
    
#else
    {
        s8 ret = -1;
        s8 retry = 0;
        u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8), (u8)GTP_REG_SLEEP, 5};
        
        
        GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
        msleep(5);
    
        while (retry++ < 5)
        {
            ret = gtp_i2c_write(client, i2c_control_buf, 3);
    
            if (ret > 0)
            {
                GTP_INFO("GTP enter sleep!");
                    
                return ret;
            }
    
            msleep(10);
        }
    
        GTP_ERROR("GTP send sleep cmd failed.");
        return ret;
    }
#endif
}
/*add by prize, eileen*/
//#endif
/*add by prize, eileen*/

/*******************************************************
Function:
    Wakeup from sleep mode Function.

Input:
    client:i2c_client.

Output:
    Executive outcomes.0--success,non-0--fail.
*******************************************************/
static s8 gtp_wakeup_sleep(struct i2c_client *client)
{
    u8 retry = 0;
    s8 ret = -1;

    GTP_ERROR("GTP wakeup begin.");

#if (GTP_POWER_CTRL_SLEEP)   

#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        force_reset_guitar();
        GTP_INFO("Esd recovery wakeup.");
        return 0;
    }
#endif

    while (retry++ < 5)
    {
        ret = tpd_power_on(client);

        if (ret < 0)
        {
            GTP_ERROR("I2C Power on ERROR!");
            continue;
        }
        GTP_INFO("Ic wakeup by poweron");
        return 0;
    }
#else

#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        u8 opr_buf[2] = {0};
        
        while (retry++ < 10)
        {
            GTP_GPIO_OUTPUT(GTP_INT_PORT, 1);
            msleep(5);
            
            ret = gtp_i2c_test(client);
    
            if (ret >= 0)
            {  
                // Hold ss51 & dsp
                opr_buf[0] = 0x0C;
                ret = i2c_write_bytes(client, 0x4180, opr_buf, 1);
                if (ret < 0)
                {
                    GTP_ERROR("Hold ss51 & dsp I2C error,retry:%d", retry);
                    continue;
                }
                
                // Confirm hold
                opr_buf[0] = 0x00;
                ret = i2c_read_bytes(client, 0x4180, opr_buf, 1);
                if (ret < 0)
                {
                    GTP_ERROR("confirm ss51 & dsp hold, I2C error,retry:%d", retry);
                    continue;
                }
                if (0x0C != opr_buf[0])
                {
                    GTP_ERROR("ss51 & dsp not hold, val: %d, retry: %d", opr_buf[0], retry);
                    continue;
                }
                GTP_ERROR("ss51 & dsp has been hold");
                
                ret = gtp_fw_startup(client);
                if (FAIL == ret)
                {
                    GTP_ERROR("[gtp_wakeup_sleep]Startup fw failed.");
                    continue;
                }
                GTP_INFO("flashless wakeup sleep success");
                return ret;
            }
            force_reset_guitar();   
            retry = 0;
            break;
        }
        if (retry >= 10)
        {
            GTP_ERROR("wakeup retry timeout, process esd reset");
            force_reset_guitar();
        }
        GTP_ERROR("GTP wakeup sleep failed.");
        return ret;
    }
#endif
    while (retry++ < 10)
    {
    #if GTP_GESTURE_WAKEUP
/*add by prize, eileen*/
	if (gesenable)
	{
/*add by prize, eileen*/
        if (DOZE_WAKEUP != doze_status)
        {
            GTP_INFO("Powerkey wakeup.");
        }
        else
        {
            GTP_INFO("Gesture wakeup.");
        }
        doze_status = DOZE_DISABLED;
        
		/* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
		disable_irq(touch_irq);
        gtp_reset_guitar(client, 20);
		/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
		enable_irq(touch_irq);

	}
    //#else
	else
#endif
	{
/*add by prize, eileen*/
        GTP_GPIO_OUTPUT(GTP_INT_PORT, 1);
        msleep(5);
/*add by prize, eileen*/
	}
    //#endif
/*add by prize, eileen*/
        
        ret = gtp_i2c_test(client);

        if (ret >= 0)
        {
            GTP_INFO("GTP wakeup sleep.");
/*add by prize, eileen*/
        //#if (!GTP_GESTURE_WAKEUP)
        #if GTP_GESTURE_WAKEUP
        if (!gesenable)
        #endif
/*add by prize, eileen*/
            {
                gtp_int_sync(25);
            #if GTP_ESD_PROTECT
                gtp_init_ext_watchdog(client);
            #endif
            }
/*add by prize, eileen*/
        //#endif
/*add by prize, eileen*/
            
            return ret;
        }
        gtp_reset_guitar(client, 20);
    }
#endif
    GTP_ERROR("GTP wakeup sleep failed.");
    return ret;
}

/* Function to manage low power suspend */
static void tpd_suspend(struct device *h)
{
    s32 ret = -1;
	u8 buf[3] = {0x81, 0xaa, 0};
    GTP_INFO("System suspend.");
#ifdef TPD_PROXIMITY
    if (tpd_proximity_flag == 1)
        return ;
#endif

	if (gtp_hotknot_enabled) {
#if HOTKNOT_BLOCK_RW	
	    if(hotknot_paired_flag)
	        return;
#endif
	    mutex_lock(&i2c_access);	
	    gtp_i2c_read(i2c_client_point,buf,sizeof(buf));
		mutex_unlock(&i2c_access);
		if(buf[2] == 0x55) {
			GTP_INFO("GTP early suspend  pair sucess");
			return;
		}
	}
	
	tpd_halt = 1; 
/*add by prize*/
    //tpd->tpd_state =0;
/*add by prize*/
#if GTP_ESD_PROTECT
    gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif

#ifdef GTP_CHARGER_DETECT
    cancel_delayed_work_sync(&gtp_charger_check_work);
#endif

    mutex_lock(&i2c_access);
#if GTP_GESTURE_WAKEUP
gesenable = gesenable_user;
printk("func:%s,line:%d, gesenable:%d\n",__func__, __LINE__, gesenable);
/*add by prize, eileen*/
if (gesenable)
/*add by prize, eileen*/
    ret = gtp_enter_doze(i2c_client_point);
/*add by prize, eileen*/
//#else
else
#endif
{
/*add by prize, eileen*/
    /* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	disable_irq(touch_irq);
    ret = gtp_enter_sleep(i2c_client_point);
/*add by prize, eileen*/
}
/*add by prize, eileen*/
    if (ret < 0)
    {
        GTP_ERROR("GTP early suspend failed.");
    }	
/*add by prize, eileen*/
//#endif
/*add by prize, eileen*/
    mutex_unlock(&i2c_access);
    msleep(58);
}

/* Function to manage power-on resume */
static void tpd_resume(struct device *h)
{
    s32 ret = -1;

/*add by prize*/
    //tpd->tpd_state =0;
/*add by prize*/
    GTP_INFO("System resume.");
    
#ifdef TPD_PROXIMITY
    if (tpd_proximity_flag == 1)
        return ;
#endif

#if HOTKNOT_BLOCK_RW
/*add by prize, eileen*/
	if (gtp_hotknot_enabled)
/*add by prize, eileen*/
	{
	if(hotknot_paired_flag)
		return;
	}
#endif
	
    if(gtp_loading_fw == 0)
    {
        ret = gtp_wakeup_sleep(i2c_client_point);
    
        if (ret < 0)
        {
            GTP_ERROR("GTP later resume failed.");
        }
    }
	
	if (!gtp_hotknot_enabled) {
		u8 exit_slave_cmd = 0x28;
		GTP_ERROR("hotknot is disabled,exit slave mode.");
		msleep(10);
		i2c_write_bytes_non_dma(i2c_client_point, 0x8046, &exit_slave_cmd, 1);
		i2c_write_bytes_non_dma(i2c_client_point, 0x8040, &exit_slave_cmd, 1);	
	}

#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        // do nothing
    }
    else
#endif
    {
/*add by prize, eileen*/
   //     gtp_send_cfg(i2c_client_point); // delete by yangsonglin 20150808
/*add by prize, eileen*/
    }
    
#if GTP_CHARGER_SWITCH
    gtp_charger_switch(1);  // force update
#endif

#if GTP_GESTURE_WAKEUP
/*add by prize, eileen*/
if (gesenable)
{
/*add by prize, eileen*/
    doze_status = DOZE_DISABLED;
	tpd_halt = 0;
/*add by prize, eileen*/
}
else
//#else 
#endif
{
/*add by prize, eileen*/
    mutex_lock(&i2c_access);
    tpd_halt = 0;
    enable_irq(touch_irq); /*mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);*/
    mutex_unlock(&i2c_access);
/*add by prize, eileen*/
}
//#endif
/*add by prize, eileen*/

#if GTP_ESD_PROTECT
    gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif

#ifdef GTP_CHARGER_DETECT
    queue_delayed_work(gtp_charger_check_workqueue, &gtp_charger_check_work, clk_tick_cnt);
#endif

}

static struct tpd_driver_t tpd_device_driver =
{
    .tpd_device_name = "gt9xx",
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif
};

static void tpd_off(void)
{
#ifdef CONFIG_MACH_MT6580
	if(regulator_disable(tpd->reg))	/* disable regulator */
		GTP_ERROR("regulator_disable() failed!\n");
#else
#ifdef TPD_POWER_SOURCE_CUSTOM
	//disable regulator
    if (regulator_disable(tpd->reg)){
		GTP_ERROR("regulator_disable() failed!");
	}
#else
	hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
	hwPowerDown(TPD_POWER_SOURCE_1800, "TP");
#endif
#endif
    GTP_INFO("GTP enter sleep!");
   
    tpd_halt = 1;
    /* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	disable_irq(touch_irq);
}

static void tpd_on(void)
{
    s32 ret = -1, retry = 0;	

    while (retry++ < 5)
    {
        ret = tpd_power_on(i2c_client_point);

        if (ret < 0)
        {
            GTP_ERROR("I2C Power on ERROR!");
        }

        ret = gtp_send_cfg(i2c_client_point);

        if (ret > 0)
        {
            GTP_ERROR("Wakeup sleep send config success.");
        }
    }
    if (ret < 0)
    {
        GTP_ERROR("GTP later resume failed.");
    }

    enable_irq(touch_irq);//(mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    tpd_halt = 0;
}

/* called when loaded into kernel */
static int __init tpd_driver_init(void)
{
    GTP_INFO("MediaTek gt91xx touch panel driver init\n");
//#if defined(TPD_I2C_NUMBER)	
//    i2c_register_board_info(TPD_I2C_NUMBER, &i2c_tpd, 1);
//#else
//    i2c_register_board_info(0, &i2c_tpd, 1)
//#endif
	printk("cxw tpd_driver_initdfdsssssssssssssss\n");
	tpd_get_dts_info();
	printk("cxw tpd_driver_init\n");
    if (tpd_driver_add(&tpd_device_driver) < 0)
        GTP_INFO("add generic driver failed\n");

    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
    GTP_INFO("MediaTek gt91xx touch panel driver exit\n");
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

