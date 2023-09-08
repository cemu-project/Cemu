#pragma once
// todo - this file is superseded by LatteReg.h

#include "Cafe/HW/Latte/ISA/LatteReg.h"

#define mmPA_CL_VS_OUT_CNTL                             0xA207

#define mmPA_SU_LINE_CNTL                               0xA282
#define mmPA_SU_POLY_OFFSET_DB_FMT_CNTL                 0xA37E

#define mmPA_SC_WINDOW_OFFSET                           0xA080
#define mmPA_SC_AA_CONFIG                               0xA301
#define mmPA_SC_AA_MASK                                 0xA312
#define mmPA_SC_AA_SAMPLE_LOCS_MCTX                     0xA307
#define mmPA_SC_AA_SAMPLE_LOCS_8S_WD1_MCTX              0xA308
#define mmPA_SC_LINE_STIPPLE                            0xA283
#define mmPA_SC_LINE_CNTL                               0xA300
#define mmPA_SC_SCREEN_SCISSOR_TL                       0xA00C
#define mmPA_SC_SCREEN_SCISSOR_BR                       0xA00D
#define mmPA_SC_WINDOW_SCISSOR_TL                       0xA081
#define mmPA_SC_WINDOW_SCISSOR_BR                       0xA082
#define mmPA_SC_CLIPRECT_RULE                           0xA083
#define mmPA_SC_CLIPRECT_0_TL                           0xA084
#define mmPA_SC_CLIPRECT_0_BR                           0xA085
#define mmPA_SC_CLIPRECT_1_TL                           0xA086
#define mmPA_SC_CLIPRECT_1_BR                           0xA087
#define mmPA_SC_CLIPRECT_2_TL                           0xA088
#define mmPA_SC_CLIPRECT_2_BR                           0xA089
#define mmPA_SC_CLIPRECT_3_TL                           0xA08A
#define mmPA_SC_CLIPRECT_3_BR                           0xA08B
#define mmPA_SC_EDGERULE                                0xA08C

#define mmPA_SC_MODE_CNTL                               0xA293
#define mmPA_SC_MPASS_PS_CNTL                           0xA292

#define mmVGT_DRAW_INITIATOR                            0xA1FC
#define mmVGT_EVENT_INITIATOR                           0xA2A4
#define mmVGT_EVENT_ADDRESS_REG                         0xA1FE
#define mmVGT_DMA_BASE_HI                               0xA1F9
#define mmVGT_DMA_BASE                                  0xA1FA
#define mmVGT_DMA_INDEX_TYPE                            0xA29F
#define mmVGT_DMA_NUM_INSTANCES                         0xA2A2
#define mmVGT_DMA_SIZE                                  0xA29D

#define mmVGT_IMMED_DATA                                0xA1FD
#define mmVGT_INDEX_TYPE                                0x2257
#define mmVGT_NUM_INDICES                               0x225C
#define mmVGT_NUM_INSTANCES                             0x225D
#define mmVGT_PRIMITIVE_TYPE                            0x2256
#define mmVGT_PRIMITIVEID_EN                            0xA2A1
#define mmVGT_VTX_CNT_EN                                0xA2AE
#define mmVGT_REUSE_OFF                                 0xA2AD
#define mmVGT_MAX_VTX_INDX                              0xA100
#define mmVGT_MIN_VTX_INDX                              0xA101
#define mmVGT_INDX_OFFSET                               0xA102
#define mmVGT_VERTEX_REUSE_BLOCK_CNTL                   0xA316
#define mmVGT_OUT_DEALLOC_CNTL                          0xA317
#define mmVGT_MULTI_PRIM_IB_RESET_EN                    0xA2A5
#define mmVGT_ENHANCE                                   0xA294
#define mmVGT_OUTPUT_PATH_CNTL                          0xA284
#define mmVGT_HOS_CNTL                                  0xA285
#define mmVGT_HOS_MAX_TESS_LEVEL                        0xA286
#define mmVGT_HOS_MIN_TESS_LEVEL                        0xA287
#define mmVGT_HOS_REUSE_DEPTH                           0xA288
#define mmVGT_GROUP_PRIM_TYPE                           0xA289
#define mmVGT_GROUP_FIRST_DECR                          0xA28A
#define mmVGT_GROUP_DECR                                0xA28B
#define mmVGT_GROUP_VECT_0_CNTL                         0xA28C
#define mmVGT_GROUP_VECT_1_CNTL                         0xA28D
#define mmVGT_GROUP_VECT_0_FMT_CNTL                     0xA28E
#define mmVGT_GROUP_VECT_1_FMT_CNTL                     0xA28F
#define mmVGT_GS_OUT_PRIM_TYPE                          0xA29B

