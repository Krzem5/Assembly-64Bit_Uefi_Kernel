#ifndef __KERNEL_GFX_GFX_H__
#define __KERNEL_GFX_GFX_H__ 1
#include <shared.h>
#include <gfx/font.h>
#include <kmain.h>
#include <stdint.h>



#define COLOR(r,g,b) ((color_t)(((r)<<16)|((g)<<8)|(b)))



typedef uint32_t color_t;



void KERNEL_CALL KERNEL_UNMAP_AFTER_LOAD gfx_init(KernelArgs* ka);



void KERNEL_CALL gfx_print_char(uint8_t c,uint64_t x,uint64_t y,color_t cl,Font f,uint8_t sc);



void KERNEL_CALL gfx_fill_rect(uint64_t x,uint64_t y,uint64_t w,uint64_t h,color_t cl);



#endif
