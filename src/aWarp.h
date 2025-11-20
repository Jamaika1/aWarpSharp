// aWarpSharp package 2012.03.28 for Avisynth 2.5
// Copyright (C) 2003 MarcFD, 2012 Skakov Pavel
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// note: when included SMAGL must be defined to log2 of src upscaling level
#define SMAG (1<<SMAGL)
#define MERGE2(a, b) a##b
#define MERGE(a, b) MERGE2(a, b)

// Warp0 and Warp2
/*
#define SMAGL 2
#include "aWarp.h"
#define SMAGL 0
#include "aWarp.h"
*/
// WxH min: 4x2, mul: 4x1
// depth: 0..7FFFh

/*
; parameter 1(src) : 8 + ebp
; parameter 2(edg) : 12 + ebp
; parameter 3(dst) : 16 + ebp
; parameter 4(plane) : 20 + ebp
; parameter 5(plane_edg) : 24 + ebp
; parameter 6(depth) : 28 + ebp
; parameter 7(dst_vi) : 32 + ebp
*/
void MERGE(Warp, SMAGL)(PVideoFrame &src, PVideoFrame &edg, PVideoFrame &dst, int plane, int plane_edg, int depth, const VideoInfo &dst_vi)
{
	const int src_pitch = src->GetPitch(plane);
	const int edg_pitch = edg->GetPitch(plane_edg);
	const int dst_pitch = dst->GetPitch(plane);
	const int row_size = dst->GetRowSize() >> dst_vi.GetPlaneWidthSubsampling (plane);
	const int i = -((row_size + 3) & ~3);
	const int c = row_size + i - 1;
	const int height = dst->GetHeight() >> dst_vi.GetPlaneHeightSubsampling (plane);
	const unsigned char *psrc = src->GetReadPtr(plane) - i*SMAG;
	const unsigned char *pedg = edg->GetReadPtr(plane_edg) - i;
	unsigned char *pdst = dst->GetWritePtr(plane) - i;

	depth <<= 8;

	const short x_limit_min[8] = { (short)(i*SMAG)    , (short)((i-1)*SMAG), (short)((i-2)*SMAG), (short)((i-3)*SMAG), 
                                 (short)((i-4)*SMAG), (short)((i-5)*SMAG), (short)((i-6)*SMAG), (short)((i-7)*SMAG)};
	const short x_limit_max[8] = { (short)(c*SMAG)    , (short)((c-1)*SMAG), (short)((c-2)*SMAG), (short)((c-3)*SMAG), 
                                 (short)((c-4)*SMAG), (short)((c-5)*SMAG), (short)((c-6)*SMAG), (short)((c-7)*SMAG)};

  if (!(g_cpuid & CPUF_SSE2))
  {
    // SSE2 and SSSE3 versions
    for (int y = 0; y < height; y++)
    {
      int y_limit_min = -y * 0x80;
      int y_limit_max = (height - y) * 0x80 - 0x81;
      int edg_pitchp = -(y ? edg_pitch : 0);
      int edg_pitchn = y != height - 1 ? edg_pitch : 0;

      asm volatile ( \
        "mov        %[psrc], %%rsi          \n\t" \
        "mov        %[pedg], %%rcx          \n\t" \
        "mov        %[pdst], %%rax          \n\t" \
        "movsxd     %[src_pitch], %%rdx     \n\t" \
        "movsxd     %[edg_pitchp], %%rbx    \n\t" \
        "movsxd     %[i], %%rdi             \n\t" \
        "sub        $0x08, %%rax            \n\t" \
        "add        %%rsi, %%rdx            \n\t" \
        "add        %%rcx, %%rbx            \n\t" \
    : \
    : [psrc] "r" (psrc), [pedg] "r" (pedg), [pdst] "r" (pdst), [src_pitch] "r" (src_pitch), [edg_pitchp] "r" (edg_pitchp), [i] "r" (i) \
    : "memory", "cc", "%rsi", "%rdi", "%rax", "%rbx", "%rcx", "%rdx" );
      asm volatile ( \
        "movd       (%[y_limit_min]), %%xmm1  \n\t" \
        "movd       (%[y_limit_max]), %%xmm2  \n\t" \
        "movdqu     (%[x_limit_min]), %%xmm3  \n\t" \
        "movdqu     (%[x_limit_max]), %%xmm4  \n\t" \
        "movd       (%[depth]), %%xmm6        \n\t" \
        "movd       (%[src_pitch]), %%xmm0    \n\t" \
        "pcmpeqw    %%xmm7, %%xmm7          \n\t" \
        "psrlw      $0x0f, %%xmm7           \n\t" \
        "punpcklwd  %%xmm7, %%xmm0           \n\t" \
        "pshufd     $0, %%xmm1, %%xmm1      \n\t" \
        "pshufd     $0, %%xmm2, %%xmm2      \n\t" \
        "pshufd     $0, %%xmm6, %%xmm6      \n\t" \
        "pshufd     $0, %%xmm0, %%xmm0      \n\t" \
        "packssdw   %%xmm1, %%xmm1          \n\t" \
        "packssdw   %%xmm2, %%xmm2          \n\t" \
        "packssdw   %%xmm6, %%xmm6          \n\t" \
        "pcmpeqw    %%xmm5, %%xmm5          \n\t" \
        "psllw      $0x0f, %%xmm5           \n\t" \
        "push       %%rbp                   \n\t" \
        "movsxd     %[edg_pitchn], %%rbp    \n\t" \
        "mov        %%rax, %%r8             \n\t" \
        "add        %%rcx, %%rbp            \n\t" \
        "movdqa     (%%rdi,%%rcx), %%xmm7   \n\t" \
        "movdqa     %%xmm0, %%xmm8          \n\t" \
        "movdqa     %%xmm6, %%xmm9          \n\t" \
        "movdqa     %%xmm1, %%xmm10         \n\t" \
        "movdqa     %%xmm2, %%xmm11         \n\t" \
        "movdqa     %%xmm3, %%xmm12         \n\t" \
        "movdqa     %%xmm4, %%xmm13         \n\t" \
        "movdqa     %%xmm7, %%xmm1          \n\t" \
        "pslldq     $7, %%xmm7              \n\t" \
        "punpcklqdq %%xmm1, %%xmm7          \n\t" \
        "psrldq     $1, %%xmm1              \n\t" \
        "psrldq     $7, %%xmm7              \n\t" \
        /*"test      %[CPUF_SSSE3], %[g_cpuid] \n\t"*/ \
        "jnz        29f                     \n\t" \
        "psrlw      $8, %%xmm5              \n\t" \
        ".align     0x10                    \n\t" \
        "12:                                \n\t" \
        "movq       (%%rdi,%%rbx), %%xmm4   \n\t" \
        "movq       (%%rdi,%%rbp), %%xmm2   \n\t" \
        "pxor       %%xmm3, %%xmm3          \n\t" \
        "punpcklbw  %%xmm3, %%xmm7          \n\t" \
        "punpcklbw  %%xmm3, %%xmm1          \n\t" \
        "punpcklbw  %%xmm3, %%xmm4          \n\t" \
        "punpcklbw  %%xmm3, %%xmm2          \n\t" \
        "psubw      %%xmm1, %%xmm7          \n\t" \
        "psubw      %%xmm2, %%xmm4          \n\t" \
        "psllw      $7, %%xmm7              \n\t" \
        "psllw      $7, %%xmm4              \n\t" \
        "pmulhw     %%xmm6, %%xmm7          \n\t" \
        "pmulhw     %%xmm6, %%xmm4          \n\t" \
        "movdqa     %%xmm8, %%xmm6          \n\t" \
        "pmaxsw     %%xmm10, %%xmm4         \n\t" \
        "pminsw     %%xmm11, %%xmm4         \n\t" \
        "pcmpeqw    %%xmm3, %%xmm3          \n\t" \
        "psrlw      $9, %%xmm3              \n\t" \
        "movdqa     %%xmm7, %%xmm1          \n\t" \
        "movdqa     %%xmm4, %%xmm2          \n\t" \
    : \
    : [y_limit_min] "r" (y_limit_min), [y_limit_max] "r" (y_limit_max), [x_limit_min] "r" (x_limit_min), [x_limit_max] "r" (x_limit_max),
      [depth] "r" (depth), [src_pitch] "r" (src_pitch), [edg_pitchn] "r" (edg_pitchn) \
    : "memory", "cc", "%r8", "%rdi", "%rax", "%rbp", "%rbx", "%rcx", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7",
      "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13" );
#if SMAGL
      asm volatile ( \
        "psllw      (%[smagl]), %%xmm4      \n\t" \
        "psllw      (%[smagl]), %%xmm7      \n\t" \
    : \
    : [smagl] "r" ((int)SMAGL) \
    : "memory", "cc", "%xmm4", "%xmm7" );
#endif
      asm volatile ( \
        "pand       %%xmm3, %%xmm7          \n\t" \
        "pand       %%xmm3, %%xmm4          \n\t" \
        "psraw      (%[smagl7]), %%xmm1     \n\t" \
        "psraw      (%[smagl7]), %%xmm2     \n\t" \
        "movd       %%edi, %%xmm3           \n\t" \
        "pmaxsw     %%xmm2, %%xmm1          \n\t" \
    : \
    : [smagl7] "r" (7-(int)SMAGL) \
    : "memory", "cc", "%edi", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm7" );
#if SMAGL
      asm volatile ( \
        "pslld      (%[smagl]), %%xmm3      \n\t" \
    : \
    : [smagl] "r" ((int)SMAGL) \
    : "memory", "cc", "%xmm3" );
#endif
      asm volatile ( \
        "packssdw   %%xmm3, %%xmm3          \n\t" \
        "paddsw     %%xmm3, %%xmm1          \n\t" \
        "movdqa     %%xmm13, %%xmm3          \n\t" \
        "movdqa     %%xmm12, %%xmm0          \n\t" \
        "pcmpgtw    %%xmm1, %%xmm3          \n\t" \
        "pcmpgtw    %%xmm1, %%xmm0          \n\t" \
        "pminsw     %%xmm13, %%xmm1          \n\t" \
        "pmaxsw     %%xmm12, %%xmm1          \n\t" \
        "pand       %%xmm3, %%xmm7          \n\t" \
        "pandn      %%xmm7, %%xmm0          \n\t" \
        "movdqa     %%xmm2, %%xmm7          \n\t" \
        "punpcklwd  %%xmm1, %%xmm2          \n\t" \
        "punpckhwd  %%xmm1, %%xmm7          \n\t" \
        "pmaddwd	%%xmm6, %%xmm2          \n\t" \
        "pmaddwd	%%xmm6, %%xmm7          \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "pinsrw     $0, (%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $0, (%%rax,%%rdx), %%xmm1 \n\t" \
    : \
    : \
    : "memory", "cc", "%eax", "%rax", "%rdx", "%rsi", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4",
      "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13" );
#if SMAGL
      asm volatile ( \
        "movd       %%xmm2, %%eax           \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "pinsrw     $1, 4(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $1, 4(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "pinsrw     $2, 8(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $2, 8(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "pinsrw     $3, 12(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $3, 12(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "pinsrw     $4, 16(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $4, 16(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "pinsrw     $5, 20(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $5, 20(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "pinsrw     $6, 24(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $6, 24(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "pinsrw     $7, 28(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $7, 28(%%rax,%%rdx), %%xmm1 \n\t" \
    : \
    : \
    : "memory", "cc", "%eax", "%rax", "%rdx", "%rsi", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4",
      "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13" );
#else
      asm volatile ( \
        "movd       %%xmm2, %%eax           \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "pinsrw     $1, 1(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $1, 1(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "pinsrw     $2, 2(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $2, 2(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "pinsrw     $3, 3(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $3, 3(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "pinsrw     $4, 4(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $4, 4(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "pinsrw     $5, 5(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $5, 5(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "pinsrw     $6, 6(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $6, 6(%%rax,%%rdx), %%xmm1 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "pinsrw     $7, 7(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $7, 7(%%rax,%%rdx), %%xmm1 \n\t" \
    : \
    : \
    : "memory", "cc", "%eax", "%rax", "%rdx", "%rsi", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4",
      "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13" );
#endif
      asm volatile ( \
        "mov        %%r8, %%rax             \n\t" \
        "pcmpeqw    %%xmm6, %%xmm6          \n\t" \
        "movdqa     %%xmm3, %%xmm2          \n\t" \
        "psrlw      $8, %%xmm6              \n\t" \
        "movdqa     %%xmm1, %%xmm7          \n\t" \
        "pand       %%xmm6, %%xmm3          \n\t" \
        "pand       %%xmm6, %%xmm1          \n\t" \
        "movdqa     %%xmm5, %%xmm6          \n\t" \
        "psubw      %%xmm0, %%xmm6          \n\t" \
        "pmullw     %%xmm6, %%xmm3          \n\t" \
        "pmullw     %%xmm6, %%xmm1          \n\t" \
        "movdqa     %%xmm5, %%xmm6          \n\t" \
        "psrlw      $1, %%xmm5              \n\t" \
        "psrlw      $8, %%xmm2              \n\t" \
        "psrlw      $8, %%xmm7              \n\t" \
        "pmullw     %%xmm0, %%xmm2          \n\t" \
        "pmullw     %%xmm0, %%xmm7          \n\t" \
        "paddw      %%xmm2, %%xmm3          \n\t" \
        "paddw      %%xmm7, %%xmm1          \n\t" \
        "paddw      %%xmm5, %%xmm3          \n\t" \
        "paddw      %%xmm5, %%xmm1          \n\t" \
        "psraw      $7, %%xmm3              \n\t" \
        "psraw      $7, %%xmm1              \n\t" \
        "psubw      %%xmm4, %%xmm6          \n\t" \
        "movdqu     7(%%rdi,%%rcx), %%xmm7  \n\t" \
        "pmullw     %%xmm4, %%xmm1          \n\t" \
        "pmullw     %%xmm6, %%xmm3          \n\t" \
        "paddw      %%xmm1, %%xmm3          \n\t" \
        "movdqa     %%xmm7, %%xmm1          \n\t" \
        "movdqa     %%xmm9, %%xmm6          \n\t" \
        "paddw      %%xmm5, %%xmm3          \n\t" \
        "psrldq     $2, %%xmm1              \n\t" \
        "psraw      $7, %%xmm3              \n\t" \
        "paddw      %%xmm5, %%xmm5          \n\t" \
        "packuswb   %%xmm3, %%xmm3          \n\t" \
        "add        $8, %%rdi               \n\t" \
        "jg         30f                     \n\t" \
        "movq       %%xmm3, (%%rdi,%%rax)   \n\t" \
        "jnz        12b                     \n\t" \
        "jmp        31f                     \n\t" \
        ".align     0x10                    \n\t" \
        "29:                                \n\t" \
        "movq       (%%rdi,%%rbx), %%xmm4   \n\t" \
        "movq       (%%rdi,%%rbp), %%xmm2   \n\t" \
        "pxor       %%xmm0, %%xmm0          \n\t" \
        "punpcklbw  %%xmm0, %%xmm7          \n\t" \
        "punpcklbw  %%xmm0, %%xmm1          \n\t" \
        "punpcklbw  %%xmm0, %%xmm4          \n\t" \
        "punpcklbw  %%xmm0, %%xmm2          \n\t" \
        "psubw      %%xmm1, %%xmm7          \n\t" \
        "psubw      %%xmm2, %%xmm4          \n\t" \
        "psllw      $7, %%xmm7              \n\t" \
        "psllw      $7, %%xmm4              \n\t" \
        "pmulhw     %%xmm6, %%xmm7          \n\t" \
        "pmulhw     %%xmm6, %%xmm4          \n\t" \
        "movd       %%edi, %%xmm6           \n\t"
        "pmaxsw     %%xmm10, %%xmm4         \n\t" \
        "pminsw     %%xmm11, %%xmm4         \n\t" \
        "pshufd     $0, %%xmm6, %%xmm6      \n\t" \
    : \
    : \
    : "memory", "cc", "%edi", "%r8", "%rbp", "%rdi", "%rax", "%rbx", "%rcx", "%rdx", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4",
      "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13" );
#if SMAGL
      asm volatile ( \
        "pslld      (%[smagl]), %%xmm6      \n\t" \
    : \
    : [smagl] "r" ((int)SMAGL) \
    : "memory", "cc", "%xmm6" );
#endif
      asm volatile ( \
        "pcmpeqw    %%xmm0, %%xmm0          \n\t" \
        "psrlw      $9, %%xmm0              \n\t" \
        "movdqa     %%xmm4, %%xmm2          \n\t" \
        "movdqa     %%xmm7, %%xmm1          \n\t" \
    : \
    : \
    : "memory", "cc", "%xmm0", "%xmm1", "%xmm2", "%xmm4", "%xmm7" );
#if SMAGL
      asm volatile ( \
        "psllw      (%[smagl]), %%xmm4      \n\t" \
        "psllw      (%[smagl]), %%xmm7      \n\t" \
    : \
    : [smagl] "r" ((int)SMAGL) \
    : "memory", "cc", "%xmm4", "%xmm7" );
#endif
      asm volatile ( \
        "pand       %%xmm0, %%xmm4          \n\t" \
        "pand       %%xmm0, %%xmm7          \n\t" \
        "psraw      (%[smagl7]), %%xmm1     \n\t" \
        "packssdw   %%xmm6, %%xmm6          \n\t" \
        "paddsw     %%xmm6, %%xmm1          \n\t" \
        "movdqa     %%xmm8, %%xmm6          \n\t" \
        "movdqa     %%xmm13, %%xmm0         \n\t" \
        "movdqa     %%xmm12, %%xmm3         \n\t" \
        "pcmpgtw    %%xmm1, %%xmm0          \n\t" \
        "pcmpgtw    %%xmm1, %%xmm3          \n\t" \
        "pmaxsw     %%xmm13, %%xmm1         \n\t" \
        "pminsw     %%xmm12, %%xmm1         \n\t" \
        "pand       %%xmm0, %%xmm7          \n\t" \
        "pandn      %%xmm7, %%xmm3          \n\t" \
        "psraw      (%[smagl7]), %%xmm2     \n\t" \
        "movdqa     %%xmm2, %%xmm7          \n\t" \
        "punpcklwd  %%xmm1, %%xmm2          \n\t" \
        "punpckhwd  %%xmm1, %%xmm7          \n\t" \
        "pmaddwd	%%xmm6, %%xmm2          \n\t" \
        "pmaddwd	%%xmm6, %%xmm7          \n\t" \
        "movdqa     %%xmm9, %%xmm6          \n\t" \
        "psignw     %%xmm5, %%xmm3          \n\t" \
        "psignw     %%xmm5, %%xmm4          \n\t" \
        "packsswb   %%xmm5, %%xmm5          \n\t" \
        "packsswb   %%xmm3, %%xmm3          \n\t" \
        "packsswb   %%xmm4, %%xmm4          \n\t" \
        "movdqa     %%xmm5, %%xmm0          \n\t" \
        "movdqa     %%xmm5, %%xmm1          \n\t" \
        "psubb      %%xmm3, %%xmm0          \n\t" \
        "psubb      %%xmm4, %%xmm1          \n\t" \
        "psrlw      $9, %%xmm5              \n\t" \
        "punpcklbw  %%xmm3, %%xmm0          \n\t" \
        "punpcklbw  %%xmm4, %%xmm1          \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $0, (%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $0, (%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
    : \
    : [smagl7] "r" (7-(int)SMAGL) \
    : "memory", "cc", "%eax", "%rax", "%rdx", "%rsi", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm12", "%xmm13" );
#if SMAGL
      asm volatile ( \
        "movd       %%xmm2, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $1, 4(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $1, 4(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $2, 8(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $2, 8(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $3, 12(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $3, 12(%%rax,%%rdx), %%xmm4 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $4, 16(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $4, 16(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $5, 20(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $5, 20(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $6, 24(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $6, 24(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $7, 28(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $7, 28(%%rax,%%rdx), %%xmm4 \n\t" \
    : \
    : \
    : "memory", "cc", "%eax", "%rax", "%rdx", "%rsi", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4",
      "%xmm5", "%xmm6", "%xmm7" );
#else
      asm volatile ( \
        "movd       %%xmm2, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $1, 1(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $1, 1(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $2, 2(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $2, 2(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm2              \n\t" \
        "movd       %%xmm2, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $3, 3(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $3, 3(%%rax,%%rdx), %%xmm4 \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $4, 4(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $4, 4(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $5, 5(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $5, 5(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $6, 6(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $6, 6(%%rax,%%rdx), %%xmm4 \n\t" \
        "psrldq     $4, %%xmm7              \n\t" \
        "movd       %%xmm7, %%eax           \n\t" \
        "movsxd     %%eax, %%rax            \n\t" \
        "pinsrw     $7, 7(%%rax,%%rsi), %%xmm3 \n\t" \
        "pinsrw     $7, 7(%%rax,%%rdx), %%xmm4 \n\t" \
    : \
    : \
    : "memory", "cc", "%eax", "%rax", "%rdx", "%rsi", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4",
      "%xmm5", "%xmm6", "%xmm7" );
#endif
      asm volatile ( \
        "pcmpeqw    %%xmm2, %%xmm2          \n\t" \
        "movdqu     7(%%rdi,%%rcx), %%xmm7  \n\t" \
        "pmaddubsw  %%xmm0, %%xmm3          \n\t" \
        "pmaddubsw  %%xmm0, %%xmm4          \n\t" \
        "mov        %%r8, %%rax             \n\t" \
        "psignw     %%xmm2, %%xmm3          \n\t" \
        "psignw     %%xmm2, %%xmm4          \n\t" \
        "paddw      %%xmm5, %%xmm3          \n\t" \
        "paddw      %%xmm5, %%xmm4          \n\t" \
        "psraw      $7, %%xmm3              \n\t" \
        "psraw      $7, %%xmm4              \n\t" \
        "packuswb   %%xmm3, %%xmm3          \n\t" \
        "packuswb   %%xmm4, %%xmm4          \n\t" \
        "punpcklbw  %%xmm4, %%xmm3          \n\t" \
        "pmaddubsw  %%xmm1, %%xmm3          \n\t" \
        "palignr    $2, %%xmm7, %%xmm1      \n\t" \
        "psignw     %%xmm2, %%xmm3          \n\t" \
        "paddw      %%xmm5, %%xmm3          \n\t" \
        "psraw      $7, %%xmm3              \n\t" \
        "psllw      $9, %%xmm5              \n\t" \
        "packuswb   %%xmm3, %%xmm3          \n\t" \
        "add        $8, %%rdi               \n\t" \
        "jg         30f                     \n\t" \
        "movq       %%xmm3, (%%rdi,%%rax)   \n\t" \
        "jnz        29b                     \n\t" \
        "jmp        31f                     \n\t" \
        "30:                                \n\t" \
        "movd       %%xmm3, (%%rdi,%%rax)   \n\t" \
        "31:                                \n\t" \
        "pop        %%rbp                   \n\t" \
    : \
    : \
    : "memory", "cc", "%eax", "%r8", "%rbp", "%rax", "%rcx", "%rdi", "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4",
      "%xmm5", "%xmm6", "%xmm7" );
      psrc += src_pitch*SMAG;
      pedg += edg_pitch;
      pdst += dst_pitch;
    } // for y
  }
}
#undef MERGE
#undef MERGE2
#undef SMAG
#undef SMAGL
