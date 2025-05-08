bool PPCRecompiler_generateIntermediateCode(ppcImlGenContext_t& ppcImlGenContext, PPCRecFunction_t* PPCRecFunction, std::set<uint32>& entryAddresses, class PPCFunctionBoundaryTracker& boundaryTracker);

IMLSegment* PPCIMLGen_CreateSplitSegmentAtEnd(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo);
IMLSegment* PPCIMLGen_CreateNewSegmentAsBranchTarget(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo);

void PPCIMLGen_AssertIfNotLastSegmentInstruction(ppcImlGenContext_t& ppcImlGenContext);

IMLInstruction* PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_pushBackIMLInstructions(IMLSegment* imlSegment, sint32 index, sint32 shiftBackCount);
IMLInstruction* PPCRecompiler_insertInstruction(IMLSegment* imlSegment, sint32 index);

void PPCRecompilerIml_insertSegments(ppcImlGenContext_t* ppcImlGenContext, sint32 index, sint32 count);

void PPCRecompilerIml_setSegmentPoint(IMLSegmentPoint* segmentPoint, IMLSegment* imlSegment, sint32 index);
void PPCRecompilerIml_removeSegmentPoint(IMLSegmentPoint* segmentPoint);

// Register management
IMLReg PPCRecompilerImlGen_LookupReg(ppcImlGenContext_t* ppcImlGenContext, IMLName mappedName, IMLRegFormat regFormat);

IMLReg PPCRecompilerImlGen_loadRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName);

// IML instruction generation
void PPCRecompilerImlGen_generateNewInstruction_conditional_r_s32(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction, uint32 operation, IMLReg registerIndex, sint32 immS32, uint32 crRegisterIndex, uint32 crBitIndex, bool bitMustBeSet);

// IML generation - FPU
bool PPCRecompilerImlGen_LFS_LFSU_LFD_LFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate, bool isDouble);
bool PPCRecompilerImlGen_LFSX_LFSUX_LFDX_LFDUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate, bool isDouble);
bool PPCRecompilerImlGen_STFS_STFSU_STFD_STFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate, bool isDouble);
bool PPCRecompilerImlGen_STFSX_STFSUX_STFDX_STFDUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool hasUpdate, bool isDouble);
bool PPCRecompilerImlGen_STFIWX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMUL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FDIV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FNMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMULS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FDIVS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FADDS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMADDS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FNMSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FCMPO(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FCMPU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FNABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FRES(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FRSP(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FNEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FSEL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FRSQRTE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FCTIWZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PSQ_L(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate);
bool PPCRecompilerImlGen_PSQ_ST(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate);
bool PPCRecompilerImlGen_PS_MULSX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isVariant1);
bool PPCRecompilerImlGen_PS_MADDSX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isVariant1);
bool PPCRecompilerImlGen_PS_ADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_SUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MUL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_DIV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_NMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withNegative);
bool PPCRecompilerImlGen_PS_SUM0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_SUM1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_NEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_ABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_RES(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_RSQRTE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_SEL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MERGE00(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MERGE01(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MERGE10(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MERGE11(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_CMPO0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_CMPU0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_CMPU1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);

// IML general

void PPCRecompilerIML_isolateEnterableSegments(ppcImlGenContext_t* ppcImlGenContext);

void PPCIMLGen_CreateSegmentBranchedPath(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo, const std::function<void(ppcImlGenContext_t&)>& genSegmentBranchTaken, const std::function<void(ppcImlGenContext_t&)>& genSegmentBranchNotTaken);
void PPCIMLGen_CreateSegmentBranchedPath(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo, const std::function<void(ppcImlGenContext_t&)>& genSegmentBranchNotTaken); // no else segment
void PPCIMLGen_CreateSegmentBranchedPathMultiple(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo, IMLSegment** segmentsOut, IMLReg compareReg, sint32* compareValues, sint32 count, sint32 defaultCaseIndex);

class IMLRedirectInstOutput
{
public:
	IMLRedirectInstOutput(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* outputSegment);
	~IMLRedirectInstOutput();


private:
	ppcImlGenContext_t* m_context;
	IMLSegment* m_prevSegment;
};