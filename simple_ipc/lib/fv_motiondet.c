//============================================================================
//                                                                           
// GrianMedia Technology Coporation 
//                                
//----------------------------------------------------------------------------
// File:        fv_motiondet.c                                                         
// Description: motion detection api functions of mp4.                                                                                          
// Functions:   fv_motion_info_init() 
//              fv_motion_detection()
//              fv_motion_info_end()    
//              fv_do_noise_filtering()
//              fv_do_IMB_filtering()                                                  
//============================================================================

//============================================================================
//  Include File
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "fv_motiondet.h"

/*
FILE *md_out;
#define OUTPUT_FILE "/mnt/nfs/pattern/motion_dat"
*/

#define abs_fv(X)    (((X)>0)?(X):-(X))
//***************************************************************************
//   API                              
//****************************************************************************
int fv_motion_info_init(struct md_cfg *md_param, struct md_res *active, struct fv_md *fvmd,
                        unsigned int mb_width, unsigned int mb_height){

    fvmd->mb_width = mb_width;
    fvmd->mb_height = mb_height;

    printf("\nMD_INIT (%d x %d).\n", mb_width, mb_height);

    fvmd->mvs_x = NULL;
    fvmd->mvs_y = NULL;
    fvmd->l_mvs = NULL;
    fvmd->IMB_flag = NULL;
    fvmd->l_mvsi = NULL;
    active->active_flag = NULL; //I and P MB active
    md_param->mb_cell_en = NULL;
    md_param->mb_cell_th = NULL;

    fvmd->mvs_x = (int*)malloc(sizeof(int) *mb_width * mb_height);
    fvmd->mvs_y = (int*)malloc(sizeof(int) *mb_width * mb_height);
    fvmd->l_mvs = (unsigned char*)malloc(sizeof(unsigned char) *mb_width * mb_height);
    fvmd->IMB_flag = (unsigned char*)malloc(sizeof(unsigned char) *mb_width * mb_height);
    fvmd->l_mvsi = (unsigned char*)malloc(sizeof(unsigned char) *mb_width * mb_height);
    active->active_flag = (unsigned char*)malloc(sizeof(unsigned char) *mb_width * mb_height);
    md_param->mb_cell_en = (unsigned char*)malloc(sizeof(unsigned char) *mb_width * mb_height);
    md_param->mb_cell_th = (unsigned char*)malloc(sizeof(unsigned char) *mb_width * mb_height);

    if ((fvmd->mvs_x == NULL) || (fvmd->mvs_y == NULL))
        return 1;

    if ((fvmd->l_mvs == NULL) || (fvmd->IMB_flag == NULL) || (fvmd->l_mvsi == NULL))
        return 1;

    if (active->active_flag == NULL)
        return 1;

    if ((md_param->mb_cell_en == NULL) || (md_param->mb_cell_th == NULL))
        return 1;

    memset(fvmd->mvs_x, 0, (sizeof(int) *mb_width * mb_height));
    memset(fvmd->mvs_y, 0, (sizeof(int) *mb_width * mb_height));
    memset(fvmd->l_mvs, 0, (sizeof(unsigned char) *mb_width * mb_height));
    memset(fvmd->IMB_flag, 0, (sizeof(unsigned char) *mb_width * mb_height));
    memset(fvmd->l_mvsi, 0, (sizeof(unsigned char) *mb_width * mb_height));
    memset(active->active_flag, 0, (sizeof(unsigned char) *mb_width * mb_height));
    memset(md_param->mb_cell_en, 0, (sizeof(unsigned char) *mb_width * mb_height));
    memset(md_param->mb_cell_th, 0, (sizeof(unsigned char) *mb_width * mb_height));

    fvmd->sad16_cur = 0;
    fvmd->sad16_pre = 0;
    fvmd->sad16_Th = 0;
    fvmd->sad_mask = 0;
    md_param->usesad = 0;
    md_param->sad_offset = 50;
    md_param->interlace_mode = 0;
/*
    md_out = fopen(OUTPUT_FILE, "w");
    if (md_out) fclose(md_out);
*/
    return 0;
} 

//----------------------------------------------------------------------------
int fv_motion_info_end(struct md_cfg *md_param, struct md_res *active, struct fv_md *fvmd){
    printf("\nMD_END.\n");
   
    if (fvmd->mvs_x != NULL)
        free(fvmd->mvs_x);

    if (fvmd->mvs_y != NULL)
        free(fvmd->mvs_y);

    if (fvmd->l_mvs != NULL)
        free(fvmd->l_mvs);

    if (fvmd->IMB_flag != NULL)
        free(fvmd->IMB_flag);

    if (fvmd->l_mvsi != NULL)
        free(fvmd->l_mvsi);

    if (active->active_flag != NULL)
        free(active->active_flag);

    if (md_param->mb_cell_en != NULL)
        free(md_param->mb_cell_en);

    if (md_param->mb_cell_th != NULL)
        free(md_param->mb_cell_th);

    return 0;
} 

