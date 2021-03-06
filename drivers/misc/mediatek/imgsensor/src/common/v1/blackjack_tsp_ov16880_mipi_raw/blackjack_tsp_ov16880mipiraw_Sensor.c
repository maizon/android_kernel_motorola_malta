/*****************************************************************************
 *
 * Filename:
 * ---------
 *   OV16880mipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * [201501] PDAF MP Version 
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "blackjack_tsp_ov16880mipiraw_Sensor.h"
//#include "ov16880_eeprom.h"

#define PFX "OV16880_camera_sensor"
#define LOG_1 LOG_INF("OV16880,MIPI 4LANE,PDAF\n")
#define LOG_2 LOG_INF("preview 2336*1752@30fps,768Mbps/lane; video 4672*3504@30fps,1440Mbps/lane; capture 16M@30fps,1440Mbps/lane\n")
#define LOG_INF(fmt, args...)   pr_err(PFX "[%s] " fmt, __FUNCTION__, ##args)
#define LOGE(format, args...)   pr_err("[%s] " format, __FUNCTION__, ##args)


static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = BLACKJACK_TSP_OV16880_SENSOR_ID,
	.checksum_value = 0xfe9e1a79,

	.pre = {
		.pclk = 288000000,											//record different mode's pclk
		.linelength = 2512,		/*OV16880 Note: linelength/4,it means line length per lane*/
		.framelength = 3824,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2336,		//record different mode's width of grabwindow
		.grabwindow_height = 1752,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
	},
	
	.cap = { /* 95:line 5312, 52/35:line 5336 */
		.pclk = 576000000,											//record different mode's pclk
		.linelength = 5024,		/*OV16880 Note: linelength/4,it means line length per lane*/
		.framelength = 3824,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 4672,		//record different mode's width of grabwindow
		.grabwindow_height = 3504,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 105,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000,
	},

	.normal_video = {
		.pclk = 576000000,											//record different mode's pclk
		.linelength = 5024,		/*OV16880 Note: linelength/4,it means line length per lane*/
		.framelength = 3824,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 4672,		//record different mode's width of grabwindow
		.grabwindow_height = 3504,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 105,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000,
	},


	.hs_video = {
		.pclk = 288000000,											//record different mode's pclk
		.linelength = 5024,		/*OV16880 Note: linelength/4,it means line length per lane*/
		.framelength = 1912,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2336,		//record different mode's width of grabwindow
		.grabwindow_height = 1752,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 30,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
	},

	.slim_video = {
		.pclk = 288000000,											//record different mode's pclk
		.linelength = 2512,		/*OV16880 Note: linelength/4,it means line length per lane*/
		.framelength = 3824,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2336,		//record different mode's width of grabwindow
		.grabwindow_height = 1752,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 30,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
	},
	.margin = 8,
	.min_shutter = 4,
	.max_frame_length = 0x7fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_8MA,	
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_CSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = 1,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20,0xff},
	.i2c_speed = 400,// i2c read/write speed
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_V_MIRROR,//the truth is mirror, no flip 
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = KAL_FALSE, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{{ 4704, 3536,	   0,     8, 4704, 3520, 2352, 1760,  8, 4, 2336, 1752,	  0,	0, 2336, 1752}, // Preview 
 { 4704, 3536,	   0,     8, 4704, 3520, 4704, 3520, 16, 8, 4672, 3504,	  0,	0, 4672, 3504}, // capture
 { 4704, 3536,	   0,     8, 4704, 3520, 4704, 3520, 16, 8, 4672, 3504,	  0,	0, 4672, 3504}, // video 
 { 4704, 3536,	   0,     8, 4704, 3520, 2352, 1760,  8, 4, 2336, 1752,	  0,	0, 2336, 1752}, //hight speed video
 { 4704, 3536,	   0,     8, 4704, 3520, 2352, 1760,  8, 4, 2336, 1752,	  0,	0, 2336, 1752}};// slim video
 
 

/*VC1 for HDR(N/A) , VC2 for PDAF(VC0,DT=0X19),*/
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {	/* Preview mode setting */
	{0x02, 0x0a, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x2b, 0x0000, 0x0000, 0x01, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 0x015E, 0x0340, 0x03, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x02, 0x0a, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x2b, 0x0000, 0x0000, 0x01, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 0x015E, 0x0340, 0x03, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x2b, 0x0000, 0x0000, 0x01, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 0x015E, 0x0340, 0x03, 0x00, 0x0000, 0x0000}
};
 	
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 96,
	.i4OffsetY = 88,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 8,
	.i4SubBlkW = 16,
	.i4SubBlkH = 8,
    .i4BlockNumX =140,
    .i4BlockNumY =104,
	.i4PosL = {
		{106, 90}, {122, 90}, {98, 102}, {114, 102}, {106, 106}, {122, 106}, {98, 118},
		{114, 118} },
	.i4PosR = {
		{106, 94}, {122, 94}, {98, 98}, {114, 98}, {106, 110}, {122, 110}, {98, 114},
		{114, 114} },
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte, 2, imgsensor.i2c_write_id);
    return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte,1,imgsensor.i2c_write_id);
    return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    char pusendcmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
    iWriteRegI2C(pusendcmd , 3, imgsensor.i2c_write_id);
}


static void set_dummy(void)
{
	 LOG_INF("dummyline = %d, dummypixels = %d ", imgsensor.dummy_line, imgsensor.dummy_pixel);
  //set vertical_total_size, means framelength
	write_cmos_sensor_8(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);	 
	
	//set horizontal_total_size, means linelength 
	write_cmos_sensor_8(0x380c, (imgsensor.line_length) >> 8);
	write_cmos_sensor_8(0x380d, (imgsensor.line_length) & 0xFF);

}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
			frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
	imgsensor.frame_length = imgsensor_info.max_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;
	}
	if (min_framelength_en)
	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	set_dummy();
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_control enable =%d\n", enable);
	if (enable)
		write_cmos_sensor_8(0x0100, 0x01);
	else
		write_cmos_sensor_8(0x0100, 0x00);

	mdelay(30);

	return ERROR_NONE;
}
static void write_shutter(kal_uint32 shutter)
{
    kal_uint16 realtime_fps = 0;

    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;

			// Framelength should be an even number
			shutter = (shutter >> 1) << 1;
			imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

			if (imgsensor.autoflicker_en) {
				realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
				if(realtime_fps >= 297 && realtime_fps <= 305)
					set_max_framerate(296,0);
				else if(realtime_fps >= 147 && realtime_fps <= 150)
					set_max_framerate(146,0);
				else {
					// Extend frame length
					write_cmos_sensor_8(0x380e, (imgsensor.frame_length >> 8) & 0x7f);
					write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);
				}
			} else {
				// Extend frame length
				imgsensor.frame_length = (imgsensor.frame_length  >> 1) << 1;
				write_cmos_sensor_8(0x380e, (imgsensor.frame_length >> 8) & 0x7f);
				write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);
			}


			// Update Shutter
 			write_cmos_sensor_8(0x3502, (shutter) & 0xFF);
 			write_cmos_sensor_8(0x3501, (shutter >> 8) & 0x7F);

			LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
}


static void set_shutter_frame_length(kal_uint16 shutter,
			kal_uint16 frame_length)
{
	kal_uint16 realtime_fps = 0;

	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	if (frame_length > 1)
		imgsensor.frame_length = frame_length;

    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;

	//frame_length and shutter should be an even number.
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
//auroflicker:need to avoid 15fps and 30 fps
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);
		else {
			// Extend frame length
			write_cmos_sensor_8(0x380e, (imgsensor.frame_length >> 8) & 0x7f);
			write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
	imgsensor.frame_length = (imgsensor.frame_length  >> 1) << 1;

		write_cmos_sensor_8(0x380e, (imgsensor.frame_length >> 8) & 0x7f);
		write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);
	}

		write_cmos_sensor_8(0x3502, (shutter) & 0xFF);
		write_cmos_sensor_8(0x3501, (shutter >> 8) & 0x7F);

	LOG_INF("shutter =%d, framelength =%d, realtime_fps =%d\n",
		shutter, imgsensor.frame_length, realtime_fps);
}				/* set_shutter_frame_length */

/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */


