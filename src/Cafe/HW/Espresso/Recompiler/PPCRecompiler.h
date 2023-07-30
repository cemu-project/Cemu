#include <vector>

#define PPC_REC_CODE_AREA_START		(0x00000000) // lower bound of executable memory area. Recompiler expects this address to be 0
#define PPC_REC_CODE_AREA_END		(0x10000000) // upper bound of executable memory area
#define PPC_REC_CODE_AREA_SIZE		(PPC_REC_CODE_AREA_END - PPC_REC_CODE_AREA_START)

#define PPC_REC_ALIGN_TO_4MB(__v)	(((__v)+4*1024*1024-1)&~(4*1024*1024-1))

#define PPC_REC_MAX_VIRTUAL_GPR		(40) // enough to store 32 GPRs + a few SPRs + temp registers (usually only 1-2)

typedef struct  
{
	uint32 ppcAddress;
	uint32 ppcSize;
	//void* x86Start;
	//size_t x86Size;
	void* storedRange;
}ppcRecRange_t;

typedef struct  
{
	uint32 ppcAddress;
	uint32 ppcSize; // ppc code size of function
	void*  x86Code; // pointer to x86 code
	size_t x86Size;
	std::vector<ppcRecRange_t> list_ranges;
}PPCRecFunction_t;

#define PPCREC_IML_OP_FLAG_SIGNEXTEND			(1<<0)
#define PPCREC_IML_OP_FLAG_SWITCHENDIAN			(1<<1)
#define PPCREC_IML_OP_FLAG_NOT_EXPANDED			(1<<2) // set single-precision load instructions to indicate that the value should not be rounded to double-precision 
#define PPCREC_IML_OP_FLAG_UNUSED				(1<<7) // used to mark instructions that are not used

typedef struct  
{
	uint8 type;
	uint8 operation;
	uint8 crRegister; // set to 0xFF if not set, not all IML instruction types support cr.
	uint8 crMode; // only used when crRegister is valid, used to differentiate between various forms of condition flag set/clear behavior
	uint32 crIgnoreMask; // bit set for every respective CR bit that doesn't need to be updated
	uint32 associatedPPCAddress; // ppc address that is associated with this instruction
	union
	{
		struct  
		{
			uint8 _padding[7];
		}padding;
		struct  
		{
			// R (op) A [update cr* in mode *]
			uint8 registerResult;
			uint8 registerA;
		}op_r_r;
		struct  
		{
			// R = A (op) B [update cr* in mode *]
			uint8 registerResult;
			uint8 registerA;
			uint8 registerB;
		}op_r_r_r;
		struct  
		{
			// R = A (op) immS32 [update cr* in mode *]
			uint8 registerResult;
			uint8 registerA;
			sint32 immS32;
		}op_r_r_s32;
		struct  
		{
			// R/F = NAME or NAME = R/F
			uint8 registerIndex;
			uint8 copyWidth;
			uint32 name;
			uint8 flags;
		}op_r_name;
		struct  
		{
			// R (op) s32 [update cr* in mode *]
			uint8 registerIndex;
			sint32 immS32;
		}op_r_immS32;
		struct  
		{
			uint32 address;
			uint8 flags;
		}op_jumpmark;
		struct  
		{
			uint32 param;
			uint32 param2;
			uint16 paramU16;
		}op_macro;
		struct  
		{
			uint32 jumpmarkAddress;
			bool jumpAccordingToSegment; //PPCRecImlSegment_t* destinationSegment; // if set, this replaces jumpmarkAddress
			uint8 condition; // only used when crRegisterIndex is 8 or above (update: Apparently only used to mark jumps without a condition? -> Cleanup)
			uint8 crRegisterIndex;
			uint8 crBitIndex;
			bool  bitMustBeSet;
		}op_conditionalJump;
		struct
		{
			uint8 registerData;
			uint8 registerMem;
			uint8 registerMem2;
			uint8 registerGQR;
			uint8 copyWidth;
			//uint8 flags;
			struct  
			{
				bool swapEndian : 1;
				bool signExtend : 1;
				bool notExpanded : 1; // for floats
			}flags2;
			uint8 mode; // transfer mode (copy width, ps0/ps1 behavior)
			sint32 immS32;
		}op_storeLoad;
		struct
		{
			struct
			{
				uint8 registerMem;
				sint32 immS32;
			}src;
			struct
			{
				uint8 registerMem;
				sint32 immS32;
			}dst;
			uint8 copyWidth;
		}op_mem2mem;
		struct  
		{
			uint8 registerResult;
			uint8 registerOperand;
			uint8 flags;
		}op_fpr_r_r;
		struct  
		{
			uint8 registerResult;
			uint8 registerOperandA;
			uint8 registerOperandB;
			uint8 flags;
		}op_fpr_r_r_r;
		struct  
		{
			uint8 registerResult;
			uint8 registerOperandA;
			uint8 registerOperandB;
			uint8 registerOperandC;
			uint8 flags;
		}op_fpr_r_r_r_r;
		struct  
		{
			uint8 registerResult;
			//uint8 flags;
		}op_fpr_r;
		struct  
		{
			uint32 ppcAddress;
			uint32 x64Offset;
		}op_ppcEnter;
		struct  
		{
			uint8 crD; // crBitIndex (result)
			uint8 crA; // crBitIndex
			uint8 crB; // crBitIndex
		}op_cr;
		// conditional operations (emitted if supported by target platform)
		struct
		{
			// r_s32
			uint8 registerIndex;
			sint32 immS32;
			// condition
			uint8 crRegisterIndex;
			uint8 crBitIndex;
			bool  bitMustBeSet;
		}op_conditional_r_s32;
	};
}PPCRecImlInstruction_t;

