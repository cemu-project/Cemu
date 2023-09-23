#pragma once

struct LatteDecompilerALUInstruction
{
	struct LatteDecompilerCFInstruction* cfInstruction{};
	bool isOP3{};
	uint32 opcode{};
	uint32 instructionGroupIndex{};
	uint8 indexMode{};
	uint8 omod{};
	// destination
	uint32 destGpr{};
	uint8 destRel{};
	uint8 destElem{};
	uint8 destClamp{};
	uint8 writeMask{};
	// flags
	uint8 updateExecuteMask{};
	uint8 updatePredicate{};
	// source operands
	struct
	{
		uint32 sel{};
		uint8 rel{};
		uint8 abs{};
		uint8 neg{};
		uint8 chan{};
	}sourceOperand[3];
	union
	{
		uint32 w[4];
		float f[4];
	}literalData;
	// information from analyzer stage
	uint8 aluUnit{}; // 0-3 -> ALU.x/y/u/w (PV), 4 -> Trans unit (PS)
	uint8 indexInGroup{}; // index of instruction within instruction group
	bool isLastInstructionOfGroup{};
};

struct LatteDecompilerTEXInstruction
{
	struct LatteDecompilerCFInstruction* cfInstruction{};
	uint32 opcode{};
	// texture or vertex fetch
	// shared
	sint32 srcGpr;
	sint32 dstGpr;
	sint8 dstSel[4];
	// texture fetch
	struct  
	{
		sint32 textureIndex{};
		sint32 samplerIndex{};
		uint32 offset{};
		sint8 srcSel[4]{};
		sint8 offsetX{};
		sint8 offsetY{};
		sint8 offsetZ{};
		bool unnormalized[4]{}; // set if texture coordinates are in [0,dim] range instead of [0,1]
	}textureFetch;
	// memRead
	struct
	{
		uint32 arrayBase{};
		sint8 srcSelX{};
		uint32 format{};
		uint8 nfa{};
		uint8 isSigned{};
	}memRead;
};

struct LatteDecompilerCFInstruction
{
	uint32 type{};
	uint32 cfAddr{};
	// for clauses with instructions
	uint32 addr{};
	sint32 count{};
	// clause contains either ALU or TEX instructions
	std::vector<LatteDecompilerALUInstruction> instructionsALU;
	std::vector<LatteDecompilerTEXInstruction> instructionsTEX;
	// for clauses that access uniform buffers
	uint32 cBank0Index{};
	uint32 cBank1Index{};
	uint32 cBank0AddrBase{};
	uint32 cBank1AddrBase{};
	// for exports
	uint32 exportType{};
	uint8  exportComponentSel[4]{};
	uint32 exportBurstCount{};
	// for mem write
	uint32 memWriteArraySize{};
	uint8 memWriteCompMask{};
	uint8 memWriteElemSize{}; // 0-3
	// for exports and mem write
	uint32 exportArrayBase{};
	uint32 exportSourceGPR{};
	// misc
	uint32 cfCond{};
	uint32 popCount{};
	// information from analyzer stage
	bool modifiesPredicate{};
	bool modifiesActiveMask{};
	uint32 numPredInstructions{};
	sint32 activeStackDepth{}; // stack depth during the clause/CF instruction

	LatteDecompilerCFInstruction()
	{

	}

	~LatteDecompilerCFInstruction()
	{
		cemu_assert_debug(!(instructionsALU.size() != 0 && instructionsTEX.size() != 0)); // make sure we haven't accidentally added the wrong instruction type
	}

#if BOOST_OS_WINDOWS
	LatteDecompilerCFInstruction(LatteDecompilerCFInstruction& mE) = default;
	LatteDecompilerCFInstruction(LatteDecompilerCFInstruction&& mE) = default;
#else
	LatteDecompilerCFInstruction(const LatteDecompilerCFInstruction& mE) = default;
	LatteDecompilerCFInstruction(LatteDecompilerCFInstruction&& mE) = default;
#endif

	LatteDecompilerCFInstruction& operator=(LatteDecompilerCFInstruction&& mE) = default;
};

struct LatteDecompilerSubroutineInfo
{
	uint32 cfAddr;
	std::vector<LatteDecompilerCFInstruction> instructions;
};

// helper struct to track the highest accessed offset within a buffer
struct LatteDecompilerBufferAccessTracker
{
	bool hasStaticIndexAccess{false};
	bool hasDynamicIndexAccess{false};
	sint32 highestAccessDynamicIndex{0};
	sint32 highestAccessStaticIndex{0};

	// track access, index is the array index and not a byte offset
	void TrackAccess(sint32 index, bool isDynamicIndex)
	{
		if (isDynamicIndex)
		{
			hasDynamicIndexAccess = true;
			if (index > highestAccessDynamicIndex)
				highestAccessDynamicIndex = index;
		}
		else
		{
			hasStaticIndexAccess = true;
			if (index > highestAccessStaticIndex)
				highestAccessStaticIndex = index;
		}
	}