#define mmVGT_STRMOUT_EN                                0xA2AC
#define mmVGT_STRMOUT_BUFFER_SIZE_0                     0xA2B4
#define mmVGT_STRMOUT_BUFFER_SIZE_1                     0xA2B8
#define mmVGT_STRMOUT_BUFFER_SIZE_2                     0xA2BC
#define mmVGT_STRMOUT_BUFFER_SIZE_3                     0xA2C0
#define mmVGT_STRMOUT_BUFFER_OFFSET_0                   0xA2B7
#define mmVGT_STRMOUT_BUFFER_OFFSET_1                   0xA2BB
#define mmVGT_STRMOUT_BUFFER_OFFSET_2                   0xA2BF
#define mmVGT_STRMOUT_BUFFER_OFFSET_3                   0xA2C3
#define mmVGT_STRMOUT_VTX_STRIDE_0                      0xA2B5
#define mmVGT_STRMOUT_VTX_STRIDE_1                      0xA2B9
#define mmVGT_STRMOUT_VTX_STRIDE_2                      0xA2BD
#define mmVGT_STRMOUT_VTX_STRIDE_3                      0xA2C1
#define mmVGT_STRMOUT_BUFFER_BASE_0                     0xA2B6
#define mmVGT_STRMOUT_BUFFER_BASE_1                     0xA2BA
#define mmVGT_STRMOUT_BUFFER_BASE_2                     0xA2BE
#define mmVGT_STRMOUT_BUFFER_BASE_3                     0xA2C2
#define mmVGT_STRMOUT_BUFFER_EN                         0xA2C8
#define mmVGT_STRMOUT_BASE_OFFSET_0                     0xA2C4
#define mmVGT_STRMOUT_BASE_OFFSET_1                     0xA2C5
#define mmVGT_STRMOUT_BASE_OFFSET_2                     0xA2C6
#define mmVGT_STRMOUT_BASE_OFFSET_3                     0xA2C7
#define mmVGT_STRMOUT_BASE_OFFSET_HI_0                  0xA2D1
#define mmVGT_STRMOUT_BASE_OFFSET_HI_1                  0xA2D2
#define mmVGT_STRMOUT_BASE_OFFSET_HI_2                  0xA2D3
#define mmVGT_STRMOUT_BASE_OFFSET_HI_3                  0xA2D4
#define mmVGT_STRMOUT_DRAW_OPAQUE_OFFSET                0xA2CA
#define mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE    0xA2CB
#define mmVGT_STRMOUT_DRAW_OPAQUE_VERTEX_STRIDE         0xA2CC

#define mmSQ_PGM_START_PS                               0xA210
#define mmSQ_PGM_CF_OFFSET_PS                           0xA233
#define mmSQ_PGM_RESOURCES_PS                           0xA214
#define mmSQ_PGM_EXPORTS_PS                             0xA215
#define mmSQ_PGM_START_VS                               0xA216
#define mmSQ_PGM_CF_OFFSET_VS                           0xA234
#define mmSQ_PGM_RESOURCES_VS                           0xA21A
#define mmSQ_PGM_START_GS                               0xA21B
#define mmSQ_PGM_CF_OFFSET_GS                           0xA235
#define mmSQ_PGM_RESOURCES_GS                           0xA21F
#define mmSQ_PGM_START_ES                               0xA220
#define mmSQ_PGM_CF_OFFSET_ES                           0xA236
#define mmSQ_PGM_RESOURCES_ES                           0xA224
#define mmSQ_PGM_START_FS                               0xA225
#define mmSQ_PGM_CF_OFFSET_FS                           0xA237
#define mmSQ_PGM_RESOURCES_FS                           0xA229
#define mmSQ_ESGS_RING_ITEMSIZE                         0xA22A
#define mmSQ_GSVS_RING_ITEMSIZE                         0xA22B
#define mmSQ_ESTMP_RING_ITEMSIZE                        0xA22C
#define mmSQ_GSTMP_RING_ITEMSIZE                        0xA22D
#define mmSQ_VSTMP_RING_ITEMSIZE                        0xA22E
#define mmSQ_PSTMP_RING_ITEMSIZE                        0xA22F
#define mmSQ_FBUF_RING_ITEMSIZE                         0xA230
#define mmSQ_REDUC_RING_ITEMSIZE                        0xA231
#define mmSQ_GS_VERT_ITEMSIZE                           0xA232
#define mmSQ_VTX_SEMANTIC_CLEAR                         0xA238

