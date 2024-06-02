/* MIT License
 *
 * Copyright (c) 2020 Tino Hernandez <vjmuzik1@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "T4_PXP.h"
volatile IMXRT_NEXT_PXP_t next_pxp __attribute__ ((aligned(32))) = {0};
volatile bool PXP_done = true;

void (*PXP_doneCB)() = nullptr;

volatile IMXRT_NEXT_PXP_t *PXP_next_pxp() {
  return &next_pxp;
}


//These are only used to flush cached buffers
uint16_t PS_BUF_width, PS_BUF_height, PS_BUF_bytesPerPixel,
         PS_UBUF_width, PS_UBUF_height, PS_UBUF_bytesPerPixel,
         PS_VBUF_width, PS_VBUF_height, PS_VBUF_bytesPerPixel,
         AS_BUF_width, AS_BUF_height, AS_BUF_bytesPerPixel,
         OUT_BUF_width, OUT_BUF_height, OUT_BUF_bytesPerPixel,
         OUT_BUF2_width, OUT_BUF2_height, OUT_BUF2_bytesPerPixel;
         


void PXP_isr(){
  if((PXP_STAT & PXP_STAT_LUT_DMA_LOAD_DONE_IRQ) != 0){
    PXP_STAT_CLR = PXP_STAT_LUT_DMA_LOAD_DONE_IRQ;
  }
  if((PXP_STAT & PXP_STAT_NEXT_IRQ) != 0){
    PXP_STAT_CLR = PXP_STAT_NEXT_IRQ;
  }
  if((PXP_STAT & PXP_STAT_AXI_READ_ERROR) != 0){
    PXP_STAT_CLR = PXP_STAT_AXI_READ_ERROR;
  }
  if((PXP_STAT & PXP_STAT_AXI_WRITE_ERROR) != 0){
    PXP_STAT_CLR = PXP_STAT_AXI_WRITE_ERROR;
  }
  if((PXP_STAT & PXP_STAT_IRQ) != 0){
    PXP_STAT_CLR = PXP_STAT_IRQ;
    PXP_done = true;
    if (PXP_doneCB) (*PXP_doneCB)();
  }
#if defined(__IMXRT1062__) // Teensy 4.x
  asm("DSB");
#endif
}

void PXP_init(){
  attachInterruptVector(IRQ_PXP, PXP_isr);
  
  CCM_CCGR2 |= CCM_CCGR2_PXP(CCM_CCGR_ON);

  PXP_CTRL_SET = PXP_CTRL_SFTRST; //Reset
  
  PXP_CTRL_CLR = PXP_CTRL_SFTRST | PXP_CTRL_CLKGATE; //Clear reset and gate

  PXP_CTRL_SET = PXP_CTRL_IRQ_ENABLE | PXP_CTRL_NEXT_IRQ_ENABLE | PXP_CTRL_ENABLE;
//  PXP_CTRL_SET = PXP_CTRL_IRQ_ENABLE | PXP_CTRL_NEXT_IRQ_ENABLE;

  PXP_CSC1_COEF0 |= PXP_COEF0_BYPASS;

//  memcpy(&next_pxp, (const void*)&PXP_CTRL, sizeof(IMXRT_PXP_t)/4);

  next_pxp.CTRL = PXP_CTRL_IRQ_ENABLE | PXP_CTRL_NEXT_IRQ_ENABLE | PXP_CTRL_ENABLE;
  next_pxp.OUT_CTRL = PXP_OUT_CTRL;
  next_pxp.PS_SCALE = PXP_PS_SCALE;
  next_pxp.PS_CLRKEYLOW = PXP_PS_CLRKEYLOW_0;
  next_pxp.AS_CLRKEYLOW = PXP_AS_CLRKEYLOW_0;
  next_pxp.CSC1_COEF0 = PXP_COEF0_BYPASS;
    
  PXP_block_size(true);

//  PXP_process();
  
  NVIC_ENABLE_IRQ(IRQ_PXP);
}

void PXP_flip_vertically(bool flip){
  if(flip) next_pxp.CTRL |= PXP_CTRL_VFLIP;
  else next_pxp.CTRL &= ~PXP_CTRL_VFLIP;
}

void PXP_flip_horizontally(bool flip){
  if(flip) next_pxp.CTRL |= PXP_CTRL_HFLIP;
  else next_pxp.CTRL &= ~PXP_CTRL_HFLIP;
}

void PXP_flip_both(bool flip){
  if(flip) next_pxp.CTRL |= (PXP_CTRL_VFLIP | PXP_CTRL_HFLIP);
  else next_pxp.CTRL &= ~(PXP_CTRL_VFLIP | PXP_CTRL_HFLIP);
}

void PXP_block_size(bool size){
  if(size) next_pxp.CTRL |= PXP_CTRL_BLOCK_SIZE;
  else next_pxp.CTRL &= ~PXP_CTRL_BLOCK_SIZE;
}

void PXP_enable_repeat(bool repeat){
  if(repeat) next_pxp.CTRL |= PXP_CTRL_EN_REPEAT;
  else next_pxp.CTRL &= ~PXP_CTRL_EN_REPEAT;
}

void PXP_rotate(uint8_t r){
  next_pxp.CTRL &= ~PXP_CTRL_ROTATE(0xF);
  next_pxp.CTRL |= PXP_CTRL_ROTATE(r);
}

void PXP_rotate_position(bool pos){
  if(pos) next_pxp.CTRL |= PXP_CTRL_ROT_POS;
  else next_pxp.CTRL &= ~PXP_CTRL_ROT_POS;
}

void PXP_output_format(uint8_t format, uint8_t interlaced, bool alpha_override, uint8_t alpha){
  next_pxp.OUT_CTRL &= ~(PXP_OUT_CTRL_FORMAT(0xFF) | PXP_OUT_CTRL_INTERLACED_OUTPUT(0xF) | PXP_OUT_CTRL_ALPHA_OUTPUT | PXP_OUT_CTRL_ALPHA(0xFF));
  next_pxp.OUT_CTRL |= (PXP_OUT_CTRL_FORMAT(format) | PXP_OUT_CTRL_INTERLACED_OUTPUT(interlaced) | (alpha_override ? PXP_OUT_CTRL_ALPHA_OUTPUT : 0) | PXP_OUT_CTRL_ALPHA(alpha));
}

void PXP_output_buffer(void* buf, uint16_t bytesPerPixel, uint16_t width, uint16_t height){
  if(!buf) return;
  next_pxp.OUT_BUF = buf;
  next_pxp.OUT_PITCH = PXP_PITCH(bytesPerPixel * width);
  next_pxp.OUT_LRC = PXP_XCOORD(width-1) | PXP_YCOORD(height-1);
  
  OUT_BUF_width = width;
  OUT_BUF_height = height;
  OUT_BUF_bytesPerPixel = bytesPerPixel;
}

void PXP_output_buffer2(void* buf, uint16_t bytesPerPixel, uint16_t width, uint16_t height){ //Only for interlaced or YUV formats
  next_pxp.OUT_BUF2 = buf;
  
  OUT_BUF2_width = width;
  OUT_BUF2_height = height;
  OUT_BUF2_bytesPerPixel = bytesPerPixel;
}

void PXP_output_clip(uint16_t x, uint16_t y){
  next_pxp.OUT_LRC = PXP_XCOORD(x) | PXP_YCOORD(y);
}

void PXP_input_format(uint8_t format, uint8_t decimationScaleX, uint8_t decimationScaleY, bool wb_swap){
  next_pxp.PS_CTRL = PXP_PS_CTRL_FORMAT(format) | PXP_PS_CTRL_DECX(decimationScaleX) | PXP_PS_CTRL_DECY(decimationScaleY) | (wb_swap ? PXP_PS_CTRL_WB_SWAP : 0);
}

void PXP_input_buffer(void* buf, uint16_t bytesPerPixel, uint16_t width, uint16_t height){
  if(!buf) return;
  next_pxp.PS_BUF = buf;
  next_pxp.PS_PITCH = PXP_PITCH(bytesPerPixel * width);
  PXP_input_position(0, 0, width - 1, height - 1);
  
  PS_BUF_width = width;
  PS_BUF_height = height;
  PS_BUF_bytesPerPixel = bytesPerPixel;
}

void PXP_input_u_buffer(void* buf, uint16_t bytesPerPixel, uint16_t width, uint16_t height){ //Only for YCBCR or YUV formats
  if(!buf) return;
  next_pxp.PS_UBUF = buf;
  
  PS_UBUF_width = width;
  PS_UBUF_height = height;
  PS_UBUF_bytesPerPixel = bytesPerPixel;
}

void PXP_input_v_buffer(void* buf, uint16_t bytesPerPixel, uint16_t width, uint16_t height){ //Only for YCBCR or YUV formats
  if(!buf) return;
  next_pxp.PS_VBUF = buf;
  
  PS_VBUF_width = width;
  PS_VBUF_height = height;
  PS_VBUF_bytesPerPixel = bytesPerPixel;
}

void PXP_input_position(uint16_t x, uint16_t y, uint16_t x1, uint16_t y1){
  /*
    * Set the process surface position in output buffer
    * x, y: Upper left corner
    * x1, y1: Lower right corners
    */
  Serial.printf("PXP_input_position(%u, %u, %u, %u)\n", x, y, x1, y1);
  next_pxp.OUT_PS_ULC = PXP_XCOORD(x) | PXP_YCOORD(y);
  next_pxp.OUT_PS_LRC = PXP_XCOORD(x1) | PXP_YCOORD(y1);
}