/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	/*
	* sensor gain 1x = 0x10
	* max gain = 0xf8 = 15.5x
	*/
	kal_uint16 reg_gain = 0;
	
	reg_gain = gain*16/BASEGAIN;
		if(reg_gain < 0x10)
		{
			reg_gain = 0x10;
		}
		if(reg_gain > 0x10)
		{
			reg_gain = 0x10;
		}
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain; 
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x350a, reg_gain >> 8);
	write_cmos_sensor_8(0x350b, reg_gain & 0xFF);
	
	return gain;
}	/*	set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
#if 0
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
			if (le > imgsensor.min_frame_length - imgsensor_info.margin)
				imgsensor.frame_length = le + imgsensor_info.margin;
			else
				imgsensor.frame_length = imgsensor.min_frame_length;
			if (imgsensor.frame_length > imgsensor_info.max_frame_length)
				imgsensor.frame_length = imgsensor_info.max_frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
			if (se < imgsensor_info.min_shutter) se = imgsensor_info.min_shutter;


		write_cmos_sensor_8(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor_8(0x3502, (le ) & 0xFF);
		write_cmos_sensor_8(0x3501, (le >> 8) & 0x7F);
		
		write_cmos_sensor_8(0x3508, (se ) & 0xFF); 
		write_cmos_sensor_8(0x3507, (se >> 8) & 0x7F);
		
	LOG_INF("iHDR:imgsensor.frame_length=%d\n",imgsensor.frame_length);
		set_gain(gain);
	}

#endif

}


#if 0
static void set_mirror_flip_preview(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d", image_mirror);
	spin_lock(&imgsensor_drv_lock);
    imgsensor.mirror= image_mirror;
    spin_unlock(&imgsensor_drv_lock);  
    switch (image_mirror) {
		case IMAGE_NORMAL:
			write_cmos_sensor_8(0x3820, 0x81);
			write_cmos_sensor_8(0x3821, 0x02);
			write_cmos_sensor_8(0x4000, 0x17);
			break;
		case IMAGE_H_MIRROR://mirror
			write_cmos_sensor_8(0x3820, 0x81);
			write_cmos_sensor_8(0x3821, 0x06);
			write_cmos_sensor_8(0x4000, 0x17);
			break;
		case IMAGE_V_MIRROR://flip
			write_cmos_sensor_8(0x3820, 0xc5);
			write_cmos_sensor_8(0x3821, 0x02);
			write_cmos_sensor_8(0x4000, 0x57);
			break;
		case IMAGE_HV_MIRROR://mirror&flip
			write_cmos_sensor_8(0x3820, 0xc5);
			write_cmos_sensor_8(0x3821, 0x06);
			write_cmos_sensor_8(0x4000, 0x57);
			break;
		default:
			LOG_INF("Error image_mirror setting\n");
    }

}

static void set_mirror_flip_capture(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d", image_mirror);
	spin_lock(&imgsensor_drv_lock);
    imgsensor.mirror= image_mirror;
    spin_unlock(&imgsensor_drv_lock);  
    switch (image_mirror) {
		case IMAGE_NORMAL:
			write_cmos_sensor_8(0x3820, 0x80);
			write_cmos_sensor_8(0x3821, 0x00);
			write_cmos_sensor_8(0x4000, 0x17);
			break;
		case IMAGE_H_MIRROR://mirror
			write_cmos_sensor_8(0x3820, 0x80);
			write_cmos_sensor_8(0x3821, 0x04);
			write_cmos_sensor_8(0x4000, 0x17);
			break;
		case IMAGE_V_MIRROR://flip
			write_cmos_sensor_8(0x3820, 0xc4);
			write_cmos_sensor_8(0x3821, 0x00);
			write_cmos_sensor_8(0x4000, 0x57);
			break;
		case IMAGE_HV_MIRROR://mirror&flip
			write_cmos_sensor_8(0x3820, 0xc4);
			write_cmos_sensor_8(0x3821, 0x04);
			write_cmos_sensor_8(0x4000, 0x57);
			break;
		default:
			LOG_INF("Error image_mirror setting\n");
    }

}
#endif
/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/


static void sensor_init(void)
{
	LOG_INF("E\n");


    write_cmos_sensor_8(0x301e, 0x00);
    write_cmos_sensor_8(0x0103, 0x01);
    mdelay(5); //Delay 1ms           ;
	write_cmos_sensor_8(0x0300, 0x00);
	write_cmos_sensor_8(0x0302, 0x3d);
	write_cmos_sensor_8(0x0303, 0x00);
	write_cmos_sensor_8(0x0304, 0x07);
	write_cmos_sensor_8(0x030e, 0x02);
	write_cmos_sensor_8(0x030f, 0x03);
	write_cmos_sensor_8(0x0312, 0x03);
	write_cmos_sensor_8(0x0313, 0x03);
	write_cmos_sensor_8(0x031e, 0x09);
	write_cmos_sensor_8(0x3002, 0x00);
	write_cmos_sensor_8(0x3009, 0x06);
	write_cmos_sensor_8(0x3010, 0x00);
	write_cmos_sensor_8(0x3011, 0x04);
	write_cmos_sensor_8(0x3012, 0x41);
	write_cmos_sensor_8(0x3013, 0xcc);
	write_cmos_sensor_8(0x3017, 0xf0);
	write_cmos_sensor_8(0x3018, 0xda);
	write_cmos_sensor_8(0x3019, 0xf1);
	write_cmos_sensor_8(0x301a, 0xf2);
	write_cmos_sensor_8(0x301b, 0x96);
	write_cmos_sensor_8(0x301d, 0x02);
	write_cmos_sensor_8(0x3022, 0x0e);
	write_cmos_sensor_8(0x3023, 0xb0);
	write_cmos_sensor_8(0x3028, 0xc3);
	write_cmos_sensor_8(0x3031, 0x68);
	write_cmos_sensor_8(0x3400, 0x08);
	write_cmos_sensor_8(0x3501, 0x0e);
	write_cmos_sensor_8(0x3502, 0xea);
	write_cmos_sensor_8(0x3507, 0x02);
	write_cmos_sensor_8(0x3508, 0x00);
	write_cmos_sensor_8(0x3509, 0x12);
	write_cmos_sensor_8(0x350a, 0x00);
	write_cmos_sensor_8(0x350b, 0x40);
	write_cmos_sensor_8(0x3549, 0x12);
	write_cmos_sensor_8(0x354e, 0x00);
	write_cmos_sensor_8(0x354f, 0x10);
	write_cmos_sensor_8(0x3600, 0x12);
	write_cmos_sensor_8(0x3603, 0xc0);
	write_cmos_sensor_8(0x3604, 0x2c);
	write_cmos_sensor_8(0x3606, 0x55);
	write_cmos_sensor_8(0x3607, 0x05);
	write_cmos_sensor_8(0x360a, 0x6d);
	write_cmos_sensor_8(0x360d, 0x05);
	write_cmos_sensor_8(0x360e, 0x07);
	write_cmos_sensor_8(0x360f, 0x44);
	write_cmos_sensor_8(0x3622, 0x75);
	write_cmos_sensor_8(0x3623, 0x57);
	write_cmos_sensor_8(0x3624, 0x50);
	write_cmos_sensor_8(0x362b, 0x77);
	write_cmos_sensor_8(0x3641, 0x00);
	write_cmos_sensor_8(0x3660, 0xc0);
	write_cmos_sensor_8(0x3661, 0x0f);
	write_cmos_sensor_8(0x3662, 0x00);
	write_cmos_sensor_8(0x3663, 0x20);
	write_cmos_sensor_8(0x3664, 0x08);
	write_cmos_sensor_8(0x3667, 0x00);
	write_cmos_sensor_8(0x366a, 0x54);
	write_cmos_sensor_8(0x366c, 0x10);
	write_cmos_sensor_8(0x3708, 0x42);
	write_cmos_sensor_8(0x3709, 0x1c);
	write_cmos_sensor_8(0x3718, 0x8c);
	write_cmos_sensor_8(0x3719, 0x00);
	write_cmos_sensor_8(0x371a, 0x04);
	write_cmos_sensor_8(0x3725, 0xf0);
	write_cmos_sensor_8(0x3726, 0x00);
	write_cmos_sensor_8(0x3727, 0x22);
	write_cmos_sensor_8(0x3728, 0x22);
	write_cmos_sensor_8(0x3729, 0x00);
	write_cmos_sensor_8(0x372a, 0x20);
	write_cmos_sensor_8(0x372b, 0x40);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3730, 0x00);
	write_cmos_sensor_8(0x3731, 0x00);
	write_cmos_sensor_8(0x3732, 0x00);
	write_cmos_sensor_8(0x3733, 0x00);
	write_cmos_sensor_8(0x3748, 0x00);
	write_cmos_sensor_8(0x3760, 0x92);
	write_cmos_sensor_8(0x3767, 0x30);
	write_cmos_sensor_8(0x3772, 0x00);
	write_cmos_sensor_8(0x3773, 0x00);
	write_cmos_sensor_8(0x3774, 0x40);
	write_cmos_sensor_8(0x3775, 0x81);
	write_cmos_sensor_8(0x3776, 0x31);
	write_cmos_sensor_8(0x3777, 0x06);
	write_cmos_sensor_8(0x3778, 0xa0);
	write_cmos_sensor_8(0x377f, 0x31);
	write_cmos_sensor_8(0x378d, 0x39);
	write_cmos_sensor_8(0x3790, 0xcc);
	write_cmos_sensor_8(0x3791, 0xcc);
	write_cmos_sensor_8(0x379c, 0x02);
	write_cmos_sensor_8(0x379d, 0x00);
	write_cmos_sensor_8(0x379e, 0x00);
	write_cmos_sensor_8(0x379f, 0x1e);
	write_cmos_sensor_8(0x37a0, 0x22);
	write_cmos_sensor_8(0x37a1, 0x02);
	write_cmos_sensor_8(0x37a5, 0x96);
	write_cmos_sensor_8(0x37a2, 0x04);
	write_cmos_sensor_8(0x37a3, 0x23);
	write_cmos_sensor_8(0x37a4, 0x00);
	write_cmos_sensor_8(0x37b0, 0x48);
	write_cmos_sensor_8(0x37b3, 0x62);
	write_cmos_sensor_8(0x37b6, 0x22);
	write_cmos_sensor_8(0x37b9, 0x23);
	write_cmos_sensor_8(0x37c3, 0x07);
	write_cmos_sensor_8(0x37c6, 0x35);
	write_cmos_sensor_8(0x37c8, 0x00);
	write_cmos_sensor_8(0x37c9, 0x00);
	write_cmos_sensor_8(0x37cc, 0x93);
	write_cmos_sensor_8(0x3800, 0x00);
	write_cmos_sensor_8(0x3801, 0x00);
	write_cmos_sensor_8(0x3802, 0x00);
	write_cmos_sensor_8(0x3803, 0x08);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0x0d);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x12);
	write_cmos_sensor_8(0x3809, 0x40);
	write_cmos_sensor_8(0x380a, 0x0d);
	write_cmos_sensor_8(0x380b, 0xb0);
	write_cmos_sensor_8(0x380c, 0x13);
	write_cmos_sensor_8(0x380d, 0xa0);
	write_cmos_sensor_8(0x380e, 0x0e);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3810, 0x00);
	write_cmos_sensor_8(0x3811, 0x10);
	write_cmos_sensor_8(0x3812, 0x00);
	write_cmos_sensor_8(0x3813, 0x08);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0xc4);	//Mirror&Flip
	write_cmos_sensor_8(0x3821, 0x00);	//Mirror&Flip
	write_cmos_sensor_8(0x3836, 0x14);
	write_cmos_sensor_8(0x3837, 0x02);
	write_cmos_sensor_8(0x3841, 0x04);
	write_cmos_sensor_8(0x3d85, 0x17);
	write_cmos_sensor_8(0x3d8c, 0x79);
	write_cmos_sensor_8(0x3d8d, 0x7f);
	write_cmos_sensor_8(0x3f00, 0x50);
	write_cmos_sensor_8(0x3f85, 0x00);
	write_cmos_sensor_8(0x3f86, 0x00);
	write_cmos_sensor_8(0x3f87, 0x05);
	write_cmos_sensor_8(0x3f9e, 0x00);
	write_cmos_sensor_8(0x3f9f, 0x00);
	write_cmos_sensor_8(0x4000, 0x57);	//Mirror&Flip
	write_cmos_sensor_8(0x4001, 0x60);
	write_cmos_sensor_8(0x4003, 0x40);
	write_cmos_sensor_8(0x4006, 0x00);
	write_cmos_sensor_8(0x4007, 0x00);
	write_cmos_sensor_8(0x4008, 0x00);
	write_cmos_sensor_8(0x4009, 0x0f);
	write_cmos_sensor_8(0x400f, 0x09);
	write_cmos_sensor_8(0x4010, 0xf0);
	write_cmos_sensor_8(0x4011, 0xf0);
	write_cmos_sensor_8(0x4013, 0x02);
	write_cmos_sensor_8(0x4018, 0x04);
	write_cmos_sensor_8(0x4019, 0x04);
	write_cmos_sensor_8(0x401a, 0x48);
	write_cmos_sensor_8(0x4020, 0x08);
	write_cmos_sensor_8(0x4022, 0x08);
	write_cmos_sensor_8(0x4024, 0x08);
	write_cmos_sensor_8(0x4026, 0x08);
	write_cmos_sensor_8(0x4028, 0x08);
	write_cmos_sensor_8(0x402a, 0x08);
	write_cmos_sensor_8(0x402c, 0x08);
	write_cmos_sensor_8(0x402e, 0x08);
	write_cmos_sensor_8(0x4030, 0x08);
	write_cmos_sensor_8(0x4032, 0x08);
	write_cmos_sensor_8(0x4034, 0x08);
	write_cmos_sensor_8(0x4036, 0x08);
	write_cmos_sensor_8(0x4038, 0x08);
	write_cmos_sensor_8(0x403a, 0x08);
	write_cmos_sensor_8(0x403c, 0x08);
	write_cmos_sensor_8(0x403e, 0x08);
	write_cmos_sensor_8(0x4040, 0x00);
	write_cmos_sensor_8(0x4041, 0x00);
	write_cmos_sensor_8(0x4042, 0x00);
	write_cmos_sensor_8(0x4043, 0x00);
	write_cmos_sensor_8(0x4044, 0x00);
	write_cmos_sensor_8(0x4045, 0x00);
	write_cmos_sensor_8(0x4046, 0x00);
	write_cmos_sensor_8(0x4047, 0x00);
	write_cmos_sensor_8(0x4050, 0x04);
	write_cmos_sensor_8(0x4051, 0x0f);
	write_cmos_sensor_8(0x40b0, 0x00);
	write_cmos_sensor_8(0x40b1, 0x00);
	write_cmos_sensor_8(0x40b2, 0x00);
	write_cmos_sensor_8(0x40b3, 0x00);
	write_cmos_sensor_8(0x40b4, 0x00);
	write_cmos_sensor_8(0x40b5, 0x00);
	write_cmos_sensor_8(0x40b6, 0x00);
	write_cmos_sensor_8(0x40b7, 0x00);
	write_cmos_sensor_8(0x4052, 0x00);
	write_cmos_sensor_8(0x4053, 0x80);
	write_cmos_sensor_8(0x4054, 0x00);
	write_cmos_sensor_8(0x4055, 0x80);
	write_cmos_sensor_8(0x4056, 0x00);
	write_cmos_sensor_8(0x4057, 0x80);
	write_cmos_sensor_8(0x4058, 0x00);
	write_cmos_sensor_8(0x4059, 0x80);
	write_cmos_sensor_8(0x4202, 0x00);
	write_cmos_sensor_8(0x4203, 0x00);
	write_cmos_sensor_8(0x4066, 0x02);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4509, 0x07);
	write_cmos_sensor_8(0x4605, 0x03);
	write_cmos_sensor_8(0x4641, 0x23);
	write_cmos_sensor_8(0x4645, 0x04);
	write_cmos_sensor_8(0x4800, 0x04);
	write_cmos_sensor_8(0x4809, 0x2b);
	write_cmos_sensor_8(0x4812, 0x2b);
	write_cmos_sensor_8(0x4813, 0x98);
	write_cmos_sensor_8(0x4833, 0x18);
	write_cmos_sensor_8(0x4837, 0x0b);
	write_cmos_sensor_8(0x484b, 0x01);
	write_cmos_sensor_8(0x4850, 0x7c);
	write_cmos_sensor_8(0x4852, 0x06);
	write_cmos_sensor_8(0x4856, 0x58);
	write_cmos_sensor_8(0x4857, 0xaa);
	write_cmos_sensor_8(0x4862, 0x0a);
	write_cmos_sensor_8(0x4867, 0x02);
	write_cmos_sensor_8(0x4869, 0x18);
	write_cmos_sensor_8(0x486a, 0x6a);
	write_cmos_sensor_8(0x486e, 0x07);
	write_cmos_sensor_8(0x486f, 0x55);
	write_cmos_sensor_8(0x4875, 0xf0);
	write_cmos_sensor_8(0x4b05, 0x93);
	write_cmos_sensor_8(0x4b06, 0x00);
	write_cmos_sensor_8(0x4c01, 0x5f);
	write_cmos_sensor_8(0x4d00, 0x04);
	write_cmos_sensor_8(0x4d01, 0x22);
	write_cmos_sensor_8(0x4d02, 0xb7);
	write_cmos_sensor_8(0x4d03, 0xca);
	write_cmos_sensor_8(0x4d04, 0x30);
	write_cmos_sensor_8(0x4d05, 0x1d);
	write_cmos_sensor_8(0x4f00, 0x1c);
	write_cmos_sensor_8(0x4f03, 0x50);
	write_cmos_sensor_8(0x4f04, 0x01);
	write_cmos_sensor_8(0x4f05, 0x7c);
	write_cmos_sensor_8(0x4f08, 0x00);
	write_cmos_sensor_8(0x4f09, 0x60);
	write_cmos_sensor_8(0x4f0a, 0x00);
	write_cmos_sensor_8(0x4f0b, 0x30);
	write_cmos_sensor_8(0x4f14, 0xf0);
	write_cmos_sensor_8(0x4f15, 0xf0);
	write_cmos_sensor_8(0x4f16, 0xf0);
	write_cmos_sensor_8(0x4f17, 0xf0);
	write_cmos_sensor_8(0x4f18, 0xf0);
	write_cmos_sensor_8(0x4f19, 0x00);
	write_cmos_sensor_8(0x4f1a, 0x00);
	write_cmos_sensor_8(0x4f20, 0x07);
	write_cmos_sensor_8(0x4f21, 0xd0);
	write_cmos_sensor_8(0x5000, 0x98);
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x5002, 0x1c);
	write_cmos_sensor_8(0x501d, 0x00);
	write_cmos_sensor_8(0x5020, 0x03);
	write_cmos_sensor_8(0x5022, 0x91);
	write_cmos_sensor_8(0x5023, 0x00);
	write_cmos_sensor_8(0x5030, 0x00);
	write_cmos_sensor_8(0x5056, 0x00);
	write_cmos_sensor_8(0x5063, 0x00);
	write_cmos_sensor_8(0x5200, 0x02);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5202, 0x00);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5204, 0x00);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5209, 0x81);
	write_cmos_sensor_8(0x520e, 0x31);
	write_cmos_sensor_8(0x5280, 0x00);
	write_cmos_sensor_8(0x5400, 0x01);
	write_cmos_sensor_8(0x5401, 0x00);
	write_cmos_sensor_8(0x5500, 0x00);
	write_cmos_sensor_8(0x5501, 0x00);
	write_cmos_sensor_8(0x5502, 0x00);
	write_cmos_sensor_8(0x5503, 0x00);
	write_cmos_sensor_8(0x5504, 0x00);
	write_cmos_sensor_8(0x5505, 0x00);
	write_cmos_sensor_8(0x5506, 0x00);
	write_cmos_sensor_8(0x5507, 0x00);
	write_cmos_sensor_8(0x5508, 0x20);
	write_cmos_sensor_8(0x5509, 0x00);
	write_cmos_sensor_8(0x550a, 0x20);
	write_cmos_sensor_8(0x550b, 0x00);
	write_cmos_sensor_8(0x550c, 0x00);
	write_cmos_sensor_8(0x550d, 0x00);
	write_cmos_sensor_8(0x550e, 0x00);
	write_cmos_sensor_8(0x550f, 0x00);
	write_cmos_sensor_8(0x5510, 0x00);
	write_cmos_sensor_8(0x5511, 0x00);
	write_cmos_sensor_8(0x5512, 0x00);
	write_cmos_sensor_8(0x5513, 0x00);
	write_cmos_sensor_8(0x5514, 0x00);
	write_cmos_sensor_8(0x5515, 0x00);
	write_cmos_sensor_8(0x5516, 0x00);
	write_cmos_sensor_8(0x5517, 0x00);
	write_cmos_sensor_8(0x5518, 0x20);
	write_cmos_sensor_8(0x5519, 0x00);
	write_cmos_sensor_8(0x551a, 0x20);
	write_cmos_sensor_8(0x551b, 0x00);
	write_cmos_sensor_8(0x551c, 0x00);
	write_cmos_sensor_8(0x551d, 0x00);
	write_cmos_sensor_8(0x551e, 0x00);
	write_cmos_sensor_8(0x551f, 0x00);
	write_cmos_sensor_8(0x5520, 0x00);
	write_cmos_sensor_8(0x5521, 0x00);
	write_cmos_sensor_8(0x5522, 0x00);
	write_cmos_sensor_8(0x5523, 0x00);
	write_cmos_sensor_8(0x5524, 0x00);
	write_cmos_sensor_8(0x5525, 0x00);
	write_cmos_sensor_8(0x5526, 0x00);
	write_cmos_sensor_8(0x5527, 0x00);
	write_cmos_sensor_8(0x5528, 0x00);
	write_cmos_sensor_8(0x5529, 0x20);
	write_cmos_sensor_8(0x552a, 0x00);
	write_cmos_sensor_8(0x552b, 0x20);
	write_cmos_sensor_8(0x552c, 0x00);
	write_cmos_sensor_8(0x552d, 0x00);
	write_cmos_sensor_8(0x552e, 0x00);
	write_cmos_sensor_8(0x552f, 0x00);
	write_cmos_sensor_8(0x5530, 0x00);
	write_cmos_sensor_8(0x5531, 0x00);
	write_cmos_sensor_8(0x5532, 0x00);
	write_cmos_sensor_8(0x5533, 0x00);
	write_cmos_sensor_8(0x5534, 0x00);
	write_cmos_sensor_8(0x5535, 0x00);
	write_cmos_sensor_8(0x5536, 0x00);
	write_cmos_sensor_8(0x5537, 0x00);
	write_cmos_sensor_8(0x5538, 0x00);
	write_cmos_sensor_8(0x5539, 0x20);
	write_cmos_sensor_8(0x553a, 0x00);
	write_cmos_sensor_8(0x553b, 0x20);
	write_cmos_sensor_8(0x553c, 0x00);
	write_cmos_sensor_8(0x553d, 0x00);
	write_cmos_sensor_8(0x553e, 0x00);
	write_cmos_sensor_8(0x553f, 0x00);
	write_cmos_sensor_8(0x5540, 0x00);
	write_cmos_sensor_8(0x5541, 0x00);
	write_cmos_sensor_8(0x5542, 0x00);
	write_cmos_sensor_8(0x5543, 0x00);
	write_cmos_sensor_8(0x5544, 0x00);
	write_cmos_sensor_8(0x5545, 0x00);
	write_cmos_sensor_8(0x5546, 0x00);
	write_cmos_sensor_8(0x5547, 0x00);
	write_cmos_sensor_8(0x5548, 0x20);
	write_cmos_sensor_8(0x5549, 0x00);
	write_cmos_sensor_8(0x554a, 0x20);
	write_cmos_sensor_8(0x554b, 0x00);
	write_cmos_sensor_8(0x554c, 0x00);
	write_cmos_sensor_8(0x554d, 0x00);
	write_cmos_sensor_8(0x554e, 0x00);
	write_cmos_sensor_8(0x554f, 0x00);
	write_cmos_sensor_8(0x5550, 0x00);
	write_cmos_sensor_8(0x5551, 0x00);
	write_cmos_sensor_8(0x5552, 0x00);
	write_cmos_sensor_8(0x5553, 0x00);
	write_cmos_sensor_8(0x5554, 0x00);
	write_cmos_sensor_8(0x5555, 0x00);
	write_cmos_sensor_8(0x5556, 0x00);
	write_cmos_sensor_8(0x5557, 0x00);
	write_cmos_sensor_8(0x5558, 0x20);
	write_cmos_sensor_8(0x5559, 0x00);
	write_cmos_sensor_8(0x555a, 0x20);
	write_cmos_sensor_8(0x555b, 0x00);
	write_cmos_sensor_8(0x555c, 0x00);
	write_cmos_sensor_8(0x555d, 0x00);
	write_cmos_sensor_8(0x555e, 0x00);
	write_cmos_sensor_8(0x555f, 0x00);
	write_cmos_sensor_8(0x5560, 0x00);
	write_cmos_sensor_8(0x5561, 0x00);
	write_cmos_sensor_8(0x5562, 0x00);
	write_cmos_sensor_8(0x5563, 0x00);
	write_cmos_sensor_8(0x5564, 0x00);
	write_cmos_sensor_8(0x5565, 0x00);
	write_cmos_sensor_8(0x5566, 0x00);
	write_cmos_sensor_8(0x5567, 0x00);
	write_cmos_sensor_8(0x5568, 0x00);
	write_cmos_sensor_8(0x5569, 0x20);
	write_cmos_sensor_8(0x556a, 0x00);
	write_cmos_sensor_8(0x556b, 0x20);
	write_cmos_sensor_8(0x556c, 0x00);
	write_cmos_sensor_8(0x556d, 0x00);
	write_cmos_sensor_8(0x556e, 0x00);
	write_cmos_sensor_8(0x556f, 0x00);
	write_cmos_sensor_8(0x5570, 0x00);
	write_cmos_sensor_8(0x5571, 0x00);
	write_cmos_sensor_8(0x5572, 0x00);
	write_cmos_sensor_8(0x5573, 0x00);
	write_cmos_sensor_8(0x5574, 0x00);
	write_cmos_sensor_8(0x5575, 0x00);
	write_cmos_sensor_8(0x5576, 0x00);
	write_cmos_sensor_8(0x5577, 0x00);
	write_cmos_sensor_8(0x5578, 0x00);
	write_cmos_sensor_8(0x5579, 0x20);
	write_cmos_sensor_8(0x557a, 0x00);
	write_cmos_sensor_8(0x557b, 0x20);
	write_cmos_sensor_8(0x557c, 0x00);
	write_cmos_sensor_8(0x557d, 0x00);
	write_cmos_sensor_8(0x557e, 0x00);
	write_cmos_sensor_8(0x557f, 0x00);
	write_cmos_sensor_8(0x5580, 0x00);
	write_cmos_sensor_8(0x5581, 0x00);
	write_cmos_sensor_8(0x5582, 0x00);
	write_cmos_sensor_8(0x5583, 0x00);
	write_cmos_sensor_8(0x5584, 0x00);
	write_cmos_sensor_8(0x5585, 0x00);
	write_cmos_sensor_8(0x5586, 0x00);
	write_cmos_sensor_8(0x5587, 0x00);
	write_cmos_sensor_8(0x5588, 0x20);
	write_cmos_sensor_8(0x5589, 0x00);
	write_cmos_sensor_8(0x558a, 0x20);
	write_cmos_sensor_8(0x558b, 0x00);
	write_cmos_sensor_8(0x558c, 0x00);
	write_cmos_sensor_8(0x558d, 0x00);
	write_cmos_sensor_8(0x558e, 0x00);
	write_cmos_sensor_8(0x558f, 0x00);
	write_cmos_sensor_8(0x5590, 0x00);
	write_cmos_sensor_8(0x5591, 0x00);
	write_cmos_sensor_8(0x5592, 0x00);
	write_cmos_sensor_8(0x5593, 0x00);
	write_cmos_sensor_8(0x5594, 0x00);
	write_cmos_sensor_8(0x5595, 0x00);
	write_cmos_sensor_8(0x5596, 0x00);
	write_cmos_sensor_8(0x5597, 0x00);
	write_cmos_sensor_8(0x5598, 0x20);
	write_cmos_sensor_8(0x5599, 0x00);
	write_cmos_sensor_8(0x559a, 0x20);
	write_cmos_sensor_8(0x559b, 0x00);
	write_cmos_sensor_8(0x559c, 0x00);
	write_cmos_sensor_8(0x559d, 0x00);
	write_cmos_sensor_8(0x559e, 0x00);
	write_cmos_sensor_8(0x559f, 0x00);
	write_cmos_sensor_8(0x55a0, 0x00);
	write_cmos_sensor_8(0x55a1, 0x00);
	write_cmos_sensor_8(0x55a2, 0x00);
	write_cmos_sensor_8(0x55a3, 0x00);
	write_cmos_sensor_8(0x55a4, 0x00);
	write_cmos_sensor_8(0x55a5, 0x00);
	write_cmos_sensor_8(0x55a6, 0x00);
	write_cmos_sensor_8(0x55a7, 0x00);
	write_cmos_sensor_8(0x55a8, 0x00);
	write_cmos_sensor_8(0x55a9, 0x20);
	write_cmos_sensor_8(0x55aa, 0x00);
	write_cmos_sensor_8(0x55ab, 0x20);
	write_cmos_sensor_8(0x55ac, 0x00);
	write_cmos_sensor_8(0x55ad, 0x00);
	write_cmos_sensor_8(0x55ae, 0x00);
	write_cmos_sensor_8(0x55af, 0x00);
	write_cmos_sensor_8(0x55b0, 0x00);
	write_cmos_sensor_8(0x55b1, 0x00);
	write_cmos_sensor_8(0x55b2, 0x00);
	write_cmos_sensor_8(0x55b3, 0x00);
	write_cmos_sensor_8(0x55b4, 0x00);
	write_cmos_sensor_8(0x55b5, 0x00);
	write_cmos_sensor_8(0x55b6, 0x00);
	write_cmos_sensor_8(0x55b7, 0x00);
	write_cmos_sensor_8(0x55b8, 0x00);
	write_cmos_sensor_8(0x55b9, 0x20);
	write_cmos_sensor_8(0x55ba, 0x00);
	write_cmos_sensor_8(0x55bb, 0x20);
	write_cmos_sensor_8(0x55bc, 0x00);
	write_cmos_sensor_8(0x55bd, 0x00);
	write_cmos_sensor_8(0x55be, 0x00);
	write_cmos_sensor_8(0x55bf, 0x00);
	write_cmos_sensor_8(0x55c0, 0x00);
	write_cmos_sensor_8(0x55c1, 0x00);
	write_cmos_sensor_8(0x55c2, 0x00);
	write_cmos_sensor_8(0x55c3, 0x00);
	write_cmos_sensor_8(0x55c4, 0x00);
	write_cmos_sensor_8(0x55c5, 0x00);
	write_cmos_sensor_8(0x55c6, 0x00);
	write_cmos_sensor_8(0x55c7, 0x00);
	write_cmos_sensor_8(0x55c8, 0x20);
	write_cmos_sensor_8(0x55c9, 0x00);
	write_cmos_sensor_8(0x55ca, 0x20);
	write_cmos_sensor_8(0x55cb, 0x00);
	write_cmos_sensor_8(0x55cc, 0x00);
	write_cmos_sensor_8(0x55cd, 0x00);
	write_cmos_sensor_8(0x55ce, 0x00);
	write_cmos_sensor_8(0x55cf, 0x00);
	write_cmos_sensor_8(0x55d0, 0x00);
	write_cmos_sensor_8(0x55d1, 0x00);
	write_cmos_sensor_8(0x55d2, 0x00);
	write_cmos_sensor_8(0x55d3, 0x00);
	write_cmos_sensor_8(0x55d4, 0x00);
	write_cmos_sensor_8(0x55d5, 0x00);
	write_cmos_sensor_8(0x55d6, 0x00);
	write_cmos_sensor_8(0x55d7, 0x00);
	write_cmos_sensor_8(0x55d8, 0x20);
	write_cmos_sensor_8(0x55d9, 0x00);
	write_cmos_sensor_8(0x55da, 0x20);
	write_cmos_sensor_8(0x55db, 0x00);
	write_cmos_sensor_8(0x55dc, 0x00);
	write_cmos_sensor_8(0x55dd, 0x00);
	write_cmos_sensor_8(0x55de, 0x00);
	write_cmos_sensor_8(0x55df, 0x00);
	write_cmos_sensor_8(0x55e0, 0x00);
	write_cmos_sensor_8(0x55e1, 0x00);
	write_cmos_sensor_8(0x55e2, 0x00);
	write_cmos_sensor_8(0x55e3, 0x00);
	write_cmos_sensor_8(0x55e4, 0x00);
	write_cmos_sensor_8(0x55e5, 0x00);
	write_cmos_sensor_8(0x55e6, 0x00);
	write_cmos_sensor_8(0x55e7, 0x00);
	write_cmos_sensor_8(0x55e8, 0x00);
	write_cmos_sensor_8(0x55e9, 0x20);
	write_cmos_sensor_8(0x55ea, 0x00);
	write_cmos_sensor_8(0x55eb, 0x20);
	write_cmos_sensor_8(0x55ec, 0x00);
	write_cmos_sensor_8(0x55ed, 0x00);
	write_cmos_sensor_8(0x55ee, 0x00);
	write_cmos_sensor_8(0x55ef, 0x00);
	write_cmos_sensor_8(0x55f0, 0x00);
	write_cmos_sensor_8(0x55f1, 0x00);
	write_cmos_sensor_8(0x55f2, 0x00);
	write_cmos_sensor_8(0x55f3, 0x00);
	write_cmos_sensor_8(0x55f4, 0x00);
	write_cmos_sensor_8(0x55f5, 0x00);
	write_cmos_sensor_8(0x55f6, 0x00);
	write_cmos_sensor_8(0x55f7, 0x00);
	write_cmos_sensor_8(0x55f8, 0x00);
	write_cmos_sensor_8(0x55f9, 0x20);
	write_cmos_sensor_8(0x55fa, 0x00);
	write_cmos_sensor_8(0x55fb, 0x20);
	write_cmos_sensor_8(0x55fc, 0x00);
	write_cmos_sensor_8(0x55fd, 0x00);
	write_cmos_sensor_8(0x55fe, 0x00);
	write_cmos_sensor_8(0x55ff, 0x00);
	write_cmos_sensor_8(0x5600, 0x30);
	write_cmos_sensor_8(0x560f, 0xfc);
	write_cmos_sensor_8(0x5610, 0xf0);
	write_cmos_sensor_8(0x5611, 0x10);
	write_cmos_sensor_8(0x562f, 0xfc);
	write_cmos_sensor_8(0x5630, 0xf0);
	write_cmos_sensor_8(0x5631, 0x10);
	write_cmos_sensor_8(0x564f, 0xfc);
	write_cmos_sensor_8(0x5650, 0xf0);
	write_cmos_sensor_8(0x5651, 0x10);
	write_cmos_sensor_8(0x566f, 0xfc);
	write_cmos_sensor_8(0x5670, 0xf0);
	write_cmos_sensor_8(0x5671, 0x10);
	write_cmos_sensor_8(0x5690, 0x00);
	write_cmos_sensor_8(0x5691, 0x00);
	write_cmos_sensor_8(0x5692, 0x00);
	write_cmos_sensor_8(0x5693, 0x08);
	write_cmos_sensor_8(0x5694, 0xc0);
	write_cmos_sensor_8(0x5695, 0x00);
	write_cmos_sensor_8(0x5696, 0x00);
	write_cmos_sensor_8(0x5697, 0x08);
	write_cmos_sensor_8(0x5698, 0x00);
	write_cmos_sensor_8(0x5699, 0x70);
	write_cmos_sensor_8(0x569a, 0x11);
	write_cmos_sensor_8(0x569b, 0xf0);
	write_cmos_sensor_8(0x569c, 0x00);
	write_cmos_sensor_8(0x569d, 0x68);
	write_cmos_sensor_8(0x569e, 0x0d);
	write_cmos_sensor_8(0x569f, 0x68);
	write_cmos_sensor_8(0x56a1, 0xff);
	write_cmos_sensor_8(0x5bd9, 0x01);
	write_cmos_sensor_8(0x5d04, 0x01);
	write_cmos_sensor_8(0x5d05, 0x00);
	write_cmos_sensor_8(0x5d06, 0x01);
	write_cmos_sensor_8(0x5d07, 0x00);
	write_cmos_sensor_8(0x5d12, 0x38);
	write_cmos_sensor_8(0x5d13, 0x38);
	write_cmos_sensor_8(0x5d14, 0x38);
	write_cmos_sensor_8(0x5d15, 0x38);
	write_cmos_sensor_8(0x5d16, 0x38);
	write_cmos_sensor_8(0x5d17, 0x38);
	write_cmos_sensor_8(0x5d18, 0x38);
	write_cmos_sensor_8(0x5d19, 0x38);
	write_cmos_sensor_8(0x5d1a, 0x38);
	write_cmos_sensor_8(0x5d1b, 0x38);
	write_cmos_sensor_8(0x5d1c, 0x00);
	write_cmos_sensor_8(0x5d1d, 0x00);
	write_cmos_sensor_8(0x5d1e, 0x00);
	write_cmos_sensor_8(0x5d1f, 0x00);
	write_cmos_sensor_8(0x5d20, 0x16);
	write_cmos_sensor_8(0x5d21, 0x20);
	write_cmos_sensor_8(0x5d22, 0x10);
	write_cmos_sensor_8(0x5d23, 0xa0);
	write_cmos_sensor_8(0x5d24, 0x00);
	write_cmos_sensor_8(0x5d25, 0x00);
	write_cmos_sensor_8(0x5d26, 0x00);
	write_cmos_sensor_8(0x5d27, 0x08);
	write_cmos_sensor_8(0x5d28, 0xc0);
	write_cmos_sensor_8(0x5d29, 0x00);
	write_cmos_sensor_8(0x5d2a, 0x00);
	write_cmos_sensor_8(0x5d2b, 0x70);
	write_cmos_sensor_8(0x5d2c, 0x11);
	write_cmos_sensor_8(0x5d2d, 0xf0);
	write_cmos_sensor_8(0x5d2e, 0x00);
	write_cmos_sensor_8(0x5d2f, 0x68);
	write_cmos_sensor_8(0x5d30, 0x0d);
	write_cmos_sensor_8(0x5d31, 0x68);
	write_cmos_sensor_8(0x5d32, 0x00);
	write_cmos_sensor_8(0x5d38, 0x70);
	write_cmos_sensor_8(0x5d3a, 0x58);
	write_cmos_sensor_8(0x5c80, 0x06);
	write_cmos_sensor_8(0x5c81, 0x90);
	write_cmos_sensor_8(0x5c82, 0x09);
	write_cmos_sensor_8(0x5c83, 0x5f);
	write_cmos_sensor_8(0x5c85, 0x6c);
	write_cmos_sensor_8(0x5601, 0x04);
	write_cmos_sensor_8(0x5602, 0x02);
	write_cmos_sensor_8(0x5603, 0x01);
	write_cmos_sensor_8(0x5604, 0x04);
	write_cmos_sensor_8(0x5605, 0x02);
	write_cmos_sensor_8(0x5606, 0x01);
	write_cmos_sensor_8(0x5b80, 0x00);
	write_cmos_sensor_8(0x5b81, 0x04);
	write_cmos_sensor_8(0x5b82, 0x00);
	write_cmos_sensor_8(0x5b83, 0x08);
	write_cmos_sensor_8(0x5b84, 0x00);
	write_cmos_sensor_8(0x5b85, 0x10);
	write_cmos_sensor_8(0x5b86, 0x00);
	write_cmos_sensor_8(0x5b87, 0x20);
	write_cmos_sensor_8(0x5b88, 0x00);
	write_cmos_sensor_8(0x5b89, 0x40);
	write_cmos_sensor_8(0x5b8a, 0x00);
	write_cmos_sensor_8(0x5b8b, 0x80);
	write_cmos_sensor_8(0x5b8c, 0x1a);
	write_cmos_sensor_8(0x5b8d, 0x1a);
	write_cmos_sensor_8(0x5b8e, 0x1a);
	write_cmos_sensor_8(0x5b8f, 0x18);
	write_cmos_sensor_8(0x5b90, 0x18);
	write_cmos_sensor_8(0x5b91, 0x18);
	write_cmos_sensor_8(0x5b92, 0x1a);
	write_cmos_sensor_8(0x5b93, 0x1a);
	write_cmos_sensor_8(0x5b94, 0x1a);
	write_cmos_sensor_8(0x5b95, 0x18);
	write_cmos_sensor_8(0x5b96, 0x18);
	write_cmos_sensor_8(0x5b97, 0x18);
	write_cmos_sensor_8(0x0100, 0x00);
}	


static void preview_setting(void)
{ 
	write_cmos_sensor_8(0x0302, 0x20);
	write_cmos_sensor_8(0x030f, 0x07);
	write_cmos_sensor_8(0x3501, 0x07);
	write_cmos_sensor_8(0x3502, 0x72);
	write_cmos_sensor_8(0x3726, 0x22);
	write_cmos_sensor_8(0x3727, 0x44);
	write_cmos_sensor_8(0x3729, 0x22);
	write_cmos_sensor_8(0x372f, 0x89);
	write_cmos_sensor_8(0x3760, 0x90);
	write_cmos_sensor_8(0x37a1, 0x00);
	write_cmos_sensor_8(0x37a5, 0xaa);
	write_cmos_sensor_8(0x37a2, 0x00);
	write_cmos_sensor_8(0x37a3, 0x42);
	write_cmos_sensor_8(0x3800, 0x00);
	write_cmos_sensor_8(0x3801, 0x00);
	write_cmos_sensor_8(0x3802, 0x00);
	write_cmos_sensor_8(0x3803, 0x08);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0x0d);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x09);
	write_cmos_sensor_8(0x3809, 0x20);
	write_cmos_sensor_8(0x380a, 0x06);
	write_cmos_sensor_8(0x380b, 0xd8);
	write_cmos_sensor_8(0x380c, 0x09);
	write_cmos_sensor_8(0x380d, 0xd0);
	write_cmos_sensor_8(0x380e, 0x0e);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3810, 0x00);
	write_cmos_sensor_8(0x3811, 0x08);
	write_cmos_sensor_8(0x3812, 0x00);
	write_cmos_sensor_8(0x3813, 0x04);
	write_cmos_sensor_8(0x3814, 0x31);
	write_cmos_sensor_8(0x3815, 0x31);	
	write_cmos_sensor_8(0x3820, 0xc5);	//Mirror&Flip
	write_cmos_sensor_8(0x3821, 0x02);	//Mirror&Flip
	write_cmos_sensor_8(0x3836, 0x0c);
	write_cmos_sensor_8(0x3841, 0x02);
	write_cmos_sensor_8(0x4000, 0x57);	//Mirror&Flip
	write_cmos_sensor_8(0x4006, 0x01);
	write_cmos_sensor_8(0x4007, 0x80);
	write_cmos_sensor_8(0x4009, 0x05);
	write_cmos_sensor_8(0x4050, 0x00);
	write_cmos_sensor_8(0x4051, 0x01);
	write_cmos_sensor_8(0x4501, 0x3c);
	write_cmos_sensor_8(0x4837, 0x14);
	write_cmos_sensor_8(0x5201, 0x62);
	write_cmos_sensor_8(0x5203, 0x08);
	write_cmos_sensor_8(0x5205, 0x48);
	write_cmos_sensor_8(0x5500, 0x80);
	write_cmos_sensor_8(0x5501, 0x80);
	write_cmos_sensor_8(0x5502, 0x80);
	write_cmos_sensor_8(0x5503, 0x80);
	write_cmos_sensor_8(0x5508, 0x80);
	write_cmos_sensor_8(0x5509, 0x80);
	write_cmos_sensor_8(0x550a, 0x80);
	write_cmos_sensor_8(0x550b, 0x80);
	write_cmos_sensor_8(0x5510, 0x08);
	write_cmos_sensor_8(0x5511, 0x08);
	write_cmos_sensor_8(0x5512, 0x08);
	write_cmos_sensor_8(0x5513, 0x08);
	write_cmos_sensor_8(0x5518, 0x08);
	write_cmos_sensor_8(0x5519, 0x08);
	write_cmos_sensor_8(0x551a, 0x08);
	write_cmos_sensor_8(0x551b, 0x08);
	write_cmos_sensor_8(0x5520, 0x80);
	write_cmos_sensor_8(0x5521, 0x80);
	write_cmos_sensor_8(0x5522, 0x80);
	write_cmos_sensor_8(0x5523, 0x80);
	write_cmos_sensor_8(0x5528, 0x80);
	write_cmos_sensor_8(0x5529, 0x80);
	write_cmos_sensor_8(0x552a, 0x80);
	write_cmos_sensor_8(0x552b, 0x80);
	write_cmos_sensor_8(0x5530, 0x08);
	write_cmos_sensor_8(0x5531, 0x08);
	write_cmos_sensor_8(0x5532, 0x08);
	write_cmos_sensor_8(0x5533, 0x08);
	write_cmos_sensor_8(0x5538, 0x08);
	write_cmos_sensor_8(0x5539, 0x08);
	write_cmos_sensor_8(0x553a, 0x08);
	write_cmos_sensor_8(0x553b, 0x08);
	write_cmos_sensor_8(0x5540, 0x80);
	write_cmos_sensor_8(0x5541, 0x80);
	write_cmos_sensor_8(0x5542, 0x80);
	write_cmos_sensor_8(0x5543, 0x80);
	write_cmos_sensor_8(0x5548, 0x80);
	write_cmos_sensor_8(0x5549, 0x80);
	write_cmos_sensor_8(0x554a, 0x80);
	write_cmos_sensor_8(0x554b, 0x80);
	write_cmos_sensor_8(0x5550, 0x08);
	write_cmos_sensor_8(0x5551, 0x08);
	write_cmos_sensor_8(0x5552, 0x08);
	write_cmos_sensor_8(0x5553, 0x08);
	write_cmos_sensor_8(0x5558, 0x08);
	write_cmos_sensor_8(0x5559, 0x08);
	write_cmos_sensor_8(0x555a, 0x08);
	write_cmos_sensor_8(0x555b, 0x08);
	write_cmos_sensor_8(0x5560, 0x80);
	write_cmos_sensor_8(0x5561, 0x80);
	write_cmos_sensor_8(0x5562, 0x80);
	write_cmos_sensor_8(0x5563, 0x80);
	write_cmos_sensor_8(0x5568, 0x80);
	write_cmos_sensor_8(0x5569, 0x80);
	write_cmos_sensor_8(0x556a, 0x80);
	write_cmos_sensor_8(0x556b, 0x80);
	write_cmos_sensor_8(0x5570, 0x08);
	write_cmos_sensor_8(0x5571, 0x08);
	write_cmos_sensor_8(0x5572, 0x08);
	write_cmos_sensor_8(0x5573, 0x08);
	write_cmos_sensor_8(0x5578, 0x08);
	write_cmos_sensor_8(0x5579, 0x08);
	write_cmos_sensor_8(0x557a, 0x08);
	write_cmos_sensor_8(0x557b, 0x08);
	write_cmos_sensor_8(0x5580, 0x80);
	write_cmos_sensor_8(0x5581, 0x80);
	write_cmos_sensor_8(0x5582, 0x80);
	write_cmos_sensor_8(0x5583, 0x80);
	write_cmos_sensor_8(0x5588, 0x80);
	write_cmos_sensor_8(0x5589, 0x80);
	write_cmos_sensor_8(0x558a, 0x80);
	write_cmos_sensor_8(0x558b, 0x80);
	write_cmos_sensor_8(0x5590, 0x08);
	write_cmos_sensor_8(0x5591, 0x08);
	write_cmos_sensor_8(0x5592, 0x08);
	write_cmos_sensor_8(0x5593, 0x08);
	write_cmos_sensor_8(0x5598, 0x08);
	write_cmos_sensor_8(0x5599, 0x08);
	write_cmos_sensor_8(0x559a, 0x08);
	write_cmos_sensor_8(0x559b, 0x08);
	write_cmos_sensor_8(0x55a0, 0x80);
	write_cmos_sensor_8(0x55a1, 0x80);
	write_cmos_sensor_8(0x55a2, 0x80);
	write_cmos_sensor_8(0x55a3, 0x80);
	write_cmos_sensor_8(0x55a8, 0x80);
	write_cmos_sensor_8(0x55a9, 0x80);
	write_cmos_sensor_8(0x55aa, 0x80);
	write_cmos_sensor_8(0x55ab, 0x80);
	write_cmos_sensor_8(0x55b0, 0x08);
	write_cmos_sensor_8(0x55b1, 0x08);
	write_cmos_sensor_8(0x55b2, 0x08);
	write_cmos_sensor_8(0x55b3, 0x08);
	write_cmos_sensor_8(0x55b8, 0x08);
	write_cmos_sensor_8(0x55b9, 0x08);
	write_cmos_sensor_8(0x55ba, 0x08);
	write_cmos_sensor_8(0x55bb, 0x08);
	write_cmos_sensor_8(0x55c0, 0x80);
	write_cmos_sensor_8(0x55c1, 0x80);
	write_cmos_sensor_8(0x55c2, 0x80);
	write_cmos_sensor_8(0x55c3, 0x80);
	write_cmos_sensor_8(0x55c8, 0x80);
	write_cmos_sensor_8(0x55c9, 0x80);
	write_cmos_sensor_8(0x55ca, 0x80);
	write_cmos_sensor_8(0x55cb, 0x80);
	write_cmos_sensor_8(0x55d0, 0x08);
	write_cmos_sensor_8(0x55d1, 0x08);
	write_cmos_sensor_8(0x55d2, 0x08);
	write_cmos_sensor_8(0x55d3, 0x08);
	write_cmos_sensor_8(0x55d8, 0x08);
	write_cmos_sensor_8(0x55d9, 0x08);
	write_cmos_sensor_8(0x55da, 0x08);
	write_cmos_sensor_8(0x55db, 0x08);
	write_cmos_sensor_8(0x55e0, 0x80);
	write_cmos_sensor_8(0x55e1, 0x80);
	write_cmos_sensor_8(0x55e2, 0x80);
	write_cmos_sensor_8(0x55e3, 0x80);
	write_cmos_sensor_8(0x55e8, 0x80);
	write_cmos_sensor_8(0x55e9, 0x80);
	write_cmos_sensor_8(0x55ea, 0x80);
	write_cmos_sensor_8(0x55eb, 0x80);
	write_cmos_sensor_8(0x55f0, 0x08);
	write_cmos_sensor_8(0x55f1, 0x08);
	write_cmos_sensor_8(0x55f2, 0x08);
	write_cmos_sensor_8(0x55f3, 0x08);
	write_cmos_sensor_8(0x55f8, 0x08);
	write_cmos_sensor_8(0x55f9, 0x08);
	write_cmos_sensor_8(0x55fa, 0x08);
	write_cmos_sensor_8(0x55fb, 0x08);
	write_cmos_sensor_8(0x5693, 0x04);
	write_cmos_sensor_8(0x5d27, 0x04);
	write_cmos_sensor_8(0x5d3a, 0x50);	
	write_cmos_sensor_8(0x3661, 0x0c);
	write_cmos_sensor_8(0x366c, 0x00);
	write_cmos_sensor_8(0x4605, 0x00);
	write_cmos_sensor_8(0x4645, 0x00);
	write_cmos_sensor_8(0x4813, 0x90);
	write_cmos_sensor_8(0x486e, 0x03);
}/*	preview_setting  */