//----------------------------------------------------------------------------
int fv_motion_detection(MBK_INFO *mb_array, struct md_cfg *md_param, struct md_res *active, struct
                        fv_md *fvmd){
    unsigned int x_pos, y_pos;
    unsigned int sad16 = 0, sad16_odd_field = 0;
    int mv_x = 0, mv_y = 0;
    int mv_odd_x, mv_odd_y;
    unsigned int mv_thd = 0;
    unsigned int mv_even_field = 0, mv_odd_field = 0;

    MBK_INFO *mb, *mb_odd_field;

    int sad_diff = 0;
    unsigned int mbwidth, mbheight;

    active->active_num = 0;
    mbwidth = fvmd->mb_width;
    mbheight = fvmd->mb_height;
/*
    md_out = fopen(OUTPUT_FILE, "a");
    if (md_out) {
        fwrite(mb_array, 1, mbwidth*(mbheight-1)*sizeof(MBK_INFO), md_out);
        fclose(md_out);
    }
*/
    if (md_param->interlace_mode){
        for (y_pos = 0; y_pos < ((mbheight-1) >> 1); y_pos++){
            for (x_pos = 0; x_pos < mbwidth; x_pos++){

                mb = mb_array+(y_pos *mbwidth+x_pos);
                mv_x = mb->mvs[3].vec.s16x;
                mv_y = mb->mvs[3].vec.s16y;
                mv_even_field = (mv_x *mv_x+mv_y * mv_y);

                if (md_param->usesad){
                    sad16 = mb->sad16;
                } 

                mb_odd_field = mb_array+((y_pos+(mbheight >> 1)) *mbwidth+x_pos);
                mv_odd_x = mb_odd_field->mvs[3].vec.s16x;
                mv_odd_y = mb_odd_field->mvs[3].vec.s16y;
                mv_odd_field = (mv_odd_x *mv_odd_x+mv_odd_y * mv_odd_y);

                if (md_param->usesad){
                    sad16_odd_field = mb_odd_field->sad16;
                }

                //even_field
                if (mb->mode == INTRA_MODE)
                { //I MB
                    fvmd->mvs_x[((y_pos << 1) *mbwidth+x_pos)] = (fvmd->mvs_x[((y_pos << 1)*mbwidth+x_pos)] << 1) / 3;
                    fvmd->mvs_y[((y_pos << 1) *mbwidth+x_pos)] = (fvmd->mvs_y[((y_pos << 1)*mbwidth+x_pos)] << 1) / 3;
                    fvmd->IMB_flag[((y_pos << 1) *mbwidth+x_pos)] += 1;
                }
                else if ((mb->mode == INTER_MODE) || (mb->mode == INTER4V_MODE))
                { // P MB 
                    fvmd->mvs_x[((y_pos << 1) *mbwidth+x_pos)] = ((fvmd->mvs_x[((y_pos << 1)*mbwidth+x_pos)] << 1)+mv_x) / 3;
                    fvmd->mvs_y[((y_pos << 1) *mbwidth+x_pos)] = ((fvmd->mvs_y[((y_pos << 1)*mbwidth+x_pos)] << 1)+mv_y) / 3;
                    fvmd->IMB_flag[((y_pos << 1) *mbwidth+x_pos)] = 0;

                    //if (mv_even_field == 0 && (md_param->usesad))
                    if (mv_even_field <= 2 && (md_param->usesad))   // Andrew
                        fvmd->sad16_cur = ((fvmd->sad16_cur *3)+sad16) >> 2;
                }

                mv_thd = md_param->mb_cell_th[((y_pos << 1) *mbwidth+x_pos)];

                if (((fvmd->mvs_x[((y_pos << 1) *mbwidth+x_pos)] *fvmd->mvs_x[((y_pos << 1)*mbwidth+x_pos)]+
                    fvmd->mvs_y[((y_pos << 1) *mbwidth+x_pos)] *fvmd->mvs_y[((y_pos << 1) *mbwidth+x_pos)]) > mv_thd))
                    fvmd->l_mvs[((y_pos << 1) *mbwidth+x_pos)] = 1;
                else
                    fvmd->l_mvs[((y_pos << 1) *mbwidth+x_pos)] = 0;

                //odd_field
                if (mb_odd_field->mode == INTRA_MODE)
                { //I MB
                    fvmd->mvs_x[(((y_pos << 1)+1) *mbwidth+x_pos)] = (fvmd->mvs_x[(((y_pos << 1)+1)*mbwidth+x_pos)] << 1) / 3;
                    fvmd->mvs_y[(((y_pos << 1)+1) *mbwidth+x_pos)] = (fvmd->mvs_y[(((y_pos << 1)+1)*mbwidth+x_pos)] << 1) / 3;
                    fvmd->IMB_flag[(((y_pos << 1)+1) *mbwidth+x_pos)] += 1;
                }
                else if ((mb->mode == INTER_MODE) || (mb->mode == INTER4V_MODE))
                { // P MB 
                    fvmd->mvs_x[(((y_pos << 1)+1) *mbwidth+x_pos)] = ((fvmd->mvs_x[(((y_pos << 1)+1)*mbwidth+x_pos)] << 1)+mv_odd_x) / 3;
                    fvmd->mvs_y[(((y_pos << 1)+1) *mbwidth+x_pos)] = ((fvmd->mvs_y[(((y_pos << 1)+1)*mbwidth+x_pos)] << 1)+mv_odd_y) / 3;
                    fvmd->IMB_flag[(((y_pos << 1)+1) *mbwidth+x_pos)] = 0;

                    if (mv_odd_field == 0 && (md_param->usesad))
                        fvmd->sad16_cur = ((fvmd->sad16_cur *3)+sad16_odd_field) >> 2;
                }

                mv_thd = md_param->mb_cell_th[(((y_pos << 1)+1) *mbwidth+x_pos)];

                if (((fvmd->mvs_x[(((y_pos << 1)+1) *mbwidth+x_pos)] *fvmd->mvs_x[(((y_pos << 1)+1)*mbwidth+x_pos)]+
                    fvmd->mvs_y[(((y_pos << 1)+1) *mbwidth+x_pos)] *fvmd->mvs_y[(((y_pos << 1)+1) *mbwidth+x_pos)]) > mv_thd))
                    fvmd->l_mvs[(((y_pos << 1)+1) *mbwidth+x_pos)] = 1;
                else
                    fvmd->l_mvs[(((y_pos << 1)+1) *mbwidth+x_pos)] = 0;

                if (md_param->usesad)
                    fvmd->sad16_Th = fvmd->sad16_cur+md_param->sad_offset;

                if (md_param->usesad){
                    if ((fvmd->IMB_flag[((y_pos << 1) *mbwidth+x_pos)] >= md_param->mb_time_th) && (sad16 >= fvmd->sad16_Th))
                        fvmd->l_mvsi[((y_pos << 1) *mbwidth+x_pos)] = md_param->mb_time_th;
                    else
                        fvmd->l_mvsi[((y_pos << 1) *mbwidth+x_pos)] = 0;

                    if ((fvmd->IMB_flag[(((y_pos << 1)+1) *mbwidth+x_pos)] >= md_param->mb_time_th)&& (sad16 >= fvmd->sad16_Th))
                        fvmd->l_mvsi[(((y_pos << 1)+1) *mbwidth+x_pos)] = md_param->mb_time_th;
                    else
                        fvmd->l_mvsi[(((y_pos << 1)+1) *mbwidth+x_pos)] = 0;

                    if (fvmd->l_mvs[((y_pos << 1) *mbwidth+x_pos)] && (sad16 < fvmd->sad16_Th))
                        fvmd->l_mvs[((y_pos << 1) *mbwidth+x_pos)] = 0;

                    if (fvmd->l_mvs[(((y_pos << 1)+1) *mbwidth+x_pos)] && (sad16 < fvmd->sad16_Th))
                        fvmd->l_mvs[(((y_pos << 1)+1) *mbwidth+x_pos)] = 0;


                }
                else{
                    if (fvmd->IMB_flag[((y_pos << 1) *mbwidth+x_pos)] >= md_param->mb_time_th)
                        fvmd->l_mvsi[((y_pos << 1) *mbwidth+x_pos)] = md_param->mb_time_th;
                    else
                        fvmd->l_mvsi[((y_pos << 1) *mbwidth+x_pos)] = 0;
                    if (fvmd->IMB_flag[(((y_pos << 1)+1) *mbwidth+x_pos)] >= md_param->mb_time_th)
                        fvmd->l_mvsi[(((y_pos << 1)+1) *mbwidth+x_pos)] = md_param->mb_time_th;
                    else
                        fvmd->l_mvsi[(((y_pos << 1)+1) *mbwidth+x_pos)] = 0;
                }

            }
        }

    }
    else{
        //non-interlaced
        for (y_pos = 0; y_pos < (mbheight-1); y_pos++){
            for (x_pos = 0; x_pos < mbwidth; x_pos++){
                mb = mb_array+(y_pos *mbwidth+x_pos);
                mv_x = mb->mvs[3].vec.s16x;
                mv_y = mb->mvs[3].vec.s16y;
                mv_even_field = (mv_x *mv_x+mv_y * mv_y);

                if (md_param->usesad){
                    sad16 = mb->sad16;
                }

                if (mb->mode == INTRA_MODE)
                { //I MB
                    fvmd->mvs_x[(y_pos *mbwidth+x_pos)] = (fvmd->mvs_x[(y_pos *mbwidth+x_pos)] *2)/ 3;
                    fvmd->mvs_y[(y_pos *mbwidth+x_pos)] = (fvmd->mvs_y[(y_pos *mbwidth+x_pos)] *2)/ 3;
                    fvmd->IMB_flag[(y_pos *mbwidth+x_pos)] += 1;

                }
                else if ((mb->mode == INTER_MODE) || (mb->mode == INTER4V_MODE))
                { // P MB 
                    fvmd->mvs_x[(y_pos *mbwidth+x_pos)] = ((fvmd->mvs_x[(y_pos *mbwidth+x_pos)] *2)+mv_x) / 3;
                    fvmd->mvs_y[(y_pos *mbwidth+x_pos)] = ((fvmd->mvs_y[(y_pos *mbwidth+x_pos)] *2)+mv_y) / 3;
                    fvmd->IMB_flag[(y_pos *mbwidth+x_pos)] = 0;

                    //if (mv_even_field == 0 && (md_param->usesad)){
                    // mpe4 small motion vector because there is less skip MB in mpeg4 than h264
                    if (mv_even_field <= 2 && (md_param->usesad)) {     // Andrew 
                        if (sad16)
                            fvmd->sad16_cur = ((fvmd->sad16_cur *3)+sad16) >> 2;
                    }

                }

                mv_thd = md_param->mb_cell_th[(y_pos *mbwidth+x_pos)];

                if (((fvmd->mvs_x[(y_pos *mbwidth+x_pos)] *fvmd->mvs_x[(y_pos *mbwidth+x_pos)]+fvmd->mvs_y[(y_pos *mbwidth+x_pos)] *fvmd->mvs_y[(y_pos *mbwidth+x_pos)]) > mv_thd)) {
                    fvmd->l_mvs[(y_pos *mbwidth+x_pos)] = 1;
                    //printf("inter MB(%d, %d) active\n", y_pos, x_pos);
                }
                else
                    fvmd->l_mvs[(y_pos *mbwidth+x_pos)] = 0;

                if (md_param->usesad){
                    fvmd->sad16_Th = fvmd->sad16_cur+md_param->sad_offset;
                    //printf("fvmd->sad16_Th = %d\n", fvmd->sad16_Th);
                }

                if (md_param->usesad){
                    if ((fvmd->IMB_flag[(y_pos *mbwidth+x_pos)] >= md_param->mb_time_th) && (sad16>= fvmd->sad16_Th)) {
                        fvmd->l_mvsi[(y_pos *mbwidth+x_pos)] = md_param->mb_time_th;
                        //printf("intra MB(%d, %d) active, fvmd->sad16_Th = %d\n", y_pos, x_pos, fvmd->sad16_Th);
                    }
                    else
                        fvmd->l_mvsi[(y_pos *mbwidth+x_pos)] = 0;

                    if (fvmd->l_mvs[(y_pos *mbwidth+x_pos)] && (sad16 < fvmd->sad16_Th))
                        fvmd->l_mvs[(y_pos *mbwidth+x_pos)] = 0;

                }
                else{
                    if (fvmd->IMB_flag[(y_pos *mbwidth+x_pos)] >= md_param->mb_time_th)
                        fvmd->l_mvsi[(y_pos *mbwidth+x_pos)] = md_param->mb_time_th;
                    else
                        fvmd->l_mvsi[(y_pos *mbwidth+x_pos)] = 0;
                }

            }
        }
    } //non-interlaced end      

    fv_do_noise_filtering(fvmd->l_mvs, mbwidth, mbheight);
    fv_do_IMB_filtering(fvmd->l_mvsi, mbwidth, mbheight);

    //**********color rolling prevent**********//
    if (md_param->usesad){
        sad_diff = (int)(fvmd->sad16_cur-fvmd->sad16_pre);


        if (abs_fv(sad_diff) > md_param->rolling_prot_th){
            fvmd->sad16_pre = fvmd->sad16_cur;
            fvmd->sad_mask = 1;
            return 0;
        }
        else{
            fvmd->sad16_pre = (fvmd->sad16_pre+fvmd->sad16_cur) >> 1;
        }

        if (fvmd->sad_mask){
            fvmd->sad_mask = 0;
            return 0;
        }

    }
    //****************************************//    

    for (y_pos = 0; y_pos < (mbheight-1); y_pos++){
        for (x_pos = 0; x_pos < mbwidth; x_pos++){
            active->active_flag[(y_pos *mbwidth+x_pos)] = 0; //clear

            if ((fvmd->l_mvs[(y_pos *mbwidth+x_pos)]) || (fvmd->l_mvsi[(y_pos *mbwidth+x_pos)])){
                if (md_param->mb_cell_en[(y_pos *mbwidth+x_pos)]){
                    active->active_num++;
                    active->active_flag[(y_pos *mbwidth+x_pos)] = 1;
                }
            }

        }
    }

    return 0;

}