void PXP_input_background_color(uint8_t r8, uint8_t g8, uint8_t b8){
  next_pxp.PS_BACKGROUND = PXP_COLOR((r8 << 16) | (g8 << 8) | (b8));
}

void PXP_input_background_color(uint32_t rgb888){
  next_pxp.PS_BACKGROUND = PXP_COLOR(rgb888);
}

void PXP_input_color_key_low(uint8_t r8, uint8_t g8, uint8_t b8){
  next_pxp.PS_CLRKEYLOW = PXP_COLOR((r8 << 16) | (g8 << 8) | (b8));
}

void PXP_input_color_key_low(uint32_t rgb888){
  next_pxp.PS_CLRKEYLOW = PXP_COLOR(rgb888);
}

void PXP_input_color_key_high(uint8_t r8, uint8_t g8, uint8_t b8){
  next_pxp.PS_CLRKEYLOW = PXP_COLOR((r8 << 16) | (g8 << 8) | (b8));
}

void PXP_input_color_key_high(uint32_t rgb888){
  next_pxp.PS_CLRKEYHIGH = PXP_COLOR(rgb888);
}

void PXP_input_scale(uint16_t x_scale, uint16_t y_scale, uint16_t x_offset, uint16_t y_offset){ //Scale should be reciprocal of desired factor, eg. 4x larger is 0.25e0
  if(x_scale > 0x2.000p0) x_scale = 0x2.000p0; //Maximum downscale of 2
  if(y_scale > 0x2.000p0) y_scale = 0x2.000p0; //Maximum downscale of 2
  next_pxp.PS_SCALE = PXP_XSCALE(x_scale) | PXP_YSCALE(y_scale);
  next_pxp.PS_OFFSET = PXP_XOFFSET(x_offset) | PXP_YOFFSET(y_offset);
}