static void capture_setting(void)
{

	write_cmos_sensor_8(0x0302, 0x3d);
	write_cmos_sensor_8(0x030f, 0x03);
	write_cmos_sensor_8(0x3501, 0x0e);
	write_cmos_sensor_8(0x3502, 0xea);
	write_cmos_sensor_8(0x3726, 0x00);
	write_cmos_sensor_8(0x3727, 0x22);
	write_cmos_sensor_8(0x3729, 0x00);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3760, 0x92);
	write_cmos_sensor_8(0x37a1, 0x02);
	write_cmos_sensor_8(0x37a5, 0x96);
	write_cmos_sensor_8(0x37a2, 0x04);
	write_cmos_sensor_8(0x37a3, 0x23);
	write_cmos_sensor_8(0x3800, 0x00);
	write_cmos_sensor_8(0x3801, 0x00);
	write_cmos_sensor_8(0x3802, 0x00);
	write_cmos_sensor_8(0x3803, 0x08);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0x0d);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x12);
	write_cmos_sensor_8(0x3809, 0x40);
	write_cmos_sensor_8(0x380a, 0x0d);
	write_cmos_sensor_8(0x380b, 0xb0);
	write_cmos_sensor_8(0x380c, 0x13);
	write_cmos_sensor_8(0x380d, 0xa0);
	write_cmos_sensor_8(0x380e, 0x0e);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3810, 0x00);
	write_cmos_sensor_8(0x3811, 0x10);
	write_cmos_sensor_8(0x3812, 0x00);
	write_cmos_sensor_8(0x3813, 0x08);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0xc4);	//Mirror&Flip
	write_cmos_sensor_8(0x3821, 0x00);	//Mirror&Flip
	write_cmos_sensor_8(0x3836, 0x14);
	write_cmos_sensor_8(0x3841, 0x04);
	write_cmos_sensor_8(0x4000, 0x57);	//Mirror&Flip	
	write_cmos_sensor_8(0x4006, 0x00);
	write_cmos_sensor_8(0x4007, 0x00);
	write_cmos_sensor_8(0x4009, 0x0f);
	write_cmos_sensor_8(0x4050, 0x04);
	write_cmos_sensor_8(0x4051, 0x0f);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4837, 0x0b);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5500, 0x00);
	write_cmos_sensor_8(0x5501, 0x00);
	write_cmos_sensor_8(0x5502, 0x00);
	write_cmos_sensor_8(0x5503, 0x00);
	write_cmos_sensor_8(0x5508, 0x20);
	write_cmos_sensor_8(0x5509, 0x00);
	write_cmos_sensor_8(0x550a, 0x20);
	write_cmos_sensor_8(0x550b, 0x00);
	write_cmos_sensor_8(0x5510, 0x00);
	write_cmos_sensor_8(0x5511, 0x00);
	write_cmos_sensor_8(0x5512, 0x00);
	write_cmos_sensor_8(0x5513, 0x00);
	write_cmos_sensor_8(0x5518, 0x20);
	write_cmos_sensor_8(0x5519, 0x00);
	write_cmos_sensor_8(0x551a, 0x20);
	write_cmos_sensor_8(0x551b, 0x00);
	write_cmos_sensor_8(0x5520, 0x00);
	write_cmos_sensor_8(0x5521, 0x00);
	write_cmos_sensor_8(0x5522, 0x00);
	write_cmos_sensor_8(0x5523, 0x00);
	write_cmos_sensor_8(0x5528, 0x00);
	write_cmos_sensor_8(0x5529, 0x20);
	write_cmos_sensor_8(0x552a, 0x00);
	write_cmos_sensor_8(0x552b, 0x20);
	write_cmos_sensor_8(0x5530, 0x00);
	write_cmos_sensor_8(0x5531, 0x00);
	write_cmos_sensor_8(0x5532, 0x00);
	write_cmos_sensor_8(0x5533, 0x00);
	write_cmos_sensor_8(0x5538, 0x00);
	write_cmos_sensor_8(0x5539, 0x20);
	write_cmos_sensor_8(0x553a, 0x00);
	write_cmos_sensor_8(0x553b, 0x20);
	write_cmos_sensor_8(0x5540, 0x00);
	write_cmos_sensor_8(0x5541, 0x00);
	write_cmos_sensor_8(0x5542, 0x00);
	write_cmos_sensor_8(0x5543, 0x00);
	write_cmos_sensor_8(0x5548, 0x20);
	write_cmos_sensor_8(0x5549, 0x00);
	write_cmos_sensor_8(0x554a, 0x20);
	write_cmos_sensor_8(0x554b, 0x00);
	write_cmos_sensor_8(0x5550, 0x00);
	write_cmos_sensor_8(0x5551, 0x00);
	write_cmos_sensor_8(0x5552, 0x00);
	write_cmos_sensor_8(0x5553, 0x00);
	write_cmos_sensor_8(0x5558, 0x20);
	write_cmos_sensor_8(0x5559, 0x00);
	write_cmos_sensor_8(0x555a, 0x20);
	write_cmos_sensor_8(0x555b, 0x00);
	write_cmos_sensor_8(0x5560, 0x00);
	write_cmos_sensor_8(0x5561, 0x00);
	write_cmos_sensor_8(0x5562, 0x00);
	write_cmos_sensor_8(0x5563, 0x00);
	write_cmos_sensor_8(0x5568, 0x00);
	write_cmos_sensor_8(0x5569, 0x20);
	write_cmos_sensor_8(0x556a, 0x00);
	write_cmos_sensor_8(0x556b, 0x20);
	write_cmos_sensor_8(0x5570, 0x00);
	write_cmos_sensor_8(0x5571, 0x00);
	write_cmos_sensor_8(0x5572, 0x00);
	write_cmos_sensor_8(0x5573, 0x00);
	write_cmos_sensor_8(0x5578, 0x00);
	write_cmos_sensor_8(0x5579, 0x20);
	write_cmos_sensor_8(0x557a, 0x00);
	write_cmos_sensor_8(0x557b, 0x20);
	write_cmos_sensor_8(0x5580, 0x00);
	write_cmos_sensor_8(0x5581, 0x00);
	write_cmos_sensor_8(0x5582, 0x00);
	write_cmos_sensor_8(0x5583, 0x00);
	write_cmos_sensor_8(0x5588, 0x20);
	write_cmos_sensor_8(0x5589, 0x00);
	write_cmos_sensor_8(0x558a, 0x20);
	write_cmos_sensor_8(0x558b, 0x00);
	write_cmos_sensor_8(0x5590, 0x00);
	write_cmos_sensor_8(0x5591, 0x00);
	write_cmos_sensor_8(0x5592, 0x00);
	write_cmos_sensor_8(0x5593, 0x00);
	write_cmos_sensor_8(0x5598, 0x20);
	write_cmos_sensor_8(0x5599, 0x00);
	write_cmos_sensor_8(0x559a, 0x20);
	write_cmos_sensor_8(0x559b, 0x00);
	write_cmos_sensor_8(0x55a0, 0x00);
	write_cmos_sensor_8(0x55a1, 0x00);
	write_cmos_sensor_8(0x55a2, 0x00);
	write_cmos_sensor_8(0x55a3, 0x00);
	write_cmos_sensor_8(0x55a8, 0x00);
	write_cmos_sensor_8(0x55a9, 0x20);
	write_cmos_sensor_8(0x55aa, 0x00);
	write_cmos_sensor_8(0x55ab, 0x20);
	write_cmos_sensor_8(0x55b0, 0x00);
	write_cmos_sensor_8(0x55b1, 0x00);
	write_cmos_sensor_8(0x55b2, 0x00);
	write_cmos_sensor_8(0x55b3, 0x00);
	write_cmos_sensor_8(0x55b8, 0x00);
	write_cmos_sensor_8(0x55b9, 0x20);
	write_cmos_sensor_8(0x55ba, 0x00);
	write_cmos_sensor_8(0x55bb, 0x20);
	write_cmos_sensor_8(0x55c0, 0x00);
	write_cmos_sensor_8(0x55c1, 0x00);
	write_cmos_sensor_8(0x55c2, 0x00);
	write_cmos_sensor_8(0x55c3, 0x00);
	write_cmos_sensor_8(0x55c8, 0x20);
	write_cmos_sensor_8(0x55c9, 0x00);
	write_cmos_sensor_8(0x55ca, 0x20);
	write_cmos_sensor_8(0x55cb, 0x00);
	write_cmos_sensor_8(0x55d0, 0x00);
	write_cmos_sensor_8(0x55d1, 0x00);
	write_cmos_sensor_8(0x55d2, 0x00);
	write_cmos_sensor_8(0x55d3, 0x00);
	write_cmos_sensor_8(0x55d8, 0x20);
	write_cmos_sensor_8(0x55d9, 0x00);
	write_cmos_sensor_8(0x55da, 0x20);
	write_cmos_sensor_8(0x55db, 0x00);
	write_cmos_sensor_8(0x55e0, 0x00);
	write_cmos_sensor_8(0x55e1, 0x00);
	write_cmos_sensor_8(0x55e2, 0x00);
	write_cmos_sensor_8(0x55e3, 0x00);
	write_cmos_sensor_8(0x55e8, 0x00);
	write_cmos_sensor_8(0x55e9, 0x20);
	write_cmos_sensor_8(0x55ea, 0x00);
	write_cmos_sensor_8(0x55eb, 0x20);
	write_cmos_sensor_8(0x55f0, 0x00);
	write_cmos_sensor_8(0x55f1, 0x00);
	write_cmos_sensor_8(0x55f2, 0x00);
	write_cmos_sensor_8(0x55f3, 0x00);
	write_cmos_sensor_8(0x55f8, 0x00);
	write_cmos_sensor_8(0x55f9, 0x20);
	write_cmos_sensor_8(0x55fa, 0x00);
	write_cmos_sensor_8(0x55fb, 0x20);
	write_cmos_sensor_8(0x5693, 0x08);
	write_cmos_sensor_8(0x5d27, 0x08);
	write_cmos_sensor_8(0x5d3a, 0x58);
	//PD_VC
 	write_cmos_sensor_8(0x3641, 0x00);
 	write_cmos_sensor_8(0x3661, 0x0f);
 	write_cmos_sensor_8(0x366c, 0x10);
 	write_cmos_sensor_8(0x4605, 0x03);
 	write_cmos_sensor_8(0x4641, 0x23);
 	write_cmos_sensor_8(0x4645, 0x04);
 	write_cmos_sensor_8(0x4809, 0x2b);
 	write_cmos_sensor_8(0x4813, 0x98);
 	write_cmos_sensor_8(0x486e, 0x07);
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	write_cmos_sensor_8(0x0302, 0x3d);
	write_cmos_sensor_8(0x030f, 0x03);
	write_cmos_sensor_8(0x3501, 0x0e);
	write_cmos_sensor_8(0x3502, 0xea);
	write_cmos_sensor_8(0x3726, 0x00);
	write_cmos_sensor_8(0x3727, 0x22);
	write_cmos_sensor_8(0x3729, 0x00);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3760, 0x92);
	write_cmos_sensor_8(0x37a1, 0x02);
	write_cmos_sensor_8(0x37a5, 0x96);
	write_cmos_sensor_8(0x37a2, 0x04);
	write_cmos_sensor_8(0x37a3, 0x23);
	write_cmos_sensor_8(0x3800, 0x00);
	write_cmos_sensor_8(0x3801, 0x00);
	write_cmos_sensor_8(0x3802, 0x00);
	write_cmos_sensor_8(0x3803, 0x08);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0x0d);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x12);
	write_cmos_sensor_8(0x3809, 0x40);
	write_cmos_sensor_8(0x380a, 0x0d);
	write_cmos_sensor_8(0x380b, 0xb0);
	write_cmos_sensor_8(0x380c, 0x13);
	write_cmos_sensor_8(0x380d, 0xa0);
	write_cmos_sensor_8(0x380e, 0x0e);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3810, 0x00);
	write_cmos_sensor_8(0x3811, 0x10);
	write_cmos_sensor_8(0x3812, 0x00);
	write_cmos_sensor_8(0x3813, 0x08);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0xc4);	//Mirror&Flip
	write_cmos_sensor_8(0x3821, 0x00);	//Mirror&Flip
	write_cmos_sensor_8(0x3836, 0x14);
	write_cmos_sensor_8(0x3841, 0x04);
	write_cmos_sensor_8(0x4000, 0x57);	//Mirror&Flip	
	write_cmos_sensor_8(0x4006, 0x00);
	write_cmos_sensor_8(0x4007, 0x00);
	write_cmos_sensor_8(0x4009, 0x0f);
	write_cmos_sensor_8(0x4050, 0x04);
	write_cmos_sensor_8(0x4051, 0x0f);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4837, 0x0b);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5500, 0x00);
	write_cmos_sensor_8(0x5501, 0x00);
	write_cmos_sensor_8(0x5502, 0x00);
	write_cmos_sensor_8(0x5503, 0x00);
	write_cmos_sensor_8(0x5508, 0x20);
	write_cmos_sensor_8(0x5509, 0x00);
	write_cmos_sensor_8(0x550a, 0x20);
	write_cmos_sensor_8(0x550b, 0x00);
	write_cmos_sensor_8(0x5510, 0x00);
	write_cmos_sensor_8(0x5511, 0x00);
	write_cmos_sensor_8(0x5512, 0x00);
	write_cmos_sensor_8(0x5513, 0x00);
	write_cmos_sensor_8(0x5518, 0x20);
	write_cmos_sensor_8(0x5519, 0x00);
	write_cmos_sensor_8(0x551a, 0x20);
	write_cmos_sensor_8(0x551b, 0x00);
	write_cmos_sensor_8(0x5520, 0x00);
	write_cmos_sensor_8(0x5521, 0x00);
	write_cmos_sensor_8(0x5522, 0x00);
	write_cmos_sensor_8(0x5523, 0x00);
	write_cmos_sensor_8(0x5528, 0x00);
	write_cmos_sensor_8(0x5529, 0x20);
	write_cmos_sensor_8(0x552a, 0x00);
	write_cmos_sensor_8(0x552b, 0x20);
	write_cmos_sensor_8(0x5530, 0x00);
	write_cmos_sensor_8(0x5531, 0x00);
	write_cmos_sensor_8(0x5532, 0x00);
	write_cmos_sensor_8(0x5533, 0x00);
	write_cmos_sensor_8(0x5538, 0x00);
	write_cmos_sensor_8(0x5539, 0x20);
	write_cmos_sensor_8(0x553a, 0x00);
	write_cmos_sensor_8(0x553b, 0x20);
	write_cmos_sensor_8(0x5540, 0x00);
	write_cmos_sensor_8(0x5541, 0x00);
	write_cmos_sensor_8(0x5542, 0x00);
	write_cmos_sensor_8(0x5543, 0x00);
	write_cmos_sensor_8(0x5548, 0x20);
	write_cmos_sensor_8(0x5549, 0x00);
	write_cmos_sensor_8(0x554a, 0x20);
	write_cmos_sensor_8(0x554b, 0x00);
	write_cmos_sensor_8(0x5550, 0x00);
	write_cmos_sensor_8(0x5551, 0x00);
	write_cmos_sensor_8(0x5552, 0x00);
	write_cmos_sensor_8(0x5553, 0x00);
	write_cmos_sensor_8(0x5558, 0x20);
	write_cmos_sensor_8(0x5559, 0x00);
	write_cmos_sensor_8(0x555a, 0x20);
	write_cmos_sensor_8(0x555b, 0x00);
	write_cmos_sensor_8(0x5560, 0x00);
	write_cmos_sensor_8(0x5561, 0x00);
	write_cmos_sensor_8(0x5562, 0x00);
	write_cmos_sensor_8(0x5563, 0x00);
	write_cmos_sensor_8(0x5568, 0x00);
	write_cmos_sensor_8(0x5569, 0x20);
	write_cmos_sensor_8(0x556a, 0x00);
	write_cmos_sensor_8(0x556b, 0x20);
	write_cmos_sensor_8(0x5570, 0x00);
	write_cmos_sensor_8(0x5571, 0x00);
	write_cmos_sensor_8(0x5572, 0x00);
	write_cmos_sensor_8(0x5573, 0x00);
	write_cmos_sensor_8(0x5578, 0x00);
	write_cmos_sensor_8(0x5579, 0x20);
	write_cmos_sensor_8(0x557a, 0x00);
	write_cmos_sensor_8(0x557b, 0x20);
	write_cmos_sensor_8(0x5580, 0x00);
	write_cmos_sensor_8(0x5581, 0x00);
	write_cmos_sensor_8(0x5582, 0x00);
	write_cmos_sensor_8(0x5583, 0x00);
	write_cmos_sensor_8(0x5588, 0x20);
	write_cmos_sensor_8(0x5589, 0x00);
	write_cmos_sensor_8(0x558a, 0x20);
	write_cmos_sensor_8(0x558b, 0x00);
	write_cmos_sensor_8(0x5590, 0x00);
	write_cmos_sensor_8(0x5591, 0x00);
	write_cmos_sensor_8(0x5592, 0x00);
	write_cmos_sensor_8(0x5593, 0x00);
	write_cmos_sensor_8(0x5598, 0x20);
	write_cmos_sensor_8(0x5599, 0x00);
	write_cmos_sensor_8(0x559a, 0x20);
	write_cmos_sensor_8(0x559b, 0x00);
	write_cmos_sensor_8(0x55a0, 0x00);
	write_cmos_sensor_8(0x55a1, 0x00);
	write_cmos_sensor_8(0x55a2, 0x00);
	write_cmos_sensor_8(0x55a3, 0x00);
	write_cmos_sensor_8(0x55a8, 0x00);
	write_cmos_sensor_8(0x55a9, 0x20);
	write_cmos_sensor_8(0x55aa, 0x00);
	write_cmos_sensor_8(0x55ab, 0x20);
	write_cmos_sensor_8(0x55b0, 0x00);
	write_cmos_sensor_8(0x55b1, 0x00);
	write_cmos_sensor_8(0x55b2, 0x00);
	write_cmos_sensor_8(0x55b3, 0x00);
	write_cmos_sensor_8(0x55b8, 0x00);
	write_cmos_sensor_8(0x55b9, 0x20);
	write_cmos_sensor_8(0x55ba, 0x00);
	write_cmos_sensor_8(0x55bb, 0x20);
	write_cmos_sensor_8(0x55c0, 0x00);
	write_cmos_sensor_8(0x55c1, 0x00);
	write_cmos_sensor_8(0x55c2, 0x00);
	write_cmos_sensor_8(0x55c3, 0x00);
	write_cmos_sensor_8(0x55c8, 0x20);
	write_cmos_sensor_8(0x55c9, 0x00);
	write_cmos_sensor_8(0x55ca, 0x20);
	write_cmos_sensor_8(0x55cb, 0x00);
	write_cmos_sensor_8(0x55d0, 0x00);
	write_cmos_sensor_8(0x55d1, 0x00);
	write_cmos_sensor_8(0x55d2, 0x00);
	write_cmos_sensor_8(0x55d3, 0x00);
	write_cmos_sensor_8(0x55d8, 0x20);
	write_cmos_sensor_8(0x55d9, 0x00);
	write_cmos_sensor_8(0x55da, 0x20);
	write_cmos_sensor_8(0x55db, 0x00);
	write_cmos_sensor_8(0x55e0, 0x00);
	write_cmos_sensor_8(0x55e1, 0x00);
	write_cmos_sensor_8(0x55e2, 0x00);
	write_cmos_sensor_8(0x55e3, 0x00);
	write_cmos_sensor_8(0x55e8, 0x00);
	write_cmos_sensor_8(0x55e9, 0x20);
	write_cmos_sensor_8(0x55ea, 0x00);
	write_cmos_sensor_8(0x55eb, 0x20);
	write_cmos_sensor_8(0x55f0, 0x00);
	write_cmos_sensor_8(0x55f1, 0x00);
	write_cmos_sensor_8(0x55f2, 0x00);
	write_cmos_sensor_8(0x55f3, 0x00);
	write_cmos_sensor_8(0x55f8, 0x00);
	write_cmos_sensor_8(0x55f9, 0x20);
	write_cmos_sensor_8(0x55fa, 0x00);
	write_cmos_sensor_8(0x55fb, 0x20);
	write_cmos_sensor_8(0x5693, 0x08);
	write_cmos_sensor_8(0x5d27, 0x08);
	write_cmos_sensor_8(0x5d3a, 0x58);
	//PD_VC
 	write_cmos_sensor_8(0x3641, 0x00);
 	write_cmos_sensor_8(0x3661, 0x0f);
 	write_cmos_sensor_8(0x366c, 0x10);
 	write_cmos_sensor_8(0x4605, 0x03);
 	write_cmos_sensor_8(0x4641, 0x23);
 	write_cmos_sensor_8(0x4645, 0x04);
 	write_cmos_sensor_8(0x4809, 0x2b);
 	write_cmos_sensor_8(0x4813, 0x98);
 	write_cmos_sensor_8(0x486e, 0x07);
}

