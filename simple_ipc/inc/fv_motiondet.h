//============================================================================
//                                                                           
// GrianMedia Technology Coporation 
//                                
//----------------------------------------------------------------------------
// File:        motion_detection.h                                                                                                             
//============================================================================

#ifndef _FV_MOTION_DETCTION_
#define _FV_MOTION_DETCTION_

#define INTER_MODE		0
#define INTER4V_MODE	2
#define INTRA_MODE		3

struct fv_md{
    unsigned int mb_width;
    unsigned int mb_height;

    int *mvs_x; //adaptive mvs x
    int *mvs_y; //adaptive mvs y

    unsigned char *l_mvs; //PMB flag
    unsigned char *IMB_flag; //IMB number
    unsigned char *l_mvsi; //IMB flag

    unsigned int sad16_cur;
    unsigned int sad16_pre;
    unsigned int sad16_Th; //adaptive sad threshold

    int sad_mask;
} fv_md;

union VECTORmv{
    unsigned int u32num;
    struct{
        short s16y;
        short s16x;
    } vec;
};

typedef struct{
    short quant; /**< This variable indicates the macroblock level quantization value (1 ~ 31)*/
    short mode;
    /**< This variable indicates the mode of macroblock.
     *   Possible values are :
     *   <ul>
     *     <li> == INTER_MODE   : Indicate that this macroblock is encoded in INTER mode
     *                                   with 1MV.
     *     <li> == INTER4V_MODE : Indicate that this macroblock is encoded in INTER mode
     *                                   with 4MV.
     *     <li> == INTRA_MODE   : Indicate that this macroblock is encoded in INTRA mode.
     *   </ul>
     *
     */
    union VECTORmv mvs[4];
    /*
     *   - when mode = INTER_MODE :
     *			mvs[n]
     *			--n = 0 ~ 2: invalid.
     *			--n = 3: indicates the motion vector of whole macroblock.
     *   - when mode = INTER4V_MODE :
     *			mvs[n]
     *			--n = 0 ~3: indicates the motion vector of block n within this macroblock.
     *   - when mode = INTRA_MODE :
     *			mvs[n]
     *			--n = 0 ~ 3: invalid.
     */
    unsigned int sad16; // SAD value for inter-VECTOR
}

//-------------------------------------------------------------------------

MBK_INFO;

struct md_cfg{
    int interlace_mode; //interlace mode (1: interlaced; 0: non-interlaced)
    unsigned int usesad; //use sad info flag 
    unsigned int sad_offset;
    unsigned char *mb_cell_en;
    unsigned char *mb_cell_th;
    unsigned char mb_time_th; //successive N IMB regards as motion block (default: 3)
    unsigned char rolling_prot_th; //(default: 30)
    unsigned int alarm_th;
} md_cfg;


struct md_res{
    unsigned int active_num;
    unsigned char *active_flag;
} md_res;

//-----------------------------------------------------------------------------
int fv_motion_info_init(struct md_cfg *md_param, struct md_res *active, struct fv_md *fvmd,
                        unsigned int mb_width, unsigned int mb_height);
int fv_motion_detection(MBK_INFO *mb_array, struct md_cfg *md_param, struct md_res *active, struct
                        fv_md *fvmd);
int fv_motion_info_end(struct md_cfg *md_param, struct md_res *active, struct fv_md *fvmd);
void fv_do_noise_filtering(unsigned char *lmvs, unsigned int mbwidth, unsigned int mbheight);
void fv_do_IMB_filtering(unsigned char *lmvs, unsigned int mbwidth, unsigned int mbheight);

#endif
