#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"

namespace GX2
{
	struct GX2AlphaTestReg
	{
		betype<Latte::LATTE_SX_ALPHA_TEST_CONTROL> regAlphaTestControl;
		betype<Latte::LATTE_SX_ALPHA_REF> regAlphaTestRef;
	};

	static_assert(sizeof(GX2AlphaTestReg) == 8);

	struct GX2ColorControlReg
	{
		betype<Latte::LATTE_CB_COLOR_CONTROL> reg;
	};

	static_assert(sizeof(GX2ColorControlReg) == 4);

	struct GX2PolygonControlReg
	{
		betype<Latte::LATTE_PA_SU_SC_MODE_CNTL> reg;
	};

	static_assert(sizeof(GX2PolygonControlReg) == 4);

	struct GX2PolygonOffsetReg
	{
		betype<Latte::LATTE_PA_SU_POLY_OFFSET_FRONT_SCALE> regFrontScale;
		betype<Latte::LATTE_PA_SU_POLY_OFFSET_FRONT_OFFSET> regFrontOffset;
		betype<Latte::LATTE_PA_SU_POLY_OFFSET_BACK_SCALE> regBackScale;
		betype<Latte::LATTE_PA_SU_POLY_OFFSET_BACK_OFFSET> regBackOffset;
		betype<Latte::LATTE_PA_SU_POLY_OFFSET_CLAMP> regClamp;
	};

	static_assert(sizeof(GX2PolygonOffsetReg) == 0x14);

	struct GX2DepthStencilControlReg
	{
		betype<Latte::LATTE_DB_DEPTH_CONTROL> reg;
	};

	static_assert(sizeof(GX2DepthStencilControlReg) == 4);

	struct GX2StencilMaskReg
	{
		betype<Latte::LATTE_DB_STENCILREFMASK> stencilRefMaskFrontReg;
		betype<Latte::LATTE_DB_STENCILREFMASK_BF> stencilRefMaskBackReg;
	};
	
	static_assert(sizeof(GX2StencilMaskReg) == 8);

	struct GX2TargetChannelMaskReg
	{
		betype<Latte::LATTE_CB_TARGET_MASK> reg;
	};

	static_assert(sizeof(GX2TargetChannelMaskReg) == 4);

	struct GX2HIStencilInfoData
	{
		/* +0x00 */ uint32be ukn00;
		/* +0x04 */ uint8be ukn04;
		/* +0x05 */ uint8be ukn05;
		/* +0x06 */ uint8be ukn06; // probably padding?
		/* +0x07 */ uint8be ukn07; // probably padding?
		/* +0x08 */ uint32be isEnable; // 0 or 1
	};

	static_assert(sizeof(GX2HIStencilInfoData) == 0xC);

	struct GX2HiStencilInfoReg
	{
		GX2HIStencilInfoData state[2];
		uint32be reg[2]; // DB_SRESULTS_COMPARE_STATE0 and DB_SRESULTS_COMPARE_STATE1
	};

	static_assert(sizeof(GX2HiStencilInfoReg) == 0x20);

	struct GX2BlendControlReg
	{
		uint32be index;
		betype<Latte::LATTE_CB_BLENDN_CONTROL> reg;
	};

	static_assert(sizeof(GX2BlendControlReg) == 8);

	struct GX2BlendConstantColorReg
	{
		betype<Latte::LATTE_CB_BLEND_RED> regRed;
		betype<Latte::LATTE_CB_BLEND_GREEN> regGreen;
		betype<Latte::LATTE_CB_BLEND_BLUE> regBlue;
		betype<Latte::LATTE_CB_BLEND_ALPHA> regAlpha;
	};

	static_assert(sizeof(GX2BlendConstantColorReg) == 16);

	struct GX2PointSizeReg
	{
		betype<Latte::LATTE_PA_SU_POINT_SIZE> reg;
	};

	static_assert(sizeof(GX2PointSizeReg) == 4);

	struct GX2PointLimitsReg
	{
		betype<Latte::LATTE_PA_SU_POINT_MINMAX> reg;
	};

	static_assert(sizeof(GX2PointLimitsReg) == 4);

	struct GX2ViewportReg
	{
		betype<Latte::LATTE_PA_CL_VPORT_XSCALE> xScale;
		betype<Latte::LATTE_PA_CL_VPORT_XOFFSET> xOffset;
		betype<Latte::LATTE_PA_CL_VPORT_YSCALE> yScale;
		betype<Latte::LATTE_PA_CL_VPORT_YOFFSET> yOffset;
		betype<Latte::LATTE_PA_CL_VPORT_ZSCALE> zScale;
		betype<Latte::LATTE_PA_CL_VPORT_ZOFFSET> zOffset;
		uint32 ukn[6]; // clipping registers?
	};

	static_assert(sizeof(GX2ViewportReg) == 48);

	struct GX2ScissorReg
	{
		betype<Latte::LATTE_PA_SC_GENERIC_SCISSOR_TL> scissorTL;
		betype<Latte::LATTE_PA_SC_GENERIC_SCISSOR_BR> scissorBR;
	};

	static_assert(sizeof(GX2ScissorReg) == 8);

	using GX2_ALPHAFUNC = Latte::LATTE_SX_ALPHA_TEST_CONTROL::E_ALPHA_FUNC; // alias Latte::E_COMPAREFUNC
	using GX2_LOGICOP = Latte::LATTE_CB_COLOR_CONTROL::E_LOGICOP;
	using GX2_CHANNELMASK = uint32;
	using GX2_BLENDFACTOR = Latte::LATTE_CB_BLENDN_CONTROL::E_BLENDFACTOR;
	using GX2_BLENDFUNC = Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC;

