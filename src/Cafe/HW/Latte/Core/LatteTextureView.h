#pragma once

class LatteTextureView
{
public:
	enum class MagFilter
	{
		kLinear,
		kNearestNeighbor,
	};

	LatteTextureView(class LatteTexture* texture, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, bool registerView = true);
	virtual ~LatteTextureView();

	void CreateLookupForSubTexture(uint32 mipStart, uint32 sliceStart);

	class LatteTexture* baseTexture;
	// view definition
	// note that a view can be addressed by more than one combination of physAddress/firstMip/firstSlice
	// thus the firstMip and firstSlice members of this class may not match the ones that were set in the GPU registers when this view was looked up
	sint32 firstMip;
	sint32 numMip;
	sint32 firstSlice;
	sint32 numSlice;
	Latte::E_GX2SURFFMT format; // format of view, can differ from base texture
	Latte::E_DIM dim; // dimension of view
	// state
	uint32 lastTextureBindIndex = 0;
	// FBO association
	std::vector<class LatteCachedFBO*> list_fboLookup; // only set for the first color texture of each FBO, or the depth texture if no color textures are present
	std::vector<class LatteCachedFBO*> list_associatedFbo; // list of cached fbos that reference this texture view
	// view lookup cache
	std::vector<uint32> viewLookUpCacheKeys;
	std::vector<uint32> viewLookUpCacheKeysNoRes;
};

class LatteTextureViewLookupCache
{
public:
	static void Add(LatteTextureView* view, uint32 baseMip = 0, uint32 baseSlice = 0);
	static void RemoveAll(LatteTextureView* view);

	static LatteTextureView* lookup(MPTR physAddr, sint32 width, sint32 height, sint32 depth, sint32 pitch, sint32 firstMip, sint32 numMip, sint32 firstSlice, sint32 numSlice, Latte::E_GX2SURFFMT format, Latte::E_DIM dim);
	static LatteTextureView* lookupWithColorOrDepthType(MPTR physAddr, sint32 width, sint32 height, sint32 depth, sint32 pitch, sint32 firstMip, sint32 numMip, sint32 firstSlice, sint32 numSlice, Latte::E_GX2SURFFMT format, Latte::E_DIM dim, bool isDepth);
	static LatteTextureView* lookupSlice(MPTR physAddr, sint32 width, sint32 height, sint32 pitch, sint32 firstMip, sint32 firstSlice, Latte::E_GX2SURFFMT format);
	static LatteTextureView* lookupSliceMinSize(MPTR physAddr, sint32 minWidth, sint32 minHeight, sint32 pitch, sint32 firstMip, sint32 firstSlice, Latte::E_GX2SURFFMT format);
	static LatteTextureView* lookupSliceEx(MPTR physAddr, sint32 width, sint32 height, sint32 pitch, sint32 firstMip, sint32 firstSlice, Latte::E_GX2SURFFMT format, bool isDepth);

	static std::unordered_set<LatteTextureView*> GetAllViews();

	
};