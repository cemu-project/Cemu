#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "config/ActiveSettings.h"
#include "Cafe/CafeSystem.h"

namespace nn
{
	typedef uint32 Result;
	namespace sl
	{
		struct VTableEntry
		{
			uint16be offsetA{0};
			uint16be offsetB{0};
			MEMPTR<void> ptr;
		};
		static_assert(sizeof(VTableEntry) == 8);

		constexpr uint32 SL_MEM_MAGIC = 0xCAFE4321;

#define DTOR_WRAPPER(__TYPE) RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) { dtor(MEMPTR<__TYPE>(hCPU->gpr[3]), hCPU->gpr[4]); osLib_returnFromFunction(hCPU, 0); })

		template<typename T>
		MEMPTR<T> sl_new()
		{
			uint32 objSize = sizeof(T);
			uint32be* basePtr = (uint32be*)coreinit::_weak_MEMAllocFromDefaultHeapEx(objSize + 8, 0x8);
			basePtr[0] = SL_MEM_MAGIC;
			basePtr[1] = objSize;
			return (T*)(basePtr + 2);
		}

		void sl_delete(MEMPTR<void> mem)
		{
			if (!mem)
				return;
			uint32be* basePtr = (uint32be*)mem.GetPtr() - 2;
			if (basePtr[0] != SL_MEM_MAGIC)
			{
				cemuLog_log(LogType::Force, "nn_sl: Detected memory corruption");
				cemu_assert_suspicious();
			}
			coreinit::_weak_MEMFreeToDefaultHeap(basePtr);
		}

#pragma pack(1)
		struct WhiteList
		{
			uint32be titleTypes[50];
			uint32be titleTypesCount;
			uint32be padding;
			uint64be titleIds[50];
			uint32be titleIdCount;
		};
		static_assert(sizeof(WhiteList) == 0x264);
#pragma pack()
		
		struct WhiteListAccessor
		{
			MEMPTR<void> vTablePtr{}; // 0x00

			struct VTable
			{
				VTableEntry rtti;
				VTableEntry dtor;
				VTableEntry get;
			};
			static inline SysAllocator<VTable> s_titleVTable;

			static WhiteListAccessor* ctor(WhiteListAccessor* _this)
			{
				if (!_this)
					_this = sl_new<WhiteListAccessor>();
				*_this = {};
				_this->vTablePtr = s_titleVTable;
				return _this;
			}

			static void dtor(WhiteListAccessor* _this, uint32 options)
			{
				if (_this && (options & 1))
					sl_delete(_this);
			}

			static void Get(WhiteListAccessor* _this, nn::sl::WhiteList* outWhiteList)
			{
				*outWhiteList = {};
			}

			static void InitVTable()
			{
				s_titleVTable->rtti.ptr = nullptr; // todo
				s_titleVTable->dtor.ptr = DTOR_WRAPPER(WhiteListAccessor);
				s_titleVTable->get.ptr = RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) { Get(MEMPTR<WhiteListAccessor>(hCPU->gpr[3]), MEMPTR<WhiteList>(hCPU->gpr[4])); osLib_returnFromFunction(hCPU, 0); });
			}
		};
		static_assert(sizeof(WhiteListAccessor) == 0x04);

		SysAllocator<WhiteListAccessor> s_defaultWhiteListAccessor;

		WhiteListAccessor* GetDefaultWhiteListAccessor()
		{
			return s_defaultWhiteListAccessor;
		}
	} // namespace sl
} // namespace nn

void nnSL_load()
{
	nn::sl::WhiteListAccessor::InitVTable();
	nn::sl::WhiteListAccessor::ctor(nn::sl::s_defaultWhiteListAccessor);

	cafeExportRegisterFunc(nn::sl::GetDefaultWhiteListAccessor, "nn_sl", "GetDefaultWhiteListAccessor__Q2_2nn2slFv", LogType::NN_SL);
}