#define mmSQ_VTX_SEMANTIC_0                             0xA0E0
#define mmSQ_VTX_SEMANTIC_1                             0xA0E1
#define mmSQ_VTX_SEMANTIC_2                             0xA0E2
#define mmSQ_VTX_SEMANTIC_3                             0xA0E3
#define mmSQ_VTX_SEMANTIC_4                             0xA0E4
#define mmSQ_VTX_SEMANTIC_5                             0xA0E5
#define mmSQ_VTX_SEMANTIC_6                             0xA0E6
#define mmSQ_VTX_SEMANTIC_7                             0xA0E7
#define mmSQ_VTX_SEMANTIC_8                             0xA0E8
#define mmSQ_VTX_SEMANTIC_9                             0xA0E9
#define mmSQ_VTX_SEMANTIC_10                            0xA0EA
#define mmSQ_VTX_SEMANTIC_11                            0xA0EB
#define mmSQ_VTX_SEMANTIC_12                            0xA0EC
#define mmSQ_VTX_SEMANTIC_13                            0xA0ED
#define mmSQ_VTX_SEMANTIC_14                            0xA0EE
#define mmSQ_VTX_SEMANTIC_15                            0xA0EF
#define mmSQ_VTX_SEMANTIC_16                            0xA0F0
#define mmSQ_VTX_SEMANTIC_17                            0xA0F1
#define mmSQ_VTX_SEMANTIC_18                            0xA0F2
#define mmSQ_VTX_SEMANTIC_19                            0xA0F3
#define mmSQ_VTX_SEMANTIC_20                            0xA0F4
#define mmSQ_VTX_SEMANTIC_21                            0xA0F5
#define mmSQ_VTX_SEMANTIC_22                            0xA0F6
#define mmSQ_VTX_SEMANTIC_23                            0xA0F7
#define mmSQ_VTX_SEMANTIC_24                            0xA0F8
#define mmSQ_VTX_SEMANTIC_25                            0xA0F9
#define mmSQ_VTX_SEMANTIC_26                            0xA0FA
#define mmSQ_VTX_SEMANTIC_27                            0xA0FB
#define mmSQ_VTX_SEMANTIC_28                            0xA0FC
#define mmSQ_VTX_SEMANTIC_29                            0xA0FD
#define mmSQ_VTX_SEMANTIC_30                            0xA0FE
#define mmSQ_VTX_SEMANTIC_31                            0xA0FF

#define mmSPI_VS_OUT_ID_0                               0xA185
#define mmSPI_VS_OUT_ID_1                               0xA186
#define mmSPI_VS_OUT_ID_2                               0xA187
#define mmSPI_VS_OUT_ID_3                               0xA188
#define mmSPI_VS_OUT_ID_4                               0xA189
#define mmSPI_VS_OUT_ID_5                               0xA18A
#define mmSPI_VS_OUT_ID_6                               0xA18B
#define mmSPI_VS_OUT_ID_7                               0xA18C
#define mmSPI_VS_OUT_ID_8                               0xA18D
#define mmSPI_VS_OUT_ID_9                               0xA18E
#define mmSPI_PS_INPUT_CNTL_0                           0xA191
#define mmSPI_PS_INPUT_CNTL_1                           0xA192
#define mmSPI_PS_INPUT_CNTL_2                           0xA193
#define mmSPI_PS_INPUT_CNTL_3                           0xA194
#define mmSPI_PS_INPUT_CNTL_4                           0xA195
#define mmSPI_PS_INPUT_CNTL_5                           0xA196
#define mmSPI_PS_INPUT_CNTL_6                           0xA197
#define mmSPI_PS_INPUT_CNTL_7                           0xA198
#define mmSPI_PS_INPUT_CNTL_8                           0xA199
#define mmSPI_PS_INPUT_CNTL_9                           0xA19A
#define mmSPI_PS_INPUT_CNTL_10                          0xA19B
#define mmSPI_PS_INPUT_CNTL_11                          0xA19C
#define mmSPI_PS_INPUT_CNTL_12                          0xA19D
#define mmSPI_PS_INPUT_CNTL_13                          0xA19E
#define mmSPI_PS_INPUT_CNTL_14                          0xA19F
#define mmSPI_PS_INPUT_CNTL_15                          0xA1A0
#define mmSPI_PS_INPUT_CNTL_16                          0xA1A1
#define mmSPI_PS_INPUT_CNTL_17                          0xA1A2
#define mmSPI_PS_INPUT_CNTL_18                          0xA1A3
#define mmSPI_PS_INPUT_CNTL_19                          0xA1A4
#define mmSPI_PS_INPUT_CNTL_20                          0xA1A5
#define mmSPI_PS_INPUT_CNTL_21                          0xA1A6
#define mmSPI_PS_INPUT_CNTL_22                          0xA1A7
#define mmSPI_PS_INPUT_CNTL_23                          0xA1A8
#define mmSPI_PS_INPUT_CNTL_24                          0xA1A9
#define mmSPI_PS_INPUT_CNTL_25                          0xA1AA
#define mmSPI_PS_INPUT_CNTL_26                          0xA1AB
#define mmSPI_PS_INPUT_CNTL_27                          0xA1AC
#define mmSPI_PS_INPUT_CNTL_28                          0xA1AD
#define mmSPI_PS_INPUT_CNTL_29                          0xA1AE
#define mmSPI_PS_INPUT_CNTL_30                          0xA1AF
#define mmSPI_PS_INPUT_CNTL_31                          0xA1B0
#define mmSPI_VS_OUT_CONFIG                             0xA1B1
#define mmSPI_THREAD_GROUPING                           0xA1B2
#define mmSPI_PS_IN_CONTROL_0                           0xA1B3
#define mmSPI_PS_IN_CONTROL_1                           0xA1B4
#define mmSPI_INTERP_CONTROL_0                          0xA1B5
#define mmSPI_INPUT_Z                                   0xA1B6