void PXP_overlay_format(uint8_t format, uint8_t alpha_ctrl, bool colorKey, uint8_t alpha, uint8_t rop, bool alpha_invert){
  next_pxp.AS_CTRL = PXP_AS_CTRL_FORMAT(format) | PXP_AS_CTRL_ALPHA_CTRL(alpha_ctrl) | (colorKey ? PXP_AS_CTRL_ENABLE_COLORKEY : 0) | PXP_AS_CTRL_ALPHA(alpha) | PXP_AS_CTRL_ROP(rop) | (alpha_invert ? PXP_AS_CTRL_ALPHA_INVERT : 0);
}

void PXP_overlay_buffer(void* buf, uint16_t bytesPerPixel, uint16_t width, uint16_t height){
  if(!buf) return;
  next_pxp.AS_BUF = buf;
  next_pxp.AS_PITCH = PXP_PITCH(bytesPerPixel * width);
  PXP_overlay_position(0, 0, width - 1, height - 1);
  
  AS_BUF_width = width;
  AS_BUF_height = height;
  AS_BUF_bytesPerPixel = bytesPerPixel;
}

void PXP_overlay_position(uint16_t x, uint16_t y, uint16_t x1, uint16_t y1){
  next_pxp.OUT_AS_ULC = PXP_XCOORD(x) | PXP_YCOORD(y);
  next_pxp.OUT_AS_LRC = PXP_XCOORD(x1) | PXP_YCOORD(y1);
}

