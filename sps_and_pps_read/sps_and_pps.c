
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

typedef uint8_t byte, uint8, uint8_bitmask;
typedef uint16_t uint16, uint16_be, uint16_le;
typedef int16_t sint16, sint16_be, sint16_le;
typedef uint32_t uint32, uint32_be, uint32_le;
typedef int32_t sint32, sint32_be, sint32_le;
typedef struct __uint24 {
	uint8 b[3];
} uint24, uint24_be, uint24_le;
typedef struct __bit_buffer {
	byte * start;   // buf 开始
	size_t size;    // buf size
	byte * current; // 当前buf位置
	uint8 read_bits;// 当前buf的第几个bit
} bit_buffer;
static void skip_bits(bit_buffer * bb, size_t nbits) {
	bb->current = bb->current + ((nbits + bb->read_bits) / 8);
	bb->read_bits = (uint8)((bb->read_bits + nbits) % 8);
}

/**

 * 函数static uint8 get_bit(bit_buffer* bb)的功能是读取bb的某个字节的每一位，然后返回，实现很简单，这里略去。

*/
static uint8 get_bit(bit_buffer* bb)
{ // andy
	uint8 result = *bb->current;
	result >>= 7-bb->read_bits; 
	result  &= 0x01;
	skip_bits(bb,1);
	return result;
}

/**

 * 函数static uint32 get_bits(bit_buffer* bb,size_t nbits)的功能相当于连续读取bb的nbits位，若nbits是8的整数倍，则相当于读取几个字节

 *由uint32可知，nbits的取值范围为0~32，实现很简单，这里略去

*/
static uint32 get_bits(bit_buffer* bb,size_t nbits)
{

	// 67        42         E0         0A        89 
	// 0110 0111 0100 0010  1110 0000  0000 1010 1000 1001


	uint32 result = 0;
	uint8 tmp = 0;
	int i = 0;
	for(i=0;i<nbits;i++)
	{
		tmp = get_bit(bb);
		result <<= 1;
		result += tmp;
		printf("%d ",tmp);
	}

	return result;

}

//转载请注明出处：山水间博客，http://blog.csdn.net/linyanwen99/article/details/8260199

static uint32 exp_golomb_ue(bit_buffer * bb) {
	uint8 bit, significant_bits;
	significant_bits = 0;
	bit = get_bit(bb);
	while (bit == 0) {
		significant_bits++;
		bit = get_bit(bb);
	}
	return (1 << significant_bits) + get_bits(bb, significant_bits) - 1;
}
static sint32 exp_golomb_se(bit_buffer * bb) {
	sint32 ret;
	ret = exp_golomb_ue(bb);
	if ((ret & 0x1) == 0) {
		return -(ret >> 1);
	}
	else {
		return (ret + 1) >> 1;
	}
}
static void parse_scaling_list(uint32 size, bit_buffer * bb) {
	uint32 last_scale, next_scale, i;
	sint32 delta_scale;
	last_scale = 8;
	next_scale = 8;
	for (i = 0; i < size; i++) {
		if (next_scale != 0) {
			delta_scale = exp_golomb_se(bb);
			next_scale = (last_scale + delta_scale + 256) % 256;
		}
		if (next_scale != 0) {
			last_scale = next_scale;
		}
	}
}
/**
  Parses a SPS NALU to retrieve video width and height
  */