#define mmDB_DEPTH_BASE                                 0xA003
#define mmDB_DEPTH_INFO                                 0xA004
#define mmDB_HTILE_DATA_BASE                            0xA005
#define mmDB_DEPTH_SIZE                                 0xA000
#define mmDB_DEPTH_VIEW                                 0xA001
#define mmDB_RENDER_CONTROL                             0xA343
#define mmDB_RENDER_OVERRIDE                            0xA344
#define mmDB_SHADER_CONTROL                             0xA203
#define mmDB_STENCIL_CLEAR                              0xA00A
#define mmDB_DEPTH_CLEAR                                0xA00B
#define mmDB_HTILE_SURFACE                              0xA349
#define mmDB_PRELOAD_CONTROL                            0xA34C
#define mmDB_PREFETCH_LIMIT                             0xA34D
#define mmDB_STENCILREFMASK                             0xA10C
#define mmDB_STENCILREFMASK_BF                          0xA10D
#define mmDB_SRESULTS_COMPARE_STATE0                    0xA34A
#define mmDB_SRESULTS_COMPARE_STATE1                    0xA34B

#define mmDB_ALPHA_TO_MASK                              0xA351

#define mmCB_CLRCMP_CONTROL                             0xA30C
#define mmCB_CLRCMP_SRC                                 0xA30D
#define mmCB_CLRCMP_DST                                 0xA30E
#define mmCB_CLRCMP_MSK                                 0xA30F
#define mmCB_COLOR0_BASE                                0xA010
#define mmCB_COLOR1_BASE                                0xA011
#define mmCB_COLOR2_BASE                                0xA012
#define mmCB_COLOR3_BASE                                0xA013
#define mmCB_COLOR4_BASE                                0xA014
#define mmCB_COLOR5_BASE                                0xA015
#define mmCB_COLOR6_BASE                                0xA016
#define mmCB_COLOR7_BASE                                0xA017
#define mmCB_COLOR0_SIZE                                0xA018
#define mmCB_COLOR1_SIZE                                0xA019
#define mmCB_COLOR2_SIZE                                0xA01A
#define mmCB_COLOR3_SIZE                                0xA01B
#define mmCB_COLOR4_SIZE                                0xA01C
#define mmCB_COLOR5_SIZE                                0xA01D
#define mmCB_COLOR6_SIZE                                0xA01E
#define mmCB_COLOR7_SIZE                                0xA01F
#define mmCB_COLOR0_VIEW                                0xA020
#define mmCB_COLOR1_VIEW                                0xA021
#define mmCB_COLOR2_VIEW                                0xA022
#define mmCB_COLOR3_VIEW                                0xA023
#define mmCB_COLOR4_VIEW                                0xA024
#define mmCB_COLOR5_VIEW                                0xA025
#define mmCB_COLOR6_VIEW                                0xA026
#define mmCB_COLOR7_VIEW                                0xA027
#define mmCB_COLOR0_INFO                                0xA028
#define mmCB_COLOR1_INFO                                0xA029
#define mmCB_COLOR2_INFO                                0xA02A
#define mmCB_COLOR3_INFO                                0xA02B
#define mmCB_COLOR4_INFO                                0xA02C
#define mmCB_COLOR5_INFO                                0xA02D
#define mmCB_COLOR6_INFO                                0xA02E
#define mmCB_COLOR7_INFO                                0xA02F
#define mmCB_COLOR0_TILE                                0xA030
#define mmCB_COLOR1_TILE                                0xA031
#define mmCB_COLOR2_TILE                                0xA032
#define mmCB_COLOR3_TILE                                0xA033
#define mmCB_COLOR4_TILE                                0xA034
#define mmCB_COLOR5_TILE                                0xA035
#define mmCB_COLOR6_TILE                                0xA036
#define mmCB_COLOR7_TILE                                0xA037
#define mmCB_COLOR0_FRAG                                0xA038
#define mmCB_COLOR1_FRAG                                0xA039
#define mmCB_COLOR2_FRAG                                0xA03A
#define mmCB_COLOR3_FRAG                                0xA03B
#define mmCB_COLOR4_FRAG                                0xA03C
#define mmCB_COLOR5_FRAG                                0xA03D
#define mmCB_COLOR6_FRAG                                0xA03E
#define mmCB_COLOR7_FRAG                                0xA03F
#define mmCB_COLOR0_MASK                                0xA040
#define mmCB_COLOR1_MASK                                0xA041
#define mmCB_COLOR2_MASK                                0xA042
#define mmCB_COLOR3_MASK                                0xA043
#define mmCB_COLOR4_MASK                                0xA044
#define mmCB_COLOR5_MASK                                0xA045
#define mmCB_COLOR6_MASK                                0xA046
#define mmCB_COLOR7_MASK                                0xA047
#define mmCB_SHADER_MASK                                0xA08F
#define mmCB_SHADER_CONTROL                             0xA1E8