void PXP_overlay_color_key_low(uint8_t r8, uint8_t g8, uint8_t b8){
  next_pxp.AS_CLRKEYLOW = PXP_COLOR((r8 << 16) | (g8 << 8) | (b8));
}

void PXP_overlay_color_key_low(uint32_t rgb888){
  next_pxp.AS_CLRKEYLOW = PXP_COLOR(rgb888);
}

void PXP_overlay_color_key_high(uint8_t r8, uint8_t g8, uint8_t b8){
  next_pxp.AS_CLRKEYLOW = PXP_COLOR((r8 << 16) | (g8 << 8) | (b8));
}

void PXP_overlay_color_key_high(uint32_t rgb888){
  next_pxp.AS_CLRKEYHIGH = PXP_COLOR(rgb888);
}

void PXP_process(){
  while ((PXP_NEXT & PXP_NEXT_ENABLED) != 0){}
  PXP_done = false;
  
  if ((uint32_t)next_pxp.OUT_BUF >= 0x20200000u)  arm_dcache_flush_delete((void*)next_pxp.OUT_BUF, OUT_BUF_width * OUT_BUF_height * OUT_BUF_bytesPerPixel);
  if ((uint32_t)next_pxp.OUT_BUF2 >= 0x20200000u)  arm_dcache_flush_delete((void*)next_pxp.OUT_BUF2, OUT_BUF2_width * OUT_BUF2_height * OUT_BUF2_bytesPerPixel);
  if ((uint32_t)next_pxp.PS_BUF >= 0x20200000u)  arm_dcache_flush_delete((void*)next_pxp.PS_BUF, PS_BUF_width * PS_BUF_height * PS_BUF_bytesPerPixel);
  if ((uint32_t)next_pxp.PS_UBUF >= 0x20200000u)  arm_dcache_flush_delete((void*)next_pxp.PS_UBUF, PS_UBUF_width * PS_UBUF_height * PS_UBUF_bytesPerPixel);
  if ((uint32_t)next_pxp.PS_VBUF >= 0x20200000u)  arm_dcache_flush_delete((void*)next_pxp.PS_VBUF, PS_VBUF_width * PS_VBUF_height * PS_VBUF_bytesPerPixel);
  if ((uint32_t)next_pxp.AS_BUF >= 0x20200000u)  arm_dcache_flush_delete((void*)next_pxp.AS_BUF, AS_BUF_width * AS_BUF_height * AS_BUF_bytesPerPixel);
  
  
  PXP_NEXT = (uint32_t)&next_pxp;
}

void PXP_finish(){
//  while((PXP_STAT & PXP_STAT_IRQ) == 0){}
//  if((PXP_STAT & PXP_STAT_IRQ) != 0){
//    PXP_STAT_CLR = PXP_STAT_IRQ;
//  }
  while(!PXP_done){};
  PXP_CTRL_CLR =  PXP_CTRL_ENABLE;
  if ((uint32_t)next_pxp.OUT_BUF > 0x2020000) arm_dcache_flush_delete((void *)next_pxp.OUT_BUF, OUT_BUF_width * OUT_BUF_height * OUT_BUF_bytesPerPixel);
}