typedef struct _PPCRecImlSegment_t PPCRecImlSegment_t;

typedef struct _ppcRecompilerSegmentPoint_t
{
	sint32 index;
	PPCRecImlSegment_t* imlSegment;
	_ppcRecompilerSegmentPoint_t* next;
	_ppcRecompilerSegmentPoint_t* prev;
}ppcRecompilerSegmentPoint_t;

struct raLivenessLocation_t
{
	sint32 index;
	bool isRead;
	bool isWrite;

	raLivenessLocation_t() = default;

	raLivenessLocation_t(sint32 index, bool isRead, bool isWrite)
		: index(index), isRead(isRead), isWrite(isWrite) {};
};

struct raLivenessSubrangeLink_t
{
	struct raLivenessSubrange_t* prev;
	struct raLivenessSubrange_t* next;
};

struct raLivenessSubrange_t
{
	struct raLivenessRange_t* range;
	PPCRecImlSegment_t* imlSegment;
	ppcRecompilerSegmentPoint_t start;
	ppcRecompilerSegmentPoint_t end;
	// dirty state tracking
	bool _noLoad;
	bool hasStore;
	bool hasStoreDelayed;
	// next
	raLivenessSubrange_t* subrangeBranchTaken;
	raLivenessSubrange_t* subrangeBranchNotTaken;
	// processing
	uint32 lastIterationIndex;
	// instruction locations
	std::vector<raLivenessLocation_t> list_locations;
	// linked list (subranges with same GPR virtual register)
	raLivenessSubrangeLink_t link_sameVirtualRegisterGPR;
	// linked list (all subranges for this segment)
	raLivenessSubrangeLink_t link_segmentSubrangesGPR;
};

struct raLivenessRange_t
{
	sint32 virtualRegister;
	sint32 physicalRegister;
	sint32 name;
	std::vector<raLivenessSubrange_t*> list_subranges;
};

struct PPCSegmentRegisterAllocatorInfo_t
{
	// analyzer stage
	bool isPartOfProcessedLoop{}; // used during loop detection
	sint32 lastIterationIndex{};
	// linked lists
	raLivenessSubrange_t* linkedList_allSubranges{};
	raLivenessSubrange_t* linkedList_perVirtualGPR[PPC_REC_MAX_VIRTUAL_GPR]{};
};

struct PPCRecVGPRDistances_t
{
	struct _RegArrayEntry
	{
		sint32 usageStart{};
		sint32 usageEnd{};
	}reg[PPC_REC_MAX_VIRTUAL_GPR];
	bool isProcessed[PPC_REC_MAX_VIRTUAL_GPR]{};
};

typedef struct _PPCRecImlSegment_t
{
	sint32 momentaryIndex{}; // index in segment list, generally not kept up to date except if needed (necessary for loop detection)
	sint32 startOffset{}; // offset to first instruction in iml instruction list
	sint32 count{}; // number of instructions in segment
	uint32 ppcAddress{}; // ppc address (0xFFFFFFFF if not associated with an address)
	uint32 x64Offset{}; // x64 code offset of segment start
	uint32 cycleCount{}; // number of PPC cycles required to execute this segment (roughly)
	// list of intermediate instructions in this segment
	PPCRecImlInstruction_t* imlList{};
	sint32 imlListSize{};
	sint32 imlListCount{};
	// segment link
	_PPCRecImlSegment_t* nextSegmentBranchNotTaken{}; // this is also the default for segments where there is no branch
	_PPCRecImlSegment_t* nextSegmentBranchTaken{};
	bool nextSegmentIsUncertain{};
	sint32 loopDepth{};
	//sList_t* list_prevSegments;
	std::vector<_PPCRecImlSegment_t*> list_prevSegments{};
	// PPC range of segment
	uint32 ppcAddrMin{};
	uint32 ppcAddrMax{};
	// enterable segments
	bool isEnterable{}; // this segment can be entered from outside the recompiler (no preloaded registers necessary)
	uint32 enterPPCAddress{}; // used if isEnterable is true
	// jump destination segments
	bool isJumpDestination{}; // segment is a destination for one or more (conditional) jumps
	uint32 jumpDestinationPPCAddress{};
	// PPC FPR use mask
	bool ppcFPRUsed[32]{}; // same as ppcGPRUsed, but for FPR
	// CR use mask
	uint32 crBitsInput{}; // bits that are expected to be set from the previous segment (read in this segment but not overwritten)
	uint32 crBitsRead{}; // all bits that are read in this segment
	uint32 crBitsWritten{}; // bits that are written in this segment
	// register allocator info
	PPCSegmentRegisterAllocatorInfo_t raInfo{};
	PPCRecVGPRDistances_t raDistances{};
	bool raRangeExtendProcessed{};
	// segment points
	ppcRecompilerSegmentPoint_t* segmentPointList{};
}PPCRecImlSegment_t;