//----------------------------------------------------------------------------
void fv_do_noise_filtering(unsigned char *lmvs, unsigned int mbwidth, unsigned int mbheight){
    unsigned int x_pos, y_pos;
    unsigned char *l_mvs_ptr;
    l_mvs_ptr = lmvs-1;

    for (y_pos = 0; y_pos < mbheight; y_pos++){
        for (x_pos = 0; x_pos < mbwidth; x_pos++){

            l_mvs_ptr++;

            if (*l_mvs_ptr == 0)
                continue;

            if (!((x_pos == 0) || (y_pos == 0)))
                if (*(l_mvs_ptr-mbwidth-1))
                    continue;

            if (y_pos != 0)
                if (*(l_mvs_ptr-mbwidth))
                    continue;

            if (!((x_pos == (mbwidth-1)) || (y_pos == 0)))
                if (*(l_mvs_ptr-mbwidth+1))
                    continue;

            if (x_pos != 0)
                if (*(l_mvs_ptr-1))
                    continue;

            if (x_pos != (mbwidth-1))
                if (*(l_mvs_ptr+1))
                    continue;

            if (!((x_pos == 0) || (y_pos == (mbheight-1))))
                if (*(l_mvs_ptr+mbwidth-1))
                    continue;

            if (y_pos != (mbheight-1))
                if (*(l_mvs_ptr+mbwidth))
                    continue;

            if (!((x_pos == (mbwidth-1)) || (y_pos == (mbheight-1))))
                if (*(l_mvs_ptr+mbwidth+1))
                    continue;

            *l_mvs_ptr = 0; //adjoining points are all zero

        }
    }
    return ;
}