#define mmSQ_VTX_BASE_VTX_LOC                           0xF3FC // baseVertex
#define mmSQ_VTX_START_INST_LOC                         0xF3FD // baseInstance

#define mmSQ_CONFIG										0x2300 // used by GX2SetShaderModeEx
#define mmSQ_GPR_RESOURCE_MGMT_1						0x2301
#define mmSQ_THREAD_RESOURCE_MGMT						0x2303
#define mmSQ_STACK_RESOURCE_MGMT_1						0x2304
#define mmSQ_STACK_RESOURCE_MGMT_2						0x2305
#define mmSQ_ESGS_RING_BASE								0x2310
#define mmSQ_ESGS_RING_SIZE								0x2311
#define mmSQ_GSVS_RING_BASE								0x2312
#define mmSQ_GSVS_RING_SIZE								0x2313
#define mmSQ_ESTMP_RING_BASE							0x2314
#define mmSQ_ESTMP_RING_SIZE							0x2315
#define mmSQ_GSTMP_RING_BASE							0x2316
#define mmSQ_GSTMP_RING_SIZE							0x2317


#define mmSQ_TEX_RESOURCE_WORD0							0xE000
#define mmSQ_ALU_CONSTANT0_0							0xC000

#define mmSQ_VTX_ATTRIBUTE_BLOCK_START					(mmSQ_TEX_RESOURCE_WORD0+0x8C0)
#define mmSQ_VTX_ATTRIBUTE_BLOCK_END					(mmSQ_VTX_ATTRIBUTE_BLOCK_START + 7*16)

#define mmSQ_VTX_UNIFORM_BLOCK_START					(mmSQ_TEX_RESOURCE_WORD0+0x7E0) // 7 dwords for each uniform block
#define mmSQ_VTX_UNIFORM_BLOCK_END						(mmSQ_VTX_UNIFORM_BLOCK_START + 7*16 - 1)

#define mmSQ_PS_UNIFORM_BLOCK_START						(mmSQ_TEX_RESOURCE_WORD0+0x250) // 7 dwords for each uniform block
#define mmSQ_PS_UNIFORM_BLOCK_END						(mmSQ_PS_UNIFORM_BLOCK_START + 7*16 - 1)

#define mmSQ_GS_UNIFORM_BLOCK_START						(mmSQ_TEX_RESOURCE_WORD0+0xCB0) // 7 dwords for each uniform block
#define mmSQ_GS_UNIFORM_BLOCK_END						(mmSQ_GS_UNIFORM_BLOCK_START + 7*16 - 1)

#define mmSQ_CS_DISPATCH_PARAMS							(mmSQ_TEX_RESOURCE_WORD0+0x865)