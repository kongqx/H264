
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
	/* skip first byte, since we already know we're parsing a SPS */
	skip_bits(&bb, 8);
	/* get profile */
	profile = get_bits(&bb, 8);
	printf("%s,%d: profile=0x%x %d\n", __FUNCTION__,__LINE__,profile,profile);
	/* skip 4 bits + 4 zeroed bits + 8 bits = 32 bits = 4 bytes */
	skip_bits(&bb, 16);
	/* read sps id, first exp-golomb encoded value */
	printf("=================================================================\n");
	exp_golomb_ue(&bb);   // sps id

	printf("bb.read_bits = %d\n",bb.read_bits);
	printf("bb.current = %0x\n",bb.current[0]);
	printf("=================================================================\n");

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