//----------------------------------------------------------------------------
void fv_do_IMB_filtering(unsigned char *lmvs, unsigned int mbwidth, unsigned int mbheight){
    unsigned int x_pos, y_pos;
    unsigned char *l_mvs_ptr;
    l_mvs_ptr = lmvs-1;
    //int a, b, c, d, e, f, g, h;

    for (y_pos = 0; y_pos < mbheight; y_pos++){
        for (x_pos = 0; x_pos < mbwidth; x_pos++){

            l_mvs_ptr++;
            if ((x_pos == 0) || (x_pos == (mbwidth-1)) || (y_pos == 0) || (y_pos == (mbheight-1))){
                *l_mvs_ptr = 0; //edge points clear to zero
                continue;
            }

            if (*l_mvs_ptr == 0)
                continue;

            if (*(l_mvs_ptr-1))
                if (*(l_mvs_ptr-mbwidth-1))
                    if (*(l_mvs_ptr-mbwidth))
                        continue;

            if (*(l_mvs_ptr-mbwidth))
                if (*(l_mvs_ptr-mbwidth+1))
                    if (*(l_mvs_ptr+1))
                        continue;

            if (*(l_mvs_ptr+1))
                if (*(l_mvs_ptr+mbwidth+1))
                    if (*(l_mvs_ptr+mbwidth))
                        continue;

            if (*(l_mvs_ptr+mbwidth))
                if (*(l_mvs_ptr+mbwidth-1))
                    if (*(l_mvs_ptr-1))
                        continue;

            *l_mvs_ptr = 0; //adjoin points are all zero

        }
    }
    return ;
}