struct ppcImlGenContext_t
{
	PPCRecFunction_t* functionRef;
	uint32* currentInstruction;
	uint32  ppcAddressOfCurrentInstruction;
	// fpr mode
	bool LSQE{ true };
	bool PSE{ true };
	// cycle counter
	uint32 cyclesSinceLastBranch; // used to track ppc cycles
	// temporary general purpose registers
	uint32 mappedRegister[PPC_REC_MAX_VIRTUAL_GPR];
	// temporary floating point registers (single and double precision)
	uint32 mappedFPRRegister[256];
	// list of intermediate instructions
	PPCRecImlInstruction_t* imlList;
	sint32 imlListSize;
	sint32 imlListCount;
	// list of segments
	PPCRecImlSegment_t** segmentList;
	sint32 segmentListSize;
	sint32 segmentListCount;
	// code generation control
	bool hasFPUInstruction; // if true, PPCEnter macro will create FP_UNAVAIL checks -> Not needed in user mode
	// register allocator info
	struct  
	{
		std::vector<raLivenessRange_t*> list_ranges;
	}raInfo;
	// analysis info
	struct  
	{
		bool modifiesGQR[8];
	}tracking;
};

typedef void ATTR_MS_ABI (*PPCREC_JUMP_ENTRY)();

typedef struct  
{
	PPCRecFunction_t* ppcRecompilerFuncTable[PPC_REC_ALIGN_TO_4MB(PPC_REC_CODE_AREA_SIZE/4)]; // one virtual-function pointer for each potential ppc instruction
	PPCREC_JUMP_ENTRY ppcRecompilerDirectJumpTable[PPC_REC_ALIGN_TO_4MB(PPC_REC_CODE_AREA_SIZE/4)]; // lookup table for ppc offset to native code function
	// x64 data
	alignas(16) uint64 _x64XMM_xorNegateMaskBottom[2];
	alignas(16) uint64 _x64XMM_xorNegateMaskPair[2];
	alignas(16) uint64 _x64XMM_xorNOTMask[2];
	alignas(16) uint64 _x64XMM_andAbsMaskBottom[2];
	alignas(16) uint64 _x64XMM_andAbsMaskPair[2];
	alignas(16) uint32 _x64XMM_andFloatAbsMaskBottom[4];
	alignas(16) uint64 _x64XMM_singleWordMask[2];
	alignas(16) double _x64XMM_constDouble1_1[2];
	alignas(16) double _x64XMM_constDouble0_0[2];
	alignas(16) float  _x64XMM_constFloat0_0[2];
	alignas(16) float  _x64XMM_constFloat1_1[2];
	alignas(16) float  _x64XMM_constFloatMin[2];
	alignas(16) uint32 _x64XMM_flushDenormalMask1[4];
	alignas(16) uint32 _x64XMM_flushDenormalMaskResetSignBits[4];
	// PSQ load/store scale tables
	double _psq_ld_scale_ps0_ps1[64 * 2];
	double _psq_ld_scale_ps0_1[64 * 2];
	double _psq_st_scale_ps0_ps1[64 * 2];
	double _psq_st_scale_ps0_1[64 * 2];
	// MXCSR
	uint32 _x64XMM_mxCsr_ftzOn;
	uint32 _x64XMM_mxCsr_ftzOff;
}PPCRecompilerInstanceData_t;

extern PPCRecompilerInstanceData_t* ppcRecompilerInstanceData;
extern bool ppcRecompilerEnabled;

void PPCRecompiler_init();
void PPCRecompiler_Shutdown();

void PPCRecompiler_allocateRange(uint32 startAddress, uint32 size);

void PPCRecompiler_invalidateRange(uint32 startAddr, uint32 endAddr);

extern void ATTR_MS_ABI (*PPCRecompiler_enterRecompilerCode)(uint64 codeMem, uint64 ppcInterpreterInstance);
extern void ATTR_MS_ABI (*PPCRecompiler_leaveRecompilerCode_visited)();
extern void ATTR_MS_ABI (*PPCRecompiler_leaveRecompilerCode_unvisited)();

#define PPC_REC_INVALID_FUNCTION	((PPCRecFunction_t*)-1)

// todo - move some of the stuff above into PPCRecompilerInternal.h

// recompiler interface

void PPCRecompiler_recompileIfUnvisited(uint32 enterAddress);
void PPCRecompiler_attemptEnter(struct PPCInterpreter_t* hCPU, uint32 enterAddress);
void PPCRecompiler_attemptEnterWithoutRecompile(struct PPCInterpreter_t* hCPU, uint32 enterAddress);