	void GX2InitAlphaTestReg(GX2AlphaTestReg* reg, uint32 alphaTestEnable, GX2_ALPHAFUNC alphaFunc, float alphaRef);
	void GX2SetAlphaTestReg(GX2AlphaTestReg* reg);
	void GX2SetAlphaTest(uint32 alphaTestEnable, GX2_ALPHAFUNC alphaFunc, float alphaRef);

	void GX2InitColorControlReg(GX2ColorControlReg* reg, GX2_LOGICOP logicOp, uint32 blendMask, uint32 multiwriteEnable, uint32 colorBufferEnable);
	void GX2SetColorControl(GX2_LOGICOP logicOp, uint32 blendMask, uint32 multiwriteEnable, uint32 colorBufferEnable);
	void GX2SetColorControlReg(GX2ColorControlReg* reg);

	void GX2InitPolygonControlReg(GX2PolygonControlReg* reg,
		Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE frontFace, uint32 cullFront,	uint32 cullBack,
		Latte::LATTE_PA_SU_SC_MODE_CNTL::E_POLYGONMODE usePolygonMode,
		Latte::LATTE_PA_SU_SC_MODE_CNTL::E_PTYPE polyModeFront,
		Latte::LATTE_PA_SU_SC_MODE_CNTL::E_PTYPE polyModeBack,
		uint32 polygonOffsetFrontEnable, uint32 polygonOffsetBackEnable, uint32 paraOffsetEnable);
	void GX2SetPolygonControlReg(GX2PolygonControlReg* reg);
	void GX2SetPolygonControl(Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE frontFace, uint32 cullFront, uint32 cullBack,
			Latte::LATTE_PA_SU_SC_MODE_CNTL::E_POLYGONMODE usePolygonMode,
			Latte::LATTE_PA_SU_SC_MODE_CNTL::E_PTYPE polyModeFront,
			Latte::LATTE_PA_SU_SC_MODE_CNTL::E_PTYPE polyModeBack,
			uint32 polygonOffsetFrontEnable, uint32 polygonOffsetBackEnable, uint32 paraOffsetEnable);
	void GX2SetCullOnlyControl(Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE frontFace, uint32 cullFront, uint32 cullBack);

	void GX2InitPolygonOffsetReg(GX2PolygonOffsetReg* reg, float frontOffset, float frontScale, float backOffset, float backScale, float clampOffset);
	void GX2SetPolygonOffsetReg(GX2PolygonOffsetReg* reg);
	void GX2SetPolygonOffset(float frontOffset, float frontScale, float backOffset, float backScale, float clampOffset);

	void GX2InitPointSizeReg(GX2PointSizeReg* reg, float width, float height);
	void GX2SetPointSizeReg(GX2PointSizeReg* reg);
	void GX2SetPointSize(float width, float height);
	void GX2InitPointLimitsReg(GX2PointLimitsReg* reg, float minSize, float maxSize);
	void GX2SetPointLimitsReg(GX2PointLimitsReg* reg);
	void GX2SetPointLimits(float minSize, float maxSize);

	void GX2SetRasterizerClipControl(bool enableRasterizer, bool enableZClip);
	void GX2SetRasterizerClipControlHalfZ(bool enableRasterizer, bool enableZClip, bool enableHalfZ);
	void GX2SetRasterizerClipControlEx(bool enableRasterizer, bool enableZClip, bool enableHalfZ);

	void GX2SetPrimitiveRestartIndex(uint32 restartIndex);

	void GX2InitTargetChannelMasksReg(GX2TargetChannelMaskReg* reg, GX2_CHANNELMASK t0, GX2_CHANNELMASK t1, GX2_CHANNELMASK t2, GX2_CHANNELMASK t3, GX2_CHANNELMASK t4, GX2_CHANNELMASK t5, GX2_CHANNELMASK t6, GX2_CHANNELMASK t7);
	void GX2SetTargetChannelMasksReg(GX2TargetChannelMaskReg* reg);
	void GX2SetTargetChannelMasks(GX2_CHANNELMASK t0, GX2_CHANNELMASK t1, GX2_CHANNELMASK t2, GX2_CHANNELMASK t3, GX2_CHANNELMASK t4, GX2_CHANNELMASK t5, GX2_CHANNELMASK t6, GX2_CHANNELMASK t7);
	
	void GX2InitBlendControlReg(GX2BlendControlReg* reg, uint32 renderTargetIndex, GX2_BLENDFACTOR colorSrcFactor, GX2_BLENDFACTOR colorDstFactor, GX2_BLENDFUNC colorCombineFunc, uint32 separateAlphaBlend, GX2_BLENDFACTOR alphaSrcFactor, GX2_BLENDFACTOR alphaDstFactor, GX2_BLENDFUNC alphaCombineFunc);
	void GX2SetBlendControlReg(GX2BlendControlReg* reg);
	void GX2SetBlendControl(uint32 renderTargetIndex, GX2_BLENDFACTOR colorSrcFactor, GX2_BLENDFACTOR colorDstFactor, GX2_BLENDFUNC colorCombineFunc, uint32 separateAlphaBlend, GX2_BLENDFACTOR alphaSrcFactor, GX2_BLENDFACTOR alphaDstFactor, GX2_BLENDFUNC alphaCombineFunc);

	void GX2InitBlendConstantColorReg(GX2BlendConstantColorReg* reg, float red, float green, float blue, float alpha);
	void GX2SetBlendConstantColorReg(GX2BlendConstantColorReg* reg);
	void GX2SetBlendConstantColor(float red, float green, float blue, float alpha);

	void GX2StateInit();
}