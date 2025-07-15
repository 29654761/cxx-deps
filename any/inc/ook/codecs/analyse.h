#ifndef __H264_ANALYSER_H__
#define __H264_ANALYSER_H__

typedef struct _h264_sps_vui
{
	int video_signal_type_present_flag;
} h264_sps_vui;

typedef struct _h264_sps_info
{
	int profile_idc;
	int level_idc;
	    
	int sps_id;
	
	int mb_width;
	int mb_height;
	int width;
	int height;
	
	int chroma_format_idc;
	int bit_depth_luma;
	int bit_depth_chroma;
	int transform_bypass;
	int log2_max_frame_num;
	int poc_type;
	int log2_max_poc_lsb;
	int ref_frame_count;
	
	int frame_mbs_only_flag;
	int mb_aff;
	int direct_8x8_inference_flag;

	int crop;
	int vui_parameters_present_flag;
	int timing_info_present_flag;
	int fixed_frame_rate_flag;

	unsigned int num_units_in_tick;
	unsigned int time_scale;

	h264_sps_vui vui;
	
	int crop_l;
	int crop_r;
	int crop_t;
	int crop_b;
	
} h264_sps_info;

typedef struct _h264_pps_info
{
    int id;
    int sps_id;

    int cabac;

    int pic_order;
    int num_slice_groups;

    int num_ref_idx_l0_default_active;
    int num_ref_idx_l1_default_active;

    int weighted_pred;
    int weighted_bipred;

    int pic_init_qp;
    int pic_init_qs;

    int chroma_qp_index_offset;

    int deblocking_filter_control;
    int constrained_intra_pred;
    int redundant_pic_cnt;

    int transform_8x8_mode;

    int cqm_preset;
    
} h264_pps_info;

typedef struct _h264_slice_info
{
	int first_mb_in_slice;
	int slice_type;
	int pps_id;
	int frame_num;
	int field_pic;
	int bottom_field;
	int poc_lsb;
	
} h264_slice_info;

int h264_analyse_sps(const unsigned char * bits, 
					 unsigned int bitslen, 
					 h264_sps_info * info, 
					 unsigned int printmask);

int h264_analyse_pps(const unsigned char * bits, 
					 unsigned int bitslen, 
					 h264_pps_info * info, 
					 unsigned int printmask);
					 					 
int h264_analyse_slice(const unsigned char * bits, 
					   unsigned int bitslen, 
					   h264_slice_info * info, 
					   h264_sps_info   * sps,
					   unsigned int printmask);
					   
int h264_set_frame_num(const unsigned char * bits, 
					   unsigned int bitslen,
					   h264_slice_info * info,
					   unsigned int printmask);
#endif