void PXP_GetScaleFactor(uint16_t inputDimension, uint16_t outputDimension, uint8_t *dec, uint32_t *scale)
{
    uint32_t scaleFact = ((uint32_t)inputDimension << 12U) / outputDimension;

    if (scaleFact >= (16U << 12U))
    {
        /* Desired fact is two large, use the largest support value. */
        *dec = 3U;
        *scale = 0x2000U;
    }
    else
    {
        if (scaleFact > (8U << 12U))
        {
            *dec = 3U;
        }
        else if (scaleFact > (4U << 12U))
        {
            *dec = 2U;
        }
        else if (scaleFact > (2U << 12U))
        {
            *dec = 1U;
        }
        else
        {
            *dec = 0U;
        }

        *scale = scaleFact >> (*dec);

        if (0U == *scale)
        {
            *scale = 1U;
        }
    }
}

void PXP_setScaling(uint16_t inputWidth, uint16_t inputHeight, uint16_t outputWidth, uint16_t outputHeight)
{
    uint8_t decX, decY;
    uint32_t scaleX, scaleY;

    PXP_GetScaleFactor(inputWidth, outputWidth, &decX, &scaleX);
    PXP_GetScaleFactor(inputHeight, outputHeight, &decY, &scaleY);

    next_pxp.PS_CTRL = (next_pxp.PS_CTRL & ~(0xC00U | 0x300U)) | PXP_PS_CTRL_DECX(decX) | PXP_PS_CTRL_DECY(decY);

    next_pxp.PS_SCALE = PXP_XSCALE(scaleX) | PXP_YSCALE(scaleY);
}

void PXP_SetCsc1Mode(uint8_t mode)
{
    //kPXP_Csc1YUV2RGB = 0U, /*!< YUV to RGB. */
    //kPXP_Csc1YCbCr2RGB,    /*!< YCbCr to RGB. */
  
    /*
     * The equations used for Colorspace conversion are:
     *
     * R = C0*(Y+Y_OFFSET)                   + C1(V+UV_OFFSET)
     * G = C0*(Y+Y_OFFSET) + C3(U+UV_OFFSET) + C2(V+UV_OFFSET)
     * B = C0*(Y+Y_OFFSET) + C4(U+UV_OFFSET)
     */

    if (mode == 0)
    {
        //next_pxp.CSC1_COEF0 = (next_pxp.CSC1_COEF0 &
        //                    ~(0x1FFC0000U | 0x1FFU | 0x3FE00U | 0x80000000U)) |
        //                   PXP_COEF0_C0(0x100U)         /* 1.00. */
        //                   | PXP_COEF0_Y_OFFSET(0x0U)   /* 0. */
        //                   | PXP_COEF0_UV_OFFSET(0x0U); /* 0. */
        //next_pxp.CSC1_COEF1 = PXP_COEF1_C1(0x0123U)        /* 1.140. */
        //                   | PXP_COEF1_C4(0x0208U);     /* 2.032. */
        //next_pxp.CSC1_COEF2 = PXP_COEF2_C2(0x076BU)        /* -0.851. */
        //                   | PXP_COEF2_C3(0x079BU);     /* -0.394. */
        next_pxp.CSC1_COEF0 = 0x84ab01f0;
        next_pxp.CSC1_COEF1 =  0;
        next_pxp.CSC1_COEF2 = 0;                   
                           
    }
    else
    {
        next_pxp.CSC1_COEF0 = (next_pxp.CSC1_COEF0 &
                            ~(0x1FFC0000U | 0x1FFU | 0x3FE00U)) |
                           0x80000000U | PXP_COEF0_C0(0x12AU) /* 1.164. */
                           | PXP_COEF0_Y_OFFSET(0x1F0U)                          /* -16. */
                           | PXP_COEF0_UV_OFFSET(0x180U);                        /* -128. */
        next_pxp.CSC1_COEF1 = PXP_COEF1_C1(0x0198U)                                 /* 1.596. */
                           | PXP_COEF1_C4(0x0204U);                              /* 2.017. */
        next_pxp.CSC1_COEF2 = PXP_COEF2_C2(0x0730U)                                 /* -0.813. */
                           | PXP_COEF2_C3(0x079CU);                              /* -0.392. */
    }
}

