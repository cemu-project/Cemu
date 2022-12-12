
#define PPCREC_CR_REG_TEMP			8 // there are only 8 cr registers (0-7) we use the 8th as temporary cr register that is never stored (BDNZ instruction for example)

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

// GPR register management
uint32 PPCRecompilerImlGen_loadRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName, bool loadNew = false);
uint32 PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName);

// FPR register management
uint32 PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName, bool loadNew = false);
uint32 PPCRecompilerImlGen_loadOverwriteFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName);

// IML instruction generation
void PPCRecompilerImlGen_generateNewInstruction_jump(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction, uint32 jumpmarkAddress);
void PPCRecompilerImlGen_generateNewInstruction_jumpSegment(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction);

void PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 registerIndex, sint32 immS32, uint32 copyWidth, bool signExtend, bool bigEndian, uint8 crRegister, uint32 crMode);
void PPCRecompilerImlGen_generateNewInstruction_conditional_r_s32(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction, uint32 operation, uint8 registerIndex, sint32 immS32, uint32 crRegisterIndex, uint32 crBitIndex, bool bitMustBeSet);
void PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction, uint32 operation, uint8 registerResult, uint8 registerA, uint8 crRegister = PPC_REC_INVALID_REGISTER, uint8 crMode = 0);



// IML instruction generation (new style, can generate new instructions but also overwrite existing ones)

void PPCRecompilerImlGen_generateNewInstruction_noOp(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction);

void PPCRecompilerImlGen_generateNewInstruction_fpr_r(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction, sint32 operation, uint8 registerResult, sint32 crRegister = PPC_REC_INVALID_REGISTER);

// IML generation - FPU
bool PPCRecompilerImlGen_LFS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFSU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFSX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFSUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFDX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFDUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFSU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFSX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFSUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFIWX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFDX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
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
bool PPCRecompilerImlGen_PSQ_L(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PSQ_LU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PSQ_ST(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PSQ_STU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MULS0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MULS1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MADDS0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MADDS1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_ADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_SUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MUL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_DIV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_NMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_NMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
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

void PPCRecompilerIML_linkSegments(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompilerIML_isolateEnterableSegments(ppcImlGenContext_t* ppcImlGenContext);

