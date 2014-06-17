
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
	uint32 i, size, left, right, top, bottom;
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
		/* chroma format idx */
		if (exp_golomb_ue(&bb) == 3) {
			skip_bits(&bb, 1);
		}
		/* bit depth luma minus8 */
		exp_golomb_ue(&bb);
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
	/* log2_max_frame_num_minus4 */
	printf("=================================================================\n");
	int f1 = exp_golomb_ue(&bb);
	printf("%s,%d: f1=0x%x\n", __FUNCTION__,__LINE__,f1);
	printf("%s,%d: f1=%d\n", __FUNCTION__,__LINE__,f1);
	printf("bb.read_bits = %d\n",bb.read_bits);
	printf("bb.current = %0x\n",bb.current[0]);
	printf("=================================================================\n");
	//printf("%s,%d: exp_golomb_ue=0x%x\n", __FUNCTION__,__LINE__,get_bits(&bb, 8));



	/* pic_order_cnt_type */
	//图像播放顺序类型，两种类型

	printf("=================================================================\n");
	pic_order_cnt_type = exp_golomb_ue(&bb);
	printf("pic_order_cnt_type = %d\n",pic_order_cnt_type);
	printf("bb.read_bits = %d\n",bb.read_bits);
	printf("bb.current = %0x\n",bb.current[0]);
	printf("=================================================================\n");
	if (pic_order_cnt_type == 0) {
		/* log2_max_pic_order_cnt_lsb_minus4 */
		printf("=================================================================\n");
		int log2_max_pic_order_cnt_lsb_minus4 = exp_golomb_ue(&bb);
		printf("log2_max_pic_order_cnt_lsb_minus4 = %d\n",log2_max_pic_order_cnt_lsb_minus4);
		printf("bb.read_bits = %d\n",bb.read_bits);
		printf("bb.current = %0x\n",bb.current[0]);
		printf("=================================================================\n");
	}
	else if (pic_order_cnt_type == 1) {
		/* delta_pic_order_always_zero_flag */
		skip_bits(&bb, 1);
		/* offset_for_non_ref_pic */
		exp_golomb_se(&bb);
		/* offset_for_top_to_bottom_field */
		exp_golomb_se(&bb);
		size = exp_golomb_ue(&bb);
		for (i = 0; i < size; i++) {
			/* offset_for_ref_frame */
			exp_golomb_se(&bb);
		}
	}


	/* num_ref_frames */
	//参考帧个数
	printf("=================================================================\n");
	int num_ref_frames = exp_golomb_ue(&bb);
	printf("num_ref_frames = %d\n",num_ref_frames);
	printf("bb.read_bits = %d\n",bb.read_bits);
	printf("bb.current = %0x\n",bb.current[0]);
	printf("=================================================================\n");

	/* gaps_in_frame_num_value_allowed_flag */
	skip_bits(&bb, 1);

	printf("=================================================================\n");
	/* pic_width_in_mbs */
	width_in_mbs = exp_golomb_ue(&bb) + 1;
	printf("width_in_mbs = %d\n",width_in_mbs);
	printf("bb.read_bits = %d\n",bb.read_bits);
	printf("bb.current = %0x\n",bb.current[0]);


	printf("=================================================================\n");
	/* pic_height_in_map_units */
	height_in_map_units = exp_golomb_ue(&bb) + 1;
	printf("height_in_map_units = %d\n",height_in_map_units);
	printf("bb.read_bits = %d\n",bb.read_bits);
	printf("bb.current = %0x\n",bb.current[0]);

	printf("=================================================================\n");

	/* frame_mbs_only_flag */
	// 本句法元素等于 0 时表示本序列中所有图像的编码模式都是帧，没有其他编码模式存在；
	// 本句法元素等于 1 时  ，表示本序列中图像的编码模式可能是帧，也可能是场或帧场自适应，某个图像具体是哪一种要由其他句法元素决定
	//
	frame_mbs_only_flag = get_bit(&bb);
	if (!frame_mbs_only_flag) {
		/* mb_adaptive_frame_field */
		skip_bits(&bb, 1);
	}


	/* direct_8x8_inference_flag */
	skip_bits(&bb, 1);
	/* frame_cropping */
	//解码器是否要将图像裁剪后输出，如果是，后面为裁剪的左右上下的宽度
	//
	left = right = top = bottom = 0;
	if (get_bit(&bb)) {
		left = exp_golomb_ue(&bb) * 2;
		right = exp_golomb_ue(&bb) * 2;
		top = exp_golomb_ue(&bb) * 2;
		bottom = exp_golomb_ue(&bb) * 2;
		if (!frame_mbs_only_flag) {
			top *= 2;
			bottom *= 2;
		}
	}
	/* width */
	*width = width_in_mbs * 16 - (left + right);
	/* height */
	*height = height_in_map_units * 16 - (top + bottom);
	if (!frame_mbs_only_flag) {
		*height *= 2;
	}
}


int main() 
{

	int width=0,height=0;
	char spsbuf_cif[10] = {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x42 ,0xC1 ,0x2C ,0x80};
	char spsbuf_vga[10] = {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x41 ,0x40 ,0x7B ,0x20};
	char spsbuf_qvga[10] = {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x42 ,0x83 ,0xF2};
	char spsbuf_qcif[10] = {0x67 ,0x42 ,0xE0 ,0x0A ,0x89 ,0x95 ,0x45 ,0x89 ,0xC8};
	char spsbuf_tmp[24] = {0x67 ,0x64 ,0x00 ,0x1E ,0xAC ,0x6D ,0x01 ,0xA8 ,0x7B,0x42,0x00,0X00,0x03,0x00,0x02,0x00,0x00,0x03,0x00,0x51,0x1E,0x24,0x4D,0x40};

	/*parse_sps(spsbuf_cif, 10, &width, &height);*/
	/*printf("video width: %d\n", width);*/
	/*printf("video height: %d\n", height);*/

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