void PXP_set_csc_y8_to_rgb() {
  next_pxp.CSC1_COEF0 = 0x84ab01f0;
  next_pxp.CSC1_COEF1 =  0;
  next_pxp.CSC1_COEF2 = 0; 
}


/**************************************************************
 * Function that configures PXP for rotation, flip and scaling
 * and outputs to display.
 * rotation: 0 - 0 degrees, 1 - 90 degrees, 
 *           2 - 180 degrees, 3 - 270 degrees
 * flip: false - no flip, true - flip.
 *       control in filp function PXP_flip, currently configured
 *       for horizontal flip.
 * scaling: scaling factor:
 *          To scale up by a factor of 4, the value of 1/4
 *          Follows inverse of factor input so for a scale
 *          factor of 1.5 actual scaling is about 67%
 ***************************************************************/
void PXP_ps_output(uint16_t disp_width, uint16_t disp_height, uint16_t image_width, uint16_t image_height, 
                   void* buf_in, uint8_t format_in, uint8_t bpp_in, uint8_t byte_swap_in, 
                   void* buf_out, uint8_t format_out, uint8_t bpp_out, uint8_t byte_swap_out, 
                   uint8_t rotation, bool flip, float scaling, 
                   uint16_t* scr_width, uint16_t* scr_height)
{
  
    // Lets see if we can setup a window into the camera data
    //Serial.printf("PXP_ps_window_output(%u, %u, %u, %u.... %u, %u. %f)\n", disp_width, disp_height, image_width, image_height,
    //              rotation, flip, scaling);

    // we may bypass some of the helper functions and output the data directly.
    volatile IMXRT_NEXT_PXP_t *next_pxp = PXP_next_pxp();


    // lets reduce down to basic.
    PXP_input_background_color(0, 0, 153);

    // Initial input stuff.
    PXP_input_buffer(buf_in, bpp_in, image_width, image_height);
    // VYUY1P422, PXP_UYVY1P422
    PXP_input_format(format_in, 0, 0, byte_swap_in);
    if (format_in == PXP_Y8 || format_in == PXP_Y4)
        PXP_set_csc_y8_to_rgb();

    // Output stuff
    uint16_t out_width, out_height, output_Width, output_Height;
    if (rotation == 1 || rotation == 3) {
        out_width = disp_height;
        out_height = disp_width;
    } else {
        out_width = disp_width;
        out_height = disp_height;
    }

    PXP_output_buffer(buf_out, bpp_out, disp_width, disp_height);
    PXP_output_format(format_out, 0, 0, byte_swap_out);

    // Set the clip rect.
    //PXP_output_clip(disp_width - 1, disp_height - 1);

    // Rotation
    /* Setting this bit to 1'b0 will place the rotationre sources at  *
   * the output stage of the PXP data path. Image compositing will  *
   * occur before pixels are processed for rotation.                *
   * Setting this bit to a 1'b1 will place the rotation resources   *
   * before image composition.                                      *
   */
    PXP_rotate_position(0);
    //Serial.println("Rotating");
    // Performs the actual rotation specified
    PXP_rotate(rotation);

    // flip - pretty straight forward
    PXP_flip(flip);

    // Lets set scaling
    if (scaling == 0) scaling = 1.0;
    PXP_setScaling(scaling);

    // now if the input image and the scaled output image are not the same size, we may want to center either
    // the input or the output...
    uint32_t scaled_image_width = (uint32_t)((float)(image_width) / scaling);
    uint32_t scaled_image_height = (uint32_t)((float)(image_height) / scaling);
    

    if (rotation == 1 || rotation == 3) {
        PXP_output_clip(disp_height - 1, disp_width - 1);
    } else {
        PXP_output_clip(disp_width - 1, disp_height - 1);
    }

    uint16_t ul_x = 0;
    uint16_t ul_y = 0;
    uint16_t lr_x = disp_width - 1;
    uint16_t lr_y = disp_height - 1;

    if (rotation == 1 || rotation == 3) {
        if (scaled_image_width <= disp_height) {
            ul_x = (disp_height - scaled_image_width) / 2;
            lr_x = ul_x + scaled_image_width - 1;
        }
        if (scaled_image_height <= disp_width) {
            ul_y = (disp_width - scaled_image_height) / 2;
            lr_y = ul_y + scaled_image_height - 1;
        }
    } else {
        if (scaled_image_width <= disp_width) {
            ul_x = (disp_width - scaled_image_width) / 2;
            lr_x = ul_x + scaled_image_width - 1;
        }
        if (scaled_image_height <= disp_height) {
            ul_y = (disp_height - scaled_image_height) / 2;
            lr_y = ul_y + scaled_image_height - 1;
        }
    }
    PXP_input_position(ul_x, ul_y, lr_x, lr_y);  // need this to override the setup in pxp_input_buffer

    // See if we should input clip the image...  That is center the large image onto the screen.  later allow for panning.
    uint8_t *buf_in_clipped = (uint8_t *)buf_in;

    if (rotation == 1 || rotation == 3) {
        if (scaled_image_width > disp_height) {
            buf_in_clipped += ((scaled_image_width - disp_height) / 2) * bpp_in;
        }

        if (scaled_image_height > disp_width) {
            buf_in_clipped += ((scaled_image_height - disp_width) / 2) * bpp_in * image_width;
        }
    } else {
        if (scaled_image_width > disp_width) {
            buf_in_clipped += ((scaled_image_width - disp_width) / 2) * bpp_in;
        }

        if (scaled_image_height > disp_height) {
            buf_in_clipped += ((scaled_image_height - disp_height) / 2) * bpp_in * image_width;
        }
    }
    if (buf_in_clipped != (uint8_t *)buf_in) {
        Serial.printf("Input clip Buffer(%p -> %p)\n", buf_in, buf_in_clipped);
        next_pxp->PS_BUF = buf_in_clipped;
    }

    *scr_width = disp_width;
    *scr_height = disp_height;

    PXP_process();

}

