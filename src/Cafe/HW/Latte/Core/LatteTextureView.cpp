#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Core/LatteTextureView.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/GraphicPack/GraphicPack2.h"

LatteTextureView::LatteTextureView(LatteTexture* texture, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, bool registerView)
{
	this->baseTexture = texture;
	this->firstMip = firstMip;
	this->numMip = mipCount;
	this->firstSlice = firstSlice;
	this->numSlice = sliceCount;
	this->dim = dim;
	this->format = format;
	if (registerView)
	{
		texture->views.emplace_back(this);
		LatteTextureViewLookupCache::Add(this);
	}
}

LatteTextureView::~LatteTextureView()
{
	// unregister view
	LatteTextureViewLookupCache::RemoveAll(this);
	// remove from texture
	vectorRemoveByValue(baseTexture->views, this);
	if (baseTexture->baseView == this)
		baseTexture->baseView = nullptr;
	// delete all associated FBOs
	while (!list_associatedFbo.empty())
		LatteMRT::DeleteCachedFBO(list_associatedFbo[0]);
}

void LatteTextureView::CreateLookupForSubTexture(uint32 mipStart, uint32 sliceStart)
{
	cemu_assert_debug(mipStart != 0 || sliceStart != 0); // This function should never be called with both parameters zero. Every view creates a base lookup on construction
	LatteTextureViewLookupCache::Add(this, mipStart, sliceStart);
}

/* View lookup cache */

struct LatteTexViewLookupDesc
{
	LatteTexViewLookupDesc(LatteTextureView* view) : view(view)
	{
		this->physAddr = view->baseTexture->physAddress;
		this->physMipAddr = view->baseTexture->physMipAddress;
		this->width = view->baseTexture->width;
		this->height = view->baseTexture->height;
		this->pitch = view->baseTexture->pitch;
		this->firstMip = view->firstMip;
		this->numMip = view->numMip;
		this->firstSlice = view->firstSlice;
		this->numSlice = view->numSlice;
		this->format = view->format;
		this->dim = view->dim;
		this->isDepth = view->baseTexture->isDepth;
	}

	void SetParametersForSubTexture(sint32 baseMip, sint32 baseSlice)
	{
		cemu_assert_debug(baseMip >= 0);
		cemu_assert_debug(baseSlice >= 0);
		LatteTextureSliceMipInfo* sliceMipInfo = view->baseTexture->GetSliceMipArrayEntry(baseSlice, baseMip);
		physAddr = sliceMipInfo->addrStart;
		pitch = sliceMipInfo->pitch;
		cemu_assert_debug(format == view->baseTexture->format); // if the format is different then width/height calculation might differ. This only affects the case where an integer format is mapped onto a compressed format or vice versa.
		width = view->baseTexture->GetMipWidth(baseMip);
		height = view->baseTexture->GetMipHeight(baseMip);
		// adjust firstMip and firstSlice to be relative to base of subtexture
		cemu_assert(firstMip >= baseMip);
		cemu_assert(firstSlice >= baseSlice);
		firstMip -= baseMip;
		firstSlice -= baseSlice;
	}

	// key data for looking up views
	MPTR physAddr;
	MPTR physMipAddr;
	sint32 width;
	sint32 height;
	sint32 pitch;
	sint32 firstMip;
	sint32 numMip;
	sint32 firstSlice;
	sint32 numSlice;
	Latte::E_GX2SURFFMT format;
	Latte::E_DIM dim;
	bool isDepth;
	// associated view
	LatteTextureView* view;
};

struct LatteTexViewBucket
{
	std::vector<LatteTexViewLookupDesc> list;
};

#define TEXTURE_VIEW_BUCKETS	(1061)

inline uint32 _getViewBucketKey(MPTR physAddress, uint32 width, uint32 height, uint32 pitch)
{
	return (physAddress + width * 7 + height * 11 + pitch * 13) % TEXTURE_VIEW_BUCKETS;
}

inline uint32 _getViewBucketKeyNoRes(MPTR physAddress, uint32 pitch)
{
	return (physAddress + pitch * 13) % TEXTURE_VIEW_BUCKETS;
}

LatteTexViewBucket texViewBucket[TEXTURE_VIEW_BUCKETS] = { };
LatteTexViewBucket texViewBucket_nores[TEXTURE_VIEW_BUCKETS] = { };

void LatteTextureViewLookupCache::Add(LatteTextureView* view, uint32 baseMip, uint32 baseSlice)
{
	LatteTexViewLookupDesc desc(view);
	if (baseMip != 0 || baseSlice != 0)
		desc.SetParametersForSubTexture(baseMip, baseSlice);
	// generic bucket
	uint32 key = _getViewBucketKey(desc.physAddr, desc.width, desc.height, desc.pitch);
	texViewBucket[key].list.emplace_back(desc);
	vectorAppendUnique(view->viewLookUpCacheKeys, key);
	// resolution-independent bucket
	key = _getViewBucketKeyNoRes(desc.physAddr, desc.pitch);
	texViewBucket_nores[key].list.push_back(desc);
	vectorAppendUnique(view->viewLookUpCacheKeysNoRes, key);
}