	sint32 DetermineSize(sint32 maximumSize) const
	{
		// here we try to predict the accessed range so we dont have to upload the whole buffer
		// potential risky optimization: assume that if there is a fixed-index access on an index higher than any other non-zero relative accesses, it bounds the prior relative access
		sint32 highestAccessIndex = -1;
		if(hasStaticIndexAccess)
		{
			highestAccessIndex = highestAccessStaticIndex;
		}
		if(hasDynamicIndexAccess)
		{
			return maximumSize; // dynamic index exists and no bound can be determined
		}
		if (highestAccessIndex < 0)
			return 1; // no access at all? But avoid zero as a size
		return highestAccessIndex + 1;
	}

	bool HasAccess() const
	{
		return hasStaticIndexAccess || hasDynamicIndexAccess;
	}

	bool HasRelativeAccess() const
	{
		return hasDynamicIndexAccess;
	}
};

struct LatteDecompilerShaderContext
{
	LatteDecompilerOutput_t* output;
	LatteDecompilerShader* shader;
	LatteConst::ShaderType shaderType;
	const class LatteDecompilerOptions* options;
	uint32* contextRegisters; // deprecated
	struct LatteContextRegister* contextRegistersNew;
	uint64 shaderBaseHash;
	StringBuf* shaderSource;
	std::vector<LatteDecompilerCFInstruction> cfInstructions;
	// fetch shader (required for vertex shader)
	LatteFetchShader* fetchShader{};
	// geometry copy shader (only present when geometry shader is active)
	LatteParsedGSCopyShader* parsedGSCopyShader;
	// state
	bool hasError;
	// type tracker
	struct
	{
		// data type tracker
		uint8 defaultDataType;
		bool genFloatReg; // if set, generate R*f register variables
		bool genIntReg; // if set, generate R*i register variables
		bool useArrayGPRs; // if set, an array is used to represent GPRs instead of individual variables
	}typeTracker;
	// analyzer	
	struct
	{
		// general
		bool hasStreamoutEnable{}; // set if streamout is enabled
		bool hasLoops{}; // loop directives present in shader
		// vertex shader
		bool isPointsPrimitive{}; // set if current render primitive is points
		bool outputPointSize{}; // set if the current shader should output the point size
		std::bitset<256> inputAttributSemanticMask; // one set bit for every used semanticId - todo: there are only 128 bit available semantic locations? The MSB has special meaning?
		// uniforms
		LatteDecompilerBufferAccessTracker uniformRegisterAccessTracker;
		LatteDecompilerBufferAccessTracker uniformBufferAccessTracker[LATTE_NUM_MAX_UNIFORM_BUFFERS];
		// ssbo
		bool hasSSBORead; // shader has instructions that read from SSBO
		bool hasSSBOWrite; // shader has instructions that write to SSBO
		// textures
		std::bitset<LATTE_NUM_MAX_TEX_UNITS> texUnitUsesTexelCoordinates;
		bool hasCubeMapTexture; // set to true if a cubemap texture is used
		bool hasGradientLookup; // set to true if texture lookup with custom gradients is used
		// misc
		bool usesRelativeGPRRead; // set if indexed GPR reads are used
		bool usesRelativeGPRWrite; // set if indexed GPR writes are used
		uint8 gprUseMask[(LATTE_NUM_GPR + 7) / 8]; // 1 bit per GPR, set if GPR is read/written anywhere in the program (ignores GPR accesses with relative index)
		bool hasStreamoutWrite; // stream-out CF instructions are used
		bool hasRedcCUBE; // has cube reduction instruction
		bool modifiesPixelActiveState; // set if the active mask is changed anywhere in the shader (If false, we can skip active mask checks)
		bool usesIntegerValues; // set if the shader uses any kind of integer instruction or integer-based GPR/AR access
		sint32 activeStackMaxDepth; // maximum depth of pixel state stack
		// analyzer stage (vs)
		bool writesPointSize{};
		// streamout (vs and gs)
		bool useSSBOForStreamout{};
		// geometry shader
		uint32 numEmitVertex{}; // counts how often emit vertex instruction is found
	}analyzer;

	// set while generating code for subroutine
	bool isSubroutine;
	LatteDecompilerSubroutineInfo* subroutineInfo;

	// emitter
	bool hasUniformVarBlock;
	sint32 currentBindingPointVK{};
	struct ALUClauseTemporariesState* aluPVPSState{nullptr};
	// misc
	std::vector<LatteDecompilerSubroutineInfo> list_subroutines;
};

void LatteDecompiler_analyze(LatteDecompilerShaderContext* shaderContext, LatteDecompilerShader* shader);
void LatteDecompiler_analyzeDataTypes(LatteDecompilerShaderContext* shaderContext);
void LatteDecompiler_emitGLSLShader(LatteDecompilerShaderContext* shaderContext, LatteDecompilerShader* shader);

void LatteDecompiler_cleanup(LatteDecompilerShaderContext* shaderContext);

// helper functions

sint32 LatteDecompiler_getColorOutputIndexFromExportIndex(LatteDecompilerShaderContext* shaderContext, sint32 exportIndex);