static void hs_video_setting(void)
{
	//int retry=0;
	LOG_INF("E\n");
	preview_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");

  	preview_setting();
}

extern char back_cam_name[64];
extern char back_cam_efuse_id[64];

static  void get_back_cam_efuse_id(void)
{
        int ret, i = 0;
        kal_uint8 efuse_id;

        ret = read_cmos_sensor_8(0x5000);
        write_cmos_sensor_8(0x5000, (0x00 & 0x08) | (ret & (~0x08)));

        write_cmos_sensor_8(0x0100, 0x01);
        msleep(5);
        write_cmos_sensor_8(0x3D84, 0x40);
        write_cmos_sensor_8(0x3D88, 0x70);
        write_cmos_sensor_8(0x3D89, 0x00);
        write_cmos_sensor_8(0x3D8A, 0x70);
        write_cmos_sensor_8(0x3D8B, 0x0f);
        write_cmos_sensor_8(0x0100, 0x01);

        for(i=0;i<16;i++)
        {
                efuse_id = read_cmos_sensor_8(0x7000+i);
                sprintf(back_cam_efuse_id+2*i,"%02x",efuse_id);
                msleep(1);
        }
}


/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID
*
* PARAMETERS
*	*sensorID : return the sensor ID
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {	
			*sensor_id = (read_cmos_sensor_8(0x300A) << 16) | (read_cmos_sensor_8(0x300B)<<8) | read_cmos_sensor_8(0x300C);
			LOG_INF("Read sensor id fail, write id:0x%x ,sensor Id:0x%x\n", imgsensor.i2c_write_id, *sensor_id);//lx_revised
			if (*sensor_id == imgsensor_info.sensor_id) {
				memset(back_cam_name, 0x00, sizeof(back_cam_name));
				memcpy(back_cam_name, "0_ov16880_TSP", 64);
				get_back_cam_efuse_id();
				ontim_get_otp_data(*sensor_id, NULL, 0);
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);	  
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id:0x%x ,sensor Id:0x%x\n", imgsensor.i2c_write_id,*sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF 
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	//const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;
	LOG_1;
	LOG_2;
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = (read_cmos_sensor_8(0x300A) << 16) | (read_cmos_sensor_8(0x300B)<<8) | read_cmos_sensor_8(0x300C);
			LOG_INF("Read sensor id fail, write id:0x%x ,sensor Id:0x%x\n", imgsensor.i2c_write_id, sensor_id);//lx_revised
			if (sensor_id == imgsensor_info.sensor_id) {				
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);	  
				break;
			}
			LOG_INF("Read sensor id fail, write id:0x%x ,sensor Id:0x%x\n", imgsensor.i2c_write_id,sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */



/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function*/

	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	//set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;


		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n",imgsensor.current_fps);
	//preview_setting();
	capture_setting();
    //set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	//preview_setting();
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	//set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/*	hs_video   */


static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	//set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d", scenario_id);


	//sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
	//sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
	//imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 2; //2
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

			break;
		default:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			capture(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;
		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
/*Gionee <BUG> <shenweikun> <2017-8-17> modify for GNSPR #92075 begin*/
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if((framerate==150)&&(imgsensor.pclk ==imgsensor_info.cap1.pclk))
			{
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			}
			else if(framerate==300)
			{
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			}
			else
			{
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			}
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}
/*Gionee <BUG> <shenweikun> <2017-8-17> add for GNSPR #92075 end*/
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*framerate = imgsensor_info.hs_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*framerate = imgsensor_info.slim_video.max_framerate;
			break;
		default:
			break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	// 0x5280[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
	// 0x5000[0]: OTP data enable; 0x5000[1]: AWB gain enable; 0x5000[2]: LENC enable; 0x5000[3]: BLC enable; 
	// 0x5000[4]: VFPN cancellation enable; 0x5000[5]: DCW in vertical direction enable; 0x5000[6]: DCW in horizontal enable; 0x5000[7]: ISP enable; 
	if (enable) {
		write_cmos_sensor_8(0x5280, 0x80);
		write_cmos_sensor_8(0x5000, 0x00);
	} else {
		write_cmos_sensor_8(0x5280, 0x00);
		write_cmos_sensor_8(0x5000, 0x08);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16=(UINT16 *) feature_para;
	UINT16 *feature_data_16=(UINT16 *) feature_para;
	UINT32 *feature_return_para_32=(UINT32 *) feature_para;
	UINT32 *feature_data_32=(UINT32 *) feature_para;
        UINT32 rate = 0;
	unsigned long long *feature_data=(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d", feature_id);
	switch (feature_id) 
	{
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n", imgsensor.pclk,imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_ESHUTTER:
			set_shutter(*feature_data);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
		break;
		case SENSOR_FEATURE_SET_GAIN:
			set_gain((UINT16) *feature_data);
			break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			if((sensor_reg_data->RegData>>8)>0)
			write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			else
			write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
			*feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
			set_video_mode(*feature_data);
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32);
			break;
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
			break;
		case SENSOR_FEATURE_GET_PDAF_DATA:
			LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
			//read_eeprom((kal_uint16 )(*feature_data),(char*)(uintptr_t)(*(feature_data+1)),(kal_uint32)(*(feature_data+2)));
			//read_ov16880_eeprom((kal_uint16 )(*feature_data),(char*)(uintptr_t)(*(feature_data+1)),(kal_uint32)(*(feature_data+2)));
			break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATE\n");
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				 rate =	imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate = imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate = imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				rate = imgsensor_info.pre.mipi_pixel_rate;
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
		LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATE wangweifeng:rate:%d\n",rate);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;	
	}
	break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_FRAMERATE:
			LOG_INF("current fps :%d\n", (UINT32)*feature_data);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = *feature_data;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_SET_HDR:
			//LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_16);
			LOG_INF("Warning! Not Support IHDR Feature");
			spin_lock(&imgsensor_drv_lock);
			//imgsensor.ihdr_en = (BOOL)*feature_data_16;
			imgsensor.ihdr_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
			wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

			switch (*feature_data_32) 
			{
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			}
			break;
		case SENSOR_FEATURE_GET_PDAF_INFO:
			LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n", (UINT32)*feature_data);
			PDAFinfo= (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

			switch (*feature_data) 
			{
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
				break;
			}
			break;
    case SENSOR_FEATURE_GET_VC_INFO:
        LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16) *feature_data);
        pvcinfo = (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));
        switch (*feature_data_32)
        {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1], sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2], sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        //case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        //default:
           // memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0], sizeof(struct SENSOR_VC_INFO_STRUCT));

            //break;
        }
        break;
		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
			LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n", *feature_data);
			//PDAF capacity enable or not, OV16880 only full size support PDAF
			switch (*feature_data) 
			{
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; // video & capture use same setting
				break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
				default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			}
			break;

		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
			LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
		default:
			break;
	}

	return ERROR_NONE;
}	/*	feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 BLACKJACK_TSP_OV16880_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	OV16880_MIPI_RAW_SensorInit	*/