void PXP_setScaling(float scaling) {
    // we may bypass some of the helper functions and output the data directly.
    volatile IMXRT_NEXT_PXP_t *next_pxp = PXP_next_pxp();

    // lets setup the scaling.
    uint16_t pxp_scale = scaling * 4096;

    uint16_t scale = 0;
    uint8_t dec = 0;

    if (pxp_scale > 32768)  //8 *4096,
    {
        dec = 3;
    } else if (pxp_scale > 16384) {   //4 *4096
        dec = 2;
    } else if (pxp_scale > 8192) {    //2 *4096
        dec = 1;
    } else {
        dec = 0;
    }

    scale = pxp_scale >> (dec);

    if (0 == scale) {
        scale = 1U;
    }

    next_pxp->PS_CTRL = (next_pxp->PS_CTRL & ~(0xC00U | 0x300U)) | PXP_PS_CTRL_DECX(dec) | PXP_PS_CTRL_DECY(dec);
    next_pxp->PS_SCALE = PXP_XSCALE(scale) | PXP_YSCALE(scale);

}

void PXP_flip(bool flip) {
  /* there are 3 flip commands that you can use           *
   * PXP_flip_vertically                                  *
   * PXP_flip_horizontally                                *
   * PXP_flip_both                                        *
   */
  PXP_flip_both(flip);
}

void PXP_setPXPDoneCB(void (*cb)()) {
  PXP_doneCB = cb;

}