// {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x42 ,0xC1 ,0x2C ,0x80}
//  0100 0010 1110 0000
static void parse_sps(byte * sps, size_t sps_size, uint32 * width, uint32 * height) {
	bit_buffer bb;
	uint32 profile, pic_order_cnt_type, width_in_mbs, height_in_map_units;
	uint32 i, size, crop_left, crop_right, crop_top, crop_bottom;
	uint8 frame_mbs_only_flag;


	bb.start = sps;
	bb.size = sps_size;
	bb.current = sps;
	bb.read_bits = 0;
	printf("---------------------\n");
	uint8 tmp = 0;
	for(i=0;i<sps_size*8;i++)
	{
		tmp = get_bit(&bb);
		if(i%4)
			printf("%d",tmp);
		else
			printf(" %d",tmp);
	}
	printf("\n");
	bb.current = sps;
	bb.read_bits = 0;



	//0x67 ,0x64 ,0x00 ,0x1E ,0xAC ,0x6D ,0x01 ,0xA8 ,0x7B,0x42,0x00,0X00,0x03,0x00,0x02,0x00,0x00,0x03,0x00,0x51,0x1E,0x24,0x4D,0x40
	/* skip first byte, since we already know we're parsing a SPS */
	skip_bits(&bb, 8);

	//H.264编码体系定义了4种不同的Profile(类)：(其实就是画质的不同)
	//Baseline Profile(基线类)
	/*
	 *提供I/P帧，仅支持progressive(逐行扫描)和CAVLC
	 *
	 */

	//Main Profile(主要类)
	/*
	 * 提供I/P/B帧，支持progressive(逐行扫描)和interlaced(隔行扫描)，提供CAVLC或CABAC
	 */


	//Extended Profile(扩展类)
	/*
	 * 提供I/P/B/SP/SI帧，仅支持progressive(逐行扫描)和CAVLC
	 */


	//High Profile(高端类)
	/*
	 *也就是FRExt）在Main Profile基础上新增：
	 		8x8 intra prediction(8x8 帧内预测), 
			custom quant(自定义量化), 
			lossless video coding(无损视频编码), 更多的yuv格式（4:4:4...）/
	 *
	 * */
	/*
	 * profile 值取值
	Options:
	66  Baseline
        77  Main
	88  Extended
	100  High (FRExt)
	110  High 10 (FRExt)
	122  High 4:2:2 (FRExt)
	144  High 4:4:4 (FRExt)
	*/
	printf("-----------------------------------------------\n");
	/* get profile idc */
	profile = get_bits(&bb, 8);
	printf("profile  idc= %d\n", profile);


	//constrained_set0_flag
	//1  说明码流应该遵循基线profile(Baseline profile)的所有约束.
	//0  说明码流不一定要遵循基线profile的所有约束	
	int constrained_set0_flag = get_bits(&bb, 1); 
	printf("constrained_set0_flag= %d\n", constrained_set0_flag);
	
	//constrained_set1_flag
	//1  说明码流应该遵循主profile(Main profile)的所有约束.
	//0  说明码流不一定要遵循主profile的所有约束。
	int constrained_set1_flag = get_bits(&bb, 1); 
	printf("constrained_set1_flag= %d\n", constrained_set1_flag);

	//constrained_set2_flag
	//1   说明码流应该遵循扩展profile(Extended profile)的所有约束.
	//0   说明码流不一定要遵循扩展profile的所有约束。
	int constrained_set2_flag = get_bits(&bb, 1); 
	printf("constrained_set2_flag= %d\n", constrained_set2_flag);
	
	//constrained_set3_flag
	//1   说明码流应该遵循High Profile的所有约束
	//0   说明码流比一定要遵循High Profile的所有约束
	int constrained_set3_flag = get_bits(&bb, 1); 
	printf("constrained_set3_flag= %d\n", constrained_set3_flag);

	printf("-----------------------------------------------\n");
	//reserved_zero_4bits
	//保留的四位 为 0
	int reserved_zero_4bits = get_bits(&bb, 4);
	printf("reserved_zero_4bits= %d\n", reserved_zero_4bits);
	printf("-----------------------------------------------\n");


	//level idc 8位 level 级别 见三种图
	/*
	options:
	10           1 (supports only QCIF format and below with 380160 samples/sec)
	11         1.1 (CIF and below. 768000 samples/sec)
	12         1.2 (CIF and below. 1536000 samples/sec)
	13         1.3 (CIF and below. 3041280 samples/sec)
	20           2 (CIF and below. 3041280 samples/sec)
	21         2.1 (Supports HHR formats. Enables Interlace support. 5068800 samples/sec)
	22         2.2 (Supports SD/4CIF formats. Enables Interlace support. 5184000 samples/sec)
	30           3 (Supports SD/4CIF formats. Enables Interlace support. 10368000 samples/sec)
	31         3.1(Supports 720p HD format. Enables Interlace support. 27648000 samples/sec)
	32         3.2(Supports SXGA format. Enables Interlace support. 55296000 samples/sec)
	40           4(Supports 2Kx1K format. Enables Interlace support. 62914560 samples/sec)
	41         4.1(Supports 2Kx1K format. Enables Interlace support. 62914560 samples/sec)
	42         4.2(Supports 2Kx1K format. Frame coding only. 125829120 samples/sec)
	50           5(Supports 3672x1536 format. Frame coding only. 150994944 samples/sec)
	51         5.1(Supports 4096x2304 format. Frame coding only. 251658240 samples/sec)
	*/
	int level_idc = get_bits(&bb, 8);
       	printf("level_idc= %d\n", level_idc);	
	printf("-----------------------------------------------\n");
		
	/* read sps id, first exp-golomb encoded value */
	//seq_parameter_set_id
	//指定了由图像参数集指明的序列参数集.
	//seq_parameter_set_id值应该是从0到31，包括0和31
	//注意：当可用的情况下，编码器应该在sps值不同的情况下使用不同的seq_parameter_set_id值，
	//而不是变化某一特定值的seq_parameter_set_id的参数集的语法结构中的值
	int  seq_parameter_set_id = exp_golomb_ue(&bb);   // sps id
	printf("seq_parameter_set_id= %d\n", seq_parameter_set_id);
	printf("-----------------------------------------------\n");

	if (profile == 100 || profile == 110 || profile == 122 || profile == 144) {
		//chroma_format_idc 取值是0 ~ 3，表示的色度采样结构
		//chroma_format_idc 色彩格式 
		//0                 单色
		//1                4:2:0 
		//2                4:2:2 
		//3                4:4:4 
		//chroma_format_idc
		if (exp_golomb_ue(&bb) == 3) {
			printf("-----------------------------------------------\n");
			//等于1 时， 应用8.5 节规定的残余颜色变换。
			//等于0时则不使用残余颜色变换。
			//当residual_colour_transform_flag 不存在时，默认其值为0。
			//residual_colour_transform_flag
			skip_bits(&bb, 1);
		}
		printf("-----------------------------------------------\n");
		//bit_depth_luma_minus8是指亮度队列样值的比特深度以及亮度量化参数范围的取值偏移QpBdOffsetY
		//如下所示：
		//BitDepthY = 8 + bit_depth_luma_minus8 (7-1)
		//QpBdOffsetY = 6 * bit_depth_luma_minus8 (7-2)
		//当bit_depth_luma_minus8 不存在时，应推定其值为0。bit_depth_luma_minus8 取值范围应该在0 到4 之间（包括0和4）。
		/* bit depth luma minus8 */
		exp_golomb_ue(&bb);
		printf("-----------------------------------------------\n");

		//

		/* bit depth chroma minus8 */
		exp_golomb_ue(&bb);
		/* Qpprime Y Zero Transform Bypass flag */
		skip_bits(&bb, 1);
		/* Seq Scaling Matrix Present Flag */
		if (get_bit(&bb)) {
			for (i = 0; i < 8; i++) {
				/* Seq Scaling List Present Flag */
				if (get_bit(&bb)) {
					parse_scaling_list(i < 6 ? 16 : 64, &bb);
				}
			}
		}
	}
	printf("-----------------------------------------------\n");

	//指定了变量MaxFrameNum的值,MaxFrameNum = 2^(log2_max_frame_num_minus4+4) 应该在0到12之间，包括0和12.
	/* log2_max_frame_num_minus4 */
	int f1 = exp_golomb_ue(&bb);
	printf("%s,%d: f1=0x%x\n", __FUNCTION__,__LINE__,f1);
	printf("%s,%d: f1=%d\n", __FUNCTION__,__LINE__,f1);
	//printf("%s,%d: exp_golomb_ue=0x%x\n", __FUNCTION__,__LINE__,get_bits(&bb, 8));


	printf("-----------------------------------------------\n");

	/* pic_order_cnt_type */
	//图像播放顺序类型，两种类型
	//pic_order_cnt_type指定了解码图像顺序的方法。pic_order_cnt_type的值是0,1,2。
	//
	//pic_order_cnt_type在当一个编码视频序列有如下限定时不为2
	//
	//a) 包含非参考帧的可访问单元，并紧接着一个包含非参考可访问单元
	//b) 两个可访问单元，它们分别包含两个场中的一个，它们一块儿组成了一个互补的非参考场对，被紧接着一个包括非参考图像的可访问单元。
	//c) 一个包含非参考场的可访问单元，并紧接着一个包含另一个非参考图像的可访问单元，它们不组成互补的非参考场对
	pic_order_cnt_type = exp_golomb_ue(&bb);
	printf("pic_order_cnt_type = %d\n",pic_order_cnt_type);
	printf("-----------------------------------------------\n");
	

	if (pic_order_cnt_type == 0) {
		//指出变量MaxPicOrderCntLsb的值，它是在解码过程中使用到的图像顺序计算值:
		//MaxPicOrderCntLsb = 2(log2_max_pic_order_cnt_lsb_minus4+4)
		//log2_max_pic_order_cnt_lsb_minus4的值为包括0和12以及它们之间的值
		/* log2_max_pic_order_cnt_lsb_minus4 */
		int log2_max_pic_order_cnt_lsb_minus4 = exp_golomb_ue(&bb);
		printf("log2_max_pic_order_cnt_lsb_minus4 = %d\n",log2_max_pic_order_cnt_lsb_minus4);
		printf("-----------------------------------------------\n");
	}
	else if (pic_order_cnt_type == 1) {
		//delta_pic_order_always_zero_flag等于1的时候表示
		//当delta_pic_order_cnt[0]和delta_pic_order_cnt[1]在序列的切片头中不存在并被认为是0。
		//delta_pic_order_always_zero_flag值等于0时表示
		//delta_pic_order_cnt[0]在序列的切片头中存在而delta_pic_order_cnt[1]可能在序列的切片头中存在。
		/* delta_pic_order_always_zero_flag */
		get_bits(&bb, 1);
		printf("-----------------------------------------------\n");

		//被用来计算一个非参考图像的图像顺序值。
		//offset_for_non_ref_pic值取值范围为(-2)^(31)到2^(31)-1，包括边界值。
		/* offset_for_non_ref_pic */
		exp_golomb_se(&bb);

		printf("-----------------------------------------------\n");
		//offset_for_top_to_bottom_field被用来计算一帧中的下场的图像顺序值。
		//offset_for_top_to_bottom_field值的取值范围为(-2)^(31)到(2)^(31)-1，包括边界值。
		/* offset_for_top_to_bottom_field */
		exp_golomb_se(&bb);
		printf("-----------------------------------------------\n");

		//num_ref_frames_in_pic_order_cnt_cycle
		size = exp_golomb_ue(&bb);
		for (i = 0; i < size; i++) {
			/* offset_for_ref_frame */
			exp_golomb_se(&bb);
		}
	}


	/* num_ref_frames */
	//参考帧个数
	printf("-----------------------------------------------\n");
	int num_ref_frames = exp_golomb_ue(&bb);
	printf("num_ref_frames = %d\n",num_ref_frames);
	printf("-----------------------------------------------\n");

	/* gaps_in_frame_num_value_allowed_flag */
	skip_bits(&bb, 1);
	printf("-----------------------------------------------\n");

	/* pic_width_in_mbs */ //pic_width_in_mbs_minus1 
	width_in_mbs = exp_golomb_ue(&bb) + 1;
	printf("width_in_mbs = %d\n",width_in_mbs);

	printf("-----------------------------------------------\n");

	/* pic_height_in_map_units */ //pic_height_in_map_units_minus1
	height_in_map_units = exp_golomb_ue(&bb) + 1;
	printf("height_in_map_units = %d\n",height_in_map_units);

	printf("-----------------------------------------------\n");

	/* frame_mbs_only_flag */
	// 本句法元素等于 0 时表示本序列中所有图像的编码模式都是帧，没有其他编码模式存在；
	// 本句法元素等于 1 时  ，表示本序列中图像的编码模式可能是帧，也可能是场或帧场自适应，某个图像具体是哪一种要由其他句法元素决定
	//
	frame_mbs_only_flag = get_bit(&bb);

	*width = width_in_mbs * 16;
	*height = height_in_map_units * 16 * (2 - frame_mbs_only_flag);

	if (!frame_mbs_only_flag) {
		//  mb_adaptiv_frame_field_flag /* 指明本序列是否是帧场自适应模式：
		//
		//  frame_mbs_only_flag=1，全部是帧
		//
		//  frame_mbs_only_flag=0， mb_adaptiv_frame_field_flag=0，帧场共存
		//
		//  frame_mbs_only_flag=0， mb_adaptiv_frame_field_flag=1，帧场自适应和场共存
		//
		/* mb_adaptive_frame_field */
		skip_bits(&bb, 1);
	}

	//用于指明B片的直接和skip模式下的运动矢量的计算方式
	/* direct_8x8_inference_flag */
	skip_bits(&bb, 1);

	printf("-----------------------------------------------\n");
	/* frame_cropping */
	//frame_cropping_flag
	//解码器是否要将图像裁剪后输出，如果是，后面为裁剪的左右上下的宽度
	//
	int frame_cropping_flag = 0;
	frame_cropping_flag = get_bit(&bb);
	crop_left = crop_right = crop_top = crop_bottom = 0;
	if (frame_cropping_flag) {
		//frame_crop_left_offset
		crop_left = exp_golomb_ue(&bb);
		//frame_crop_right_offset
		crop_right = exp_golomb_ue(&bb);
		//frame_crop_top_offset
		crop_top = exp_golomb_ue(&bb);
		//frame_crop_bottom_offset
		crop_bottom = exp_golomb_ue(&bb);
	}

	printf("-----------------------------------------------\n");
	/* width */

	*width = *width - 2* (crop_left + crop_right);
	/* height */
	if(frame_mbs_only_flag)
	{
		*height = *height - 2*(crop_top + crop_bottom);
	}
	else
	{
		*height = *height - 4*(crop_top + crop_bottom);
	}

	printf("-----------------------------------------------\n");
	//vui_parameters_present_flag
	//
	#define Extended_SAR 255

	int vui_parameters_present_flag = get_bit(&bb);
	if(vui_parameters_present_flag)
	{
		//TODO
		//aspect_ratio_info_present_flag 
		int aspect_ratio_info_present_flag = get_bit(&bb);
		if(aspect_ratio_info_present_flag)
		{
			int aspect_ratio_idc = get_bits(&bb, 8);
			if(aspect_ratio_idc == Extended_SAR)
			{
				int sar_width = get_bits(&bb, 16);
				int sar_height = get_bits(&bb, 16);
			}
		}
		
		int overscan_info_present_flag = get_bit(&bb);
		if(overscan_info_present_flag)
		{
			int overscan_appropriate_flag = get_bit(&bb);
		}

		int video_signal_type_present_flag = get_bit(&bb);
		if(video_signal_type_present_flag)
		{
			int video_format = get_bits(&bb, 3);
			int video_full_range_flag = get_bit(&bb);

			int colour_description_present_flag = get_bit(&bb);
			if(colour_description_present_flag)
			{
				int colour_primaries = get_bits(&bb, 8);
				int transfer_characteristics = get_bits(&bb, 8);
				int matrix_coefficients = get_bits(&bb, 8);
			}
		}


		int chroma_loc_info_present_flag = get_bit(&bb);
		if(chroma_loc_info_present_flag)
		{
			int chroma_sample_loc_type_top_field = exp_golomb_ue(&bb);
			int chroma_sample_loc_type_bottom_field = exp_golomb_ue(&bb);
		}

		int timing_info_present_flag = get_bit(&bb);
		if(timing_info_present_flag)
		{
			int num_units_in_tick = get_bits(&bb, 32);
			int time_scale = get_bits(&bb, 32);
			int fixed_frame_rate_flag = get_bit(&bb);
		} 


		int nal_hrd_parameters_present_flag = get_bit(&bb);
		if(nal_hrd_parameters_present_flag)
		{
			int cpb_cnt_minus1 = exp_golomb_ue(&bb);
			int bit_rate_scale = get_bits(&bb, 4);
			int cpb_size_scale = get_bits(&bb, 4);

			int SchedSelIdx = 0;
			int bit_rate_value_minus1[16];      //0 ue(v)
			int cpb_size_value_minus1[16];      //0 ue(v)
			int cbr_flag[16];                   //0 u(1)
			for(SchedSelIdx=0; SchedSelIdx<=cpb_cnt_minus1; SchedSelIdx++)
			{
				bit_rate_value_minus1[SchedSelIdx] = exp_golomb_ue(&bb);
				cpb_size_value_minus1[SchedSelIdx] = exp_golomb_ue(&bb);
				cbr_flag[SchedSelIdx] = get_bit(&bb);
			}


			int initial_cpb_removal_delay_length_minus1 = get_bits(&bb, 5);
			int cpb_removal_delay_length_minus1 = get_bits(&bb, 5);
			int dpb_output_delay_length_minus1 = get_bits(&bb, 5);
			int time_offset_length = get_bits(&bb, 5);
		}


		int vcl_hrd_parameters_present_flag = get_bit(&bb);
		if(vcl_hrd_parameters_present_flag)
		{
			int cpb_cnt_minus1 = exp_golomb_ue(&bb);
			int bit_rate_scale = get_bits(&bb, 4);
			int cpb_size_scale = get_bits(&bb, 4);
			
			
			int SchedSelIdx = 0;
			int bit_rate_value_minus1[16];      //0 ue(v)
			int cpb_size_value_minus1[16];      //0 ue(v)
			int cbr_flag[16];                   //0 u(1)

			for(SchedSelIdx=0; SchedSelIdx<=cpb_cnt_minus1; SchedSelIdx++)
			{
				bit_rate_value_minus1[SchedSelIdx] = exp_golomb_ue(&bb);
				cpb_size_value_minus1[SchedSelIdx] = exp_golomb_ue(&bb);
				cbr_flag[SchedSelIdx] = get_bit(&bb);
			}


			int initial_cpb_removal_delay_length_minus1 = get_bits(&bb, 5);
			int cpb_removal_delay_length_minus1 = get_bits(&bb, 5);
			int dpb_output_delay_length_minus1 = get_bits(&bb, 5);
			int time_offset_length = get_bits(&bb, 5);        
		}


		if(nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
		{
			int low_delay_hrd_flag = get_bit(&bb);
		}

		int pic_struct_present_flag = get_bit(&bb);

		int bitstream_restriction_flag = get_bit(&bb);
		if(bitstream_restriction_flag)
		{
			int motion_vectors_over_pic_boundaries_flag = get_bit(&bb);
			int max_bytes_per_pic_denom = exp_golomb_ue(&bb);
			int max_bits_per_mb_denom = exp_golomb_ue(&bb);
			int log2_max_mv_length_horizontal= exp_golomb_ue(&bb);
			int log2_max_mv_length_vertical = exp_golomb_ue(&bb);
			int num_reorder_frames = exp_golomb_ue(&bb);
			int max_dec_frame_buffering = exp_golomb_ue(&bb);
		}


	}
}

//pps
static void parse_pps(byte * pps, size_t pps_size) {
	bit_buffer bb;

	bb.start = pps;
	bb.size = pps_size;
	bb.current = pps;
	bb.read_bits = 0;
	printf("---------------------\n");
	uint8 tmp = 0;
	int  i = 0;
	for(i=0;i<pps_size*8;i++)
	{
		tmp = get_bit(&bb);
		if(i%4)
			printf("%d",tmp);
		else
			printf(" %d",tmp);
	}
	printf("\n");
	bb.current = pps;
	bb.read_bits = 0;

	
	

	/* skip first byte, since we already know we're parsing a PPS */
	skip_bits(&bb, 8);

	printf("-----------------------------------------------\n");
	int pic_parameter_set_id = exp_golomb_ue(&bb);
	int seq_parameter_set_id = exp_golomb_ue(&bb);

	int entropy_coding_mode_flag = get_bit(&bb);
	int pic_order_present_flag = get_bit(&bb);

	int num_slice_groups_minus1 = exp_golomb_ue(&bb);
	if(num_slice_groups_minus1 > 0)
	{
		int  slice_group_map_type = exp_golomb_ue(&bb);
		if(slice_group_map_type == 0)
		{
			int iGroup = 0;
			int run_length_minus1[32];
			for(iGroup=0; iGroup<=num_slice_groups_minus1; iGroup++)
			{
				run_length_minus1[iGroup] = exp_golomb_ue(&bb);
			}
		}
		else if(slice_group_map_type == 2)
		{
			int iGroup = 0;
			int top_left[32];
			int bottom_right[32];
			for(iGroup=0; iGroup<num_slice_groups_minus1; iGroup++)
			{
				top_left[iGroup] = exp_golomb_ue(&bb);
				bottom_right[iGroup] = exp_golomb_ue(&bb);
			}
		}
		else if(slice_group_map_type == 3 \
				||slice_group_map_type == 4\
				||slice_group_map_type == 5)
		{
			int slice_group_change_direction_flag = get_bit(&bb);
			int slice_group_change_rate_minus1 = exp_golomb_ue(&bb);
		}
		else if(slice_group_map_type == 6)
		{
			int pic_size_in_map_units_minus1 = exp_golomb_ue(&bb);
			int slice_group_id[32];
			for(i=0; i<pic_size_in_map_units_minus1; i++)
			{
				/*这地方可能有问题，对u(v)理解偏差*/
				slice_group_id[i] = exp_golomb_ue(&bb);
			}
		}
	}


	int num_ref_idx_10_active_minus1 = exp_golomb_ue(&bb);
	int num_ref_idx_11_active_minus1 = exp_golomb_ue(&bb);
	int weighted_pred_flag = get_bit(&bb);
	int weighted_bipred_idc = get_bits(&bb, 2);
	int pic_init_qp_minus26 = exp_golomb_se(&bb); /*relative26*/
	int pic_init_qs_minus26 = exp_golomb_se(&bb); /*relative26*/
	int chroma_qp_index_offset = exp_golomb_se(&bb);
	int deblocking_filter_control_present_flag = get_bit(&bb);
	int constrained_intra_pred_flag = get_bit(&bb);
	int redundant_pic_cnt_present_flag = get_bit(&bb);


	/*pps 解析未完成
	 *     * more_rbsp_data()不知如何实现，时间原因
	 *         * 暂时搁置，没有深入。FIXME: 
	 *             */
	/*TODO*/


}


int main() 
{

	int width=0,height=0;
	char spsbuf_cif[10] = {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x42 ,0xC1 ,0x2C ,0x80};
	char spsbuf_vga[10] = {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x41 ,0x40 ,0x7B ,0x20};
	char spsbuf_qvga[10] = {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x42 ,0x83 ,0xF2};
	char spsbuf_qcif[10] = {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x45 ,0x89 ,0xC8};
	char spsbuf_tmp[24] = {0x67 ,0x64 ,0x00 ,0x1E ,0xAC ,0x6D ,0x01 ,0xA8 ,0x7B,0x42,0x00,0X00,0x03,0x00,0x02,0x00,0x00,0x03,0x00,0x51,0x1E,0x24,0x4D,0x40};

	char h264pps[4] ={0x68,0xCE,0x38,0x80};

	parse_sps(spsbuf_cif, 10, &width, &height);
	printf("video width: %d\n", width);
	printf("video height: %d\n", height);

#if 0
	parse_sps(spsbuf_vga, 10, &width, &height);
	printf("video width: %d\n", width);
	printf("video height: %d\n", height);

	parse_sps(spsbuf_qvga, 10, &width, &height);
	printf("video width: %d\n", width);
	printf("video height: %d\n", height);

	parse_sps(spsbuf_qcif, 10, &width, &height);
	printf("video width: %d\n", width);
	printf("video height: %d\n", height);
#endif
	parse_sps(spsbuf_tmp, 24, &width, &height);
	printf("video width: %d\n", width);
	printf("video height: %d\n", height);

	return 0;
}