void LatteTextureViewLookupCache::RemoveAll(LatteTextureView* view)
{
	for (auto& key : view->viewLookUpCacheKeys)
	{
		auto& bucket = texViewBucket[key].list;
		bucket.erase(std::remove_if(bucket.begin(), bucket.end(), [view](const LatteTexViewLookupDesc& v) {
			return v.view == view; }), bucket.end());
	}
	for (auto& key : view->viewLookUpCacheKeysNoRes)
	{
		auto& bucket = texViewBucket_nores[key].list;
		bucket.erase(std::remove_if(bucket.begin(), bucket.end(), [view](const LatteTexViewLookupDesc& v) {
			return v.view == view; }), bucket.end());
	}
}

LatteTextureView* LatteTextureViewLookupCache::lookup(MPTR physAddr, sint32 width, sint32 height, sint32 depth, sint32 pitch, sint32 firstMip, sint32 numMip, sint32 firstSlice, sint32 numSlice, Latte::E_GX2SURFFMT format, Latte::E_DIM dim)
{
	// todo - add tileMode param to this and the other lookup functions?
	uint32 key = _getViewBucketKey(physAddr, width, height, pitch);
	key %= TEXTURE_VIEW_BUCKETS;
	for (auto& it : texViewBucket[key].list)
	{
		if (it.format == format && it.dim == dim &&
			it.width == width && it.height == height && it.pitch == pitch && it.physAddr == physAddr
			&& it.firstMip == firstMip && it.numMip == numMip
			&& it.firstSlice == firstSlice && it.numSlice == numSlice
			)
		{
			return it.view;
		}
	}
	return nullptr;
}

LatteTextureView* LatteTextureViewLookupCache::lookupWithColorOrDepthType(MPTR physAddr, sint32 width, sint32 height, sint32 depth, sint32 pitch, sint32 firstMip, sint32 numMip, sint32 firstSlice, sint32 numSlice, Latte::E_GX2SURFFMT format, Latte::E_DIM dim, bool isDepth)
{
	cemu_assert_debug(firstSlice == 0);
	uint32 key = _getViewBucketKey(physAddr, width, height, pitch);
	key %= TEXTURE_VIEW_BUCKETS;
	for (auto& it : texViewBucket[key].list)
	{
		if (it.format == format && it.dim == dim && it.width == width && it.height == height && it.pitch == pitch && it.physAddr == physAddr
			&& it.firstMip == firstMip && it.numMip == numMip
			&& it.firstSlice == firstSlice && it.numSlice == numSlice &&
			it.isDepth == isDepth
			)
		{
			return it.view;
		}
	}
	return nullptr;
}

// look up view with unspecified mipCount and sliceCount
LatteTextureView* LatteTextureViewLookupCache::lookupSlice(MPTR physAddr, sint32 width, sint32 height, sint32 pitch, sint32 firstMip, sint32 firstSlice, Latte::E_GX2SURFFMT format)
{
	uint32 key = _getViewBucketKey(physAddr, width, height, pitch);
	key %= TEXTURE_VIEW_BUCKETS;
	for (auto& it : texViewBucket[key].list)
	{
		if (it.width == width && it.height == height && it.pitch == pitch && it.physAddr == physAddr && it.format == format)
		{
			if (firstSlice == it.firstSlice && firstMip == it.firstMip)
				return it.view;
		}
	}
	return nullptr;
}

// look up view with unspecified mipCount/sliceCount and only minimum width and height given
LatteTextureView* LatteTextureViewLookupCache::lookupSliceMinSize(MPTR physAddr, sint32 minWidth, sint32 minHeight, sint32 pitch, sint32 firstMip, sint32 firstSlice, Latte::E_GX2SURFFMT format)
{
	uint32 key = _getViewBucketKeyNoRes(physAddr, pitch);
	key %= TEXTURE_VIEW_BUCKETS;
	for (auto& it : texViewBucket_nores[key].list)
	{
		if (it.width >= minWidth && it.height >= minHeight && it.pitch == pitch && it.physAddr == physAddr && it.format == format)
		{
			if (firstSlice == it.firstSlice && firstMip == it.firstMip)
				return it.view;
		}
	}
	return nullptr;
}

// similar to lookupSlice but also compares isDepth
LatteTextureView* LatteTextureViewLookupCache::lookupSliceEx(MPTR physAddr, sint32 width, sint32 height, sint32 pitch, sint32 firstMip, sint32 firstSlice, Latte::E_GX2SURFFMT format, bool isDepth)
{
	cemu_assert_debug(firstMip == 0);
	uint32 key = _getViewBucketKey(physAddr, width, height, pitch);
	key %= TEXTURE_VIEW_BUCKETS;
	for (auto& it : texViewBucket[key].list)
	{
		if (it.width == width && it.height == height && it.pitch == pitch && it.physAddr == physAddr && it.format == format && it.isDepth == isDepth)
		{
			if (firstSlice == it.firstSlice && firstMip == it.firstMip)
				return it.view;
		}
	}
	return nullptr;
}

std::unordered_set<LatteTextureView*> LatteTextureViewLookupCache::GetAllViews()
{
	std::unordered_set<LatteTextureView*> viewSet;
	for (uint32 i = 0; i < TEXTURE_VIEW_BUCKETS; i++)
	{
		for (auto& it : texViewBucket[i].list)
			viewSet.emplace(it.view);
	}
	return viewSet;
}