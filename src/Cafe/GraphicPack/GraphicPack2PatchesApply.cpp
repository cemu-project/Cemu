#include "Cafe/GraphicPack/GraphicPack2.h"
#include "Common/FileStream.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cafe/HW/Espresso/Debugger/DebugSymbolStorage.h"

bool _relocateAddress(PatchGroup* group, PatchContext_t* ctx, uint32 addr, uint32& relocatedAddress)
{
	if (addr >= 0 && addr <= 1024 * 1024 * 8)
	{
		// codecave address
		relocatedAddress = group->getCodeCaveBase() + addr;
		return true;
	}
	// check if address is within module section
	for (sint32 i = 0; i < ctx->matchedModule->rplHeader.sectionTableEntryCount; i++)
	{
		auto sect = ctx->matchedModule->sectionTablePtr + i;
		if (addr >= sect->virtualAddress && addr < (sect->virtualAddress + sect->sectionSize))
		{
			relocatedAddress = addr - sect->virtualAddress + memory_getVirtualOffsetFromPointer(ctx->matchedModule->sectionAddressTable2[i].ptr);
			return true;
		}
	}
	relocatedAddress = 0;
	return false;
}

struct  
{
	bool hasUnknownVariable;
	PatchContext_t* activePatchContext;
	PatchGroup* currentGroup;
	// additional error information tracking
	sint32 lineNumber; // line number of the expression being processed, negative if not available
	bool captureUnresolvedSymbols;
}resolverState{};

bool GraphicPack2::ResolvePresetConstant(const std::string& varname, double& value) const
{
	const auto var = GetPresetVariable(GetActivePresets(), varname);
	if (var)
	{
		value = var->second;
		return true;
	}
	return false;
}

template<typename T>
T _expressionFuncHA(T input)
{
	uint32 u32 = (uint32)input;
	u32 = (((u32 >> 16) + ((u32 & 0x8000) ? 1 : 0)) & 0xffff);
	return (T)u32;
}

template<typename T>
T _expressionFuncHI(T input)
{
	uint32 u32 = (uint32)input;
	u32 = (u32 >> 16) & 0xffff;
	return (T)u32;
}

template<typename T>
T _expressionFuncLO(T input)
{
	uint32 u32 = (uint32)input;
	u32 &= 0xffff;
	return (T)u32;
}

template<typename T>
T _expressionFuncReloc(T input)
{
	uint32 addr = (uint32)input;
	uint32 relocatedAddress = 0;
	if(!_relocateAddress(resolverState.currentGroup, resolverState.activePatchContext, addr, relocatedAddress))
	{
		resolverState.activePatchContext->errorHandler.printError(resolverState.currentGroup, resolverState.lineNumber, fmt::format("reloc({0:#08x}): Address does not point to a known memory region", addr));
		return (T)0;
	}
	return (T)relocatedAddress;
}

double _cbResolveConstant(std::string_view varname)
{
	std::string varnameOnly;
	std::string tokenOnly;
	// detect suffix
	bool hasSuffix = false;
	const auto idx = varname.find('@');
	if (idx != std::string_view::npos)
	{
		hasSuffix = true;
		varnameOnly = varname.substr(0, idx);
		tokenOnly = varname.substr(idx + 1);
	}
	else
		varnameOnly = varname;

	double value;
	if (varnameOnly.length() >= 1 && varnameOnly[0] == '$')
	{
		// resolve preset variable
		if (!resolverState.activePatchContext->graphicPack->ResolvePresetConstant(varnameOnly, value))
		{
			resolverState.hasUnknownVariable = true;
			if (resolverState.captureUnresolvedSymbols)
				resolverState.activePatchContext->unresolvedSymbols.emplace(resolverState.lineNumber, resolverState.currentGroup, varnameOnly);
			return 0.0;
		}
	}
	else if (varnameOnly.length() >= 7 && boost::iequals(varnameOnly.substr(0, 7), "import."))
	{
		// resolve import
		std::string importName = varnameOnly.substr(7);
		// detect imports
		const auto idxDot = importName.find('.');
		bool isValidImport = false;
		std::string_view importError = "";
		if (idxDot != std::string_view::npos)
		{
			std::string moduleName = importName.substr(0, idxDot);
			std::string functionName = importName.substr(idxDot + 1);
			uint32 rplHandle = RPLLoader_GetHandleByModuleName(moduleName.c_str());
			if (rplHandle == RPL_INVALID_HANDLE)
			{
				importError = " (module not found)";
			}
			else
			{
				MPTR exportResult = RPLLoader_FindModuleOrHLEExport(rplHandle, false, functionName.c_str());
				if (exportResult)
				{
					isValidImport = true;
					value = (double)exportResult;
				}
				else
					importError = " (function not found)";
			}
		}
		else
			importError = " (invalid import syntax)";
		// error output
		if (!isValidImport)
		{
			resolverState.hasUnknownVariable = true;
			if (resolverState.captureUnresolvedSymbols)
			{
				std::string detailedSymbolName;
				detailedSymbolName.assign(importName);
				detailedSymbolName.append(importError);
				resolverState.activePatchContext->unresolvedSymbols.emplace(resolverState.lineNumber, resolverState.currentGroup, detailedSymbolName);
			}
			return 0.0;
		}
	}
	else
	{
		// resolve variable
		const auto v = resolverState.activePatchContext->map_values.find(varnameOnly);
		if (v == resolverState.activePatchContext->map_values.end())
		{
			resolverState.hasUnknownVariable = true;
			if (resolverState.captureUnresolvedSymbols)
				resolverState.activePatchContext->unresolvedSymbols.emplace(resolverState.lineNumber, resolverState.currentGroup, varnameOnly);
			return 0.0;
		}
		value = v->second;
	}
	if (hasSuffix)
	{
		std::transform(tokenOnly.cbegin(), tokenOnly.cend(), tokenOnly.begin(), tolower);
		if (tokenOnly == "ha")
		{
			value = _expressionFuncHA<double>(value);
		}
		else if (tokenOnly == "h" || tokenOnly == "hi")
		{
			value = _expressionFuncHI<double>(value);
		}
		else if (tokenOnly == "l" || tokenOnly == "lo")
		{
			value = _expressionFuncLO<double>(value);
		}
		else
		{
			// we treat unknown suffixes as unresolveable symbols
			resolverState.hasUnknownVariable = true;
			if (resolverState.captureUnresolvedSymbols)
			{
				std::string detailedSymbolName;
				detailedSymbolName.assign(varnameOnly);
				detailedSymbolName.append("@");
				detailedSymbolName.append(tokenOnly);
				detailedSymbolName.append(" (invalid suffix)");
				resolverState.activePatchContext->unresolvedSymbols.emplace(resolverState.lineNumber, resolverState.currentGroup, detailedSymbolName);
			}
			return 0.0;
		}
	}
	return value;
}

double _cbResolveFunction(std::string_view funcname, double input)
{
	std::string funcnameLC(funcname);
	std::transform(funcnameLC.cbegin(), funcnameLC.cend(), funcnameLC.begin(), tolower);
	double value = input;
	if (funcnameLC == "ha" || funcnameLC == "ha16")
		value = _expressionFuncHA<double>(value);
	else if (funcnameLC == "hi" || funcnameLC == "hi16")
		value = _expressionFuncHI<double>(value);
	else if (funcnameLC == "lo" || funcnameLC == "lo16")
		value = _expressionFuncLO<double>(value);
	else if (funcnameLC == "reloc")
		value = _expressionFuncReloc<double>(value);
	else
	{
		// unresolvable function
		resolverState.hasUnknownVariable = true;
		if (resolverState.captureUnresolvedSymbols)
		{
			std::string detailedSymbolName;
			detailedSymbolName.assign(funcname);
			detailedSymbolName.append("() (unknown function)");
			resolverState.activePatchContext->unresolvedSymbols.emplace(resolverState.lineNumber, resolverState.currentGroup, detailedSymbolName);
		}
		return 0.0;
	}
	return value;
}

template<typename T>
EXPRESSION_RESOLVE_RESULT _resolveExpression(PatchContext_t& ctx, std::string& expressionString, T& result, sint32 associatedLineNumber = -1)
{
	resolverState.lineNumber = associatedLineNumber;
	ExpressionParser ep;
	try
	{
		// add all the graphic pack constants
		ep.AddConstantCallback(_cbResolveConstant);
		ep.SetFunctionCallback(_cbResolveFunction);
		resolverState.hasUnknownVariable = false;
		result = (T)ep.Evaluate(expressionString);
		if (resolverState.hasUnknownVariable)
			return EXPRESSION_RESOLVE_RESULT::UNKNOWN_VARIABLE;
	}
	catch (const std::exception&)
	{
		cemu_assert_debug(false);
		ctx.errorHandler.printError(nullptr, -1, fmt::format("Unexpected error in expression \"{}\"", expressionString));
		return EXPRESSION_RESOLVE_RESULT::EXPRESSION_ERROR;
	}
	return EXPRESSION_RESOLVE_RESULT::AVAILABLE;
}

PATCH_RESOLVE_RESULT translateExpressionResult(EXPRESSION_RESOLVE_RESULT expressionResult)
{
	if (expressionResult == EXPRESSION_RESOLVE_RESULT::AVAILABLE)
		return PATCH_RESOLVE_RESULT::RESOLVED;
	else if (expressionResult == EXPRESSION_RESOLVE_RESULT::EXPRESSION_ERROR)
		return PATCH_RESOLVE_RESULT::EXPRESSION_ERROR;
	else if (expressionResult == EXPRESSION_RESOLVE_RESULT::UNKNOWN_VARIABLE)
		return PATCH_RESOLVE_RESULT::UNKNOWN_VARIABLE;
	cemu_assert(false);
	return PATCH_RESOLVE_RESULT::EXPRESSION_ERROR;
}

PATCH_RESOLVE_RESULT PatchEntryInstruction::resolveReloc(PatchContext_t& ctx, PPCAssemblerReloc* reloc)
{
	MPTR finalRelocAddr = m_relocatedAddr + reloc->m_byteOffset;
	if (reloc->m_relocType == PPCASM_RELOC::FLOAT)
	{
		// resolve float expression
		float result;
		auto r = _resolveExpression<float>(ctx, reloc->m_expression, result, m_lineNumber);
		if (r == EXPRESSION_RESOLVE_RESULT::AVAILABLE)
		{
			cemu_assert((reloc->m_byteOffset + sizeof(betype<float>)) <= m_length);
			*(betype<float>*)(m_dataWithRelocs + reloc->m_byteOffset) = result;
			DebugSymbolStorage::StoreDataType(finalRelocAddr, DEBUG_SYMBOL_TYPE::FLOAT);
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}
		else
			return translateExpressionResult(r);
	}
	else if (reloc->m_relocType == PPCASM_RELOC::DOUBLE)
	{
		// resolve double expression
		double result;
		auto r = _resolveExpression<double>(ctx, reloc->m_expression, result, m_lineNumber);
		if (r == EXPRESSION_RESOLVE_RESULT::AVAILABLE)
		{
			cemu_assert((reloc->m_byteOffset + sizeof(betype<double>)) <= m_length);
			*(betype<double>*)(m_dataWithRelocs + reloc->m_byteOffset) = result;
			DebugSymbolStorage::StoreDataType(finalRelocAddr, DEBUG_SYMBOL_TYPE::DOUBLE);
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}
		else
			return translateExpressionResult(r);
	}
	else
	{
		// resolve uint32 expression
		uint32 result;
		auto r = _resolveExpression<uint32>(ctx, reloc->m_expression, result, m_lineNumber);
		if (r != EXPRESSION_RESOLVE_RESULT::AVAILABLE)
			return translateExpressionResult(r);
		if (reloc->m_relocType == PPCASM_RELOC::U32)
		{
			cemu_assert((reloc->m_byteOffset + sizeof(betype<uint32>)) <= m_length);
			*(betype<uint32>*)(m_dataWithRelocs + reloc->m_byteOffset) = result;
			DebugSymbolStorage::StoreDataType(finalRelocAddr, DEBUG_SYMBOL_TYPE::U32);
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}
		else if (reloc->m_relocType == PPCASM_RELOC::U16)
		{
			cemu_assert((reloc->m_byteOffset + sizeof(betype<uint16>)) <= m_length);
			*(betype<uint16>*)(m_dataWithRelocs + reloc->m_byteOffset) = (uint16)result;
			DebugSymbolStorage::StoreDataType(finalRelocAddr, DEBUG_SYMBOL_TYPE::U16);
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}
		else if (reloc->m_relocType == PPCASM_RELOC::U8)
		{
			cemu_assert((reloc->m_byteOffset + sizeof(betype<uint8>)) <= m_length);
			*(betype<uint8>*)(m_dataWithRelocs + reloc->m_byteOffset) = (uint8)result;
			DebugSymbolStorage::StoreDataType(finalRelocAddr, DEBUG_SYMBOL_TYPE::U8);
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}
		else if (reloc->m_relocType == PPCASM_RELOC::U32_MASKED_IMM)
		{
			cemu_assert((reloc->m_byteOffset + sizeof(betype<uint32>)) <= m_length);
			uint32 opcode = *(betype<uint32>*)(m_dataWithRelocs + reloc->m_byteOffset);
			cemu_assert_debug(reloc->m_bitCount != 0);
			uint32 mask = 0xFFFFFFFF >> (32 - reloc->m_bitCount);
			mask <<= reloc->m_bitOffset;
			opcode &= ~mask;
			opcode |= ((result << reloc->m_bitOffset) & mask);
			*(betype<uint32>*)(m_dataWithRelocs + reloc->m_byteOffset) = opcode;
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}
		else if (reloc->m_relocType == PPCASM_RELOC::BRANCH_S26)
		{
			cemu_assert((reloc->m_byteOffset + sizeof(betype<uint32>)) <= m_length);
			uint32 opcode = *(betype<uint32>*)(m_dataWithRelocs + reloc->m_byteOffset);
			if (opcode & 2)
			{
				// absolute
				if (result >= 0x3FFFFFC)
				{
					cemuLog_log(LogType::Force, "Target \'{}\' for branch at line {} out of range", reloc->m_expression, m_lineNumber);
					return PATCH_RESOLVE_RESULT::VALUE_ERROR;
				}
				opcode &= ~0x3FFFFFC;
				opcode |= (result & 0x3FFFFFC);
			}
			else
			{
				// relative
				uint32 instrAddr = this->getRelocatedAddr() + reloc->m_byteOffset;
				if (result < instrAddr)
				{
					// jump backwards
					uint32 jumpB = instrAddr - result;
					if (jumpB > 0x1FFFFFF)
					{
						ctx.errorHandler.printError(nullptr, m_lineNumber, fmt::format("Target \'{0}\' for branch out of range (use MTCTR + BCTR or similar for long distance branches)", reloc->m_expression.c_str()));
						return PATCH_RESOLVE_RESULT::VALUE_ERROR;
					}
					opcode &= ~0x3FFFFFC;
					opcode |= ((~jumpB + 1) & 0x3FFFFFC);
				}
				else
				{
					// jump forwards
					uint32 jumpF = result - instrAddr;
					if (jumpF >= 0x1FFFFFF)
					{
						ctx.errorHandler.printError(nullptr, m_lineNumber, fmt::format("Target \'{0}\' for branch out of range (use MTCTR + BCTR or similar for long distance branches)", reloc->m_expression.c_str()));
						return PATCH_RESOLVE_RESULT::VALUE_ERROR;
					}
					opcode &= ~0x3FFFFFC;
					opcode |= (jumpF & 0x3FFFFFC);
				}
			}
			*(betype<uint32>*)(m_dataWithRelocs + reloc->m_byteOffset) = opcode;
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}
		else if (reloc->m_relocType == PPCASM_RELOC::BRANCH_S16)
		{
			cemu_assert((reloc->m_byteOffset + sizeof(betype<uint32>)) <= m_length);
			uint32 opcode = *(betype<uint32>*)(m_dataWithRelocs + reloc->m_byteOffset);
			uint32 instrAddr = this->getRelocatedAddr() + reloc->m_byteOffset;
			if (result < instrAddr)
			{
				// jump backwards
				uint32 jumpB = instrAddr - result;
				if (jumpB > 0x8000)
				{
					ctx.errorHandler.printError(nullptr, m_lineNumber, fmt::format("Target \'{0}\' for branch out of range (use MTCTR + BCTR or similar for long distance branches)", reloc->m_expression.c_str()));
					return PATCH_RESOLVE_RESULT::VALUE_ERROR;
				}
				opcode &= ~0xFFFC;
				opcode |= ((~jumpB + 1) & 0xFFFC);
			}
			else
			{
				// jump forwards
				uint32 jumpF = result - instrAddr;
				if (jumpF >= 0x8000)
				{
					ctx.errorHandler.printError(nullptr, m_lineNumber, fmt::format("Target \'{0}\' for branch out of range (use MTCTR + BCTR or similar for long distance branches)", reloc->m_expression.c_str()));
					return PATCH_RESOLVE_RESULT::VALUE_ERROR;
				}
				opcode &= ~0xFFFC;
				opcode |= (jumpF & 0xFFFC);
			}
			*(betype<uint32>*)(m_dataWithRelocs + reloc->m_byteOffset) = opcode;
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}


		// *internalCtx.opcode |= (relativeAddr & 0xFFFC);
		cemu_assert_debug(false);
	}
	return PATCH_RESOLVE_RESULT::UNDEFINED_ERROR;
}

PATCH_RESOLVE_RESULT PatchEntryInstruction::resolve(PatchContext_t& ctx)
{
	// relocate patch address
	if (!m_addrRelocated)
	{
		if (_relocateAddress(resolverState.currentGroup, &ctx, m_addr, m_relocatedAddr) == false)
		{
			cemuLog_log(LogType::Force, "Patches: Address 0x{:08x} (line {}) is not within code cave or any module section", this->getAddr(), this->m_lineNumber);
			cemu_assert_debug(false);
			return PATCH_RESOLVE_RESULT::INVALID_ADDRESS;
		}
		m_addrRelocated = true;
	}
	// apply relocations to instruction
	for (auto& itr : this->m_relocs)
	{
		if(itr.isApplied())
			continue;
		// evaluate expression and apply reloc to internal buffer
		auto r = resolveReloc(ctx, &itr);
		if (r == PATCH_RESOLVE_RESULT::RESOLVED)
		{
			itr.setApplied();
			continue;
		}
		return r;
	}
	return PATCH_RESOLVE_RESULT::RESOLVED;
}

void PatchEntryInstruction::applyPatch()
{
	const uint32 addr = getRelocatedAddr();
	if (addr == 0)
	{
		cemu_assert_debug(false);
		return;
	}
	uint8* patchAddr = (uint8*)memory_base + addr;
	memcpy(m_dataBackup, patchAddr, m_length);
	memcpy(patchAddr, m_dataWithRelocs, m_length);
	PPCRecompiler_invalidateRange(addr, addr + m_length);
}

void PatchEntryInstruction::undoPatch()
{
	const uint32 addr = getRelocatedAddr();
	if (addr == 0)
	{
		cemu_assert_debug(false);
		return;
	}
	uint8* patchAddr = (uint8*)memory_base + addr;
	memcpy(patchAddr, m_dataBackup, m_length);
	PPCRecompiler_invalidateRange(addr, addr + m_length);
	rplSymbolStorage_removeRange(addr, m_length);
	DebugSymbolStorage::ClearRange(addr, m_length);
}

// returns true on success, false if variable with same name already exists
bool registerU32Variable(PatchContext_t& ctx, std::string& name, uint32 value, PatchGroup* associatedPatchGroup, uint32 associatedLineNumber, bool isAddress)
{
	cemuLog_log(LogType::Patches, "Resolved symbol {} with value 0x{:08x}", name.c_str(), value);
	if (ctx.map_values.find(name) != ctx.map_values.end())
	{
		return false;
	}
	ctx.map_values[name] = value;
	// keep track of address symbols for the debugger
	rplSymbolStorage_store(ctx.graphicPack->GetName().data(), name.data(), value);

	return true;
}

PATCH_RESOLVE_RESULT PatchEntryCemuhookSymbolValue::resolve(PatchContext_t& ctx)
{
	uint32 addr;
	auto r = _resolveExpression<uint32>(ctx, m_expressionString, addr, m_lineNumber);
	if (r == EXPRESSION_RESOLVE_RESULT::AVAILABLE)
	{
		if (_relocateAddress(resolverState.currentGroup, &ctx, addr, m_resolvedValue))
		{
			m_isResolved = true;
			// register variable
			if (!registerU32Variable(ctx, m_symbolName, m_resolvedValue, resolverState.currentGroup, getLineNumber(), true))
			{
				if (resolverState.captureUnresolvedSymbols)
					ctx.errorHandler.printError(resolverState.currentGroup, m_lineNumber, fmt::format("Symbol {} is already defined", m_symbolName));
				return PATCH_RESOLVE_RESULT::VARIABLE_CONFLICT;
			}
			return PATCH_RESOLVE_RESULT::RESOLVED;
		}
		return PATCH_RESOLVE_RESULT::INVALID_ADDRESS;
	}
	return translateExpressionResult(r);
}

PATCH_RESOLVE_RESULT PatchEntryLabel::resolve(PatchContext_t& ctx)
{
	if (_relocateAddress(resolverState.currentGroup, &ctx, m_address, m_relocatedAddress))
	{
		m_isResolved = true;
		// register variable
		if (!registerU32Variable(ctx, m_symbolName, m_relocatedAddress, resolverState.currentGroup, getLineNumber(), true))
		{
			if (resolverState.captureUnresolvedSymbols)
				ctx.errorHandler.printError(resolverState.currentGroup, m_lineNumber, fmt::format("Label {} is already defined", m_symbolName));
			return PATCH_RESOLVE_RESULT::VARIABLE_CONFLICT;
		}
		return PATCH_RESOLVE_RESULT::RESOLVED;
	}
	if(resolverState.captureUnresolvedSymbols)
		ctx.errorHandler.printError(resolverState.currentGroup, m_lineNumber, fmt::format("Address {:#08x} of label {} does not point to any module section or code cave", m_address, m_symbolName));
	return PATCH_RESOLVE_RESULT::INVALID_ADDRESS;
}

PATCH_RESOLVE_RESULT PatchEntryVariableValue::resolve(PatchContext_t& ctx)
{
	uint32 v;
	auto r = _resolveExpression<uint32>(ctx, m_expressionString, v, m_lineNumber);
	if (r == EXPRESSION_RESOLVE_RESULT::AVAILABLE)
	{
		// register variable
		if (!registerU32Variable(ctx, m_symbolName, v, resolverState.currentGroup, getLineNumber(), false))
		{
			if (resolverState.captureUnresolvedSymbols)
				ctx.errorHandler.printError(resolverState.currentGroup, m_lineNumber, fmt::format("Variable {} is already defined", m_symbolName));
			return PATCH_RESOLVE_RESULT::VARIABLE_CONFLICT;
		}
		return PATCH_RESOLVE_RESULT::RESOLVED;
	}
	return translateExpressionResult(r);
}

struct UnresolvedPatches_t
{
	PatchGroup* patchGroup;
	std::vector<PatchEntry*> list_unresolvedPatches;
};

// returns number of resolved entries
bool _resolverPass(PatchContext_t& patchContext, std::vector<UnresolvedPatches_t>& unresolvedPatches, bool captureUnresolvedSymbols = false)
{
	resolverState.captureUnresolvedSymbols = captureUnresolvedSymbols;
	sint32 numResolvedEntries = 0;
	for (auto& unresolvedGroup : unresolvedPatches)
	{
		resolverState.currentGroup = unresolvedGroup.patchGroup;
		auto& list_unresolvedPatches = unresolvedGroup.list_unresolvedPatches;
		for (auto it = list_unresolvedPatches.begin(); it != list_unresolvedPatches.end();)
		{
			auto r = (*it)->resolve(patchContext);
			if (r == PATCH_RESOLVE_RESULT::RESOLVED)
			{
				// remove from list
				it = list_unresolvedPatches.erase(it);
				numResolvedEntries++;
				continue;
			}
			else if (r == PATCH_RESOLVE_RESULT::UNKNOWN_VARIABLE)
			{
				// dependency on other not yet resolved entry, continue iterating
				it++;
				continue;
			}
			else if (r == PATCH_RESOLVE_RESULT::INVALID_ADDRESS ||
				r == PATCH_RESOLVE_RESULT::VARIABLE_CONFLICT)
			{
				// errors handled and printed inside resolve()
				it++;
				continue;
			}
			else
			{
				// unknown error
				patchContext.errorHandler.printError(resolverState.currentGroup, -1, "Internal error");
				it++;
			}
		}
	}
	return numResolvedEntries;
}

void GraphicPack2::ApplyPatchGroups(std::vector<PatchGroup*>& groups, const RPLModule* rpl)
{
	// init context information
	PatchContext_t patchContext{};
	patchContext.graphicPack = this;
	patchContext.matchedModule = rpl;
	resolverState.activePatchContext = &patchContext;
	// setup error handler
	patchContext.errorHandler.setCurrentGraphicPack(this);
	patchContext.errorHandler.setStage(PatchErrorHandler::STAGE::APPLY);
	// no group can be applied more than once
	for (auto patchGroup : groups)
	{
		if (patchGroup->isApplied())
		{
			patchContext.errorHandler.printError(patchGroup, -1, "Group already applied to a different module.");
			return;
		}
	}
	// allocate code cave for every group
	for (auto patchGroup : groups)
	{
		if (patchGroup->codeCaveSize > 0)
		{
			auto codeCaveMem = RPLLoader_AllocateCodeCaveMem(256, patchGroup->codeCaveSize);
			cemuLog_log(LogType::Force, "Applying patch group \'{}\' (Codecave: {:08x}-{:08x})", patchGroup->name, codeCaveMem.GetMPTR(), codeCaveMem.GetMPTR() + patchGroup->codeCaveSize);
			patchGroup->codeCaveMem = codeCaveMem;
		}
		else
		{
			cemuLog_log(LogType::Force, "Applying patch group \'{}\'", patchGroup->name);
			patchGroup->codeCaveMem = nullptr;
		}
	}
	// resolve the patch entries
	// this means:
	// - resolving the expressions for variables and registering them
	// - calculating relocated addresses
	// - applying relocations to temporary patch buffer

	// multiple passes may be necessary since forward and backward references are allowed as well as references across group boundaries

	// create a copy of all the patch references and keep the group association intact
	std::vector<UnresolvedPatches_t> unresolvedPatches;
	unresolvedPatches.resize(groups.size());
	for (size_t i = 0; i < groups.size(); i++)
	{
		unresolvedPatches[i].patchGroup = groups[i];
		unresolvedPatches[i].list_unresolvedPatches = groups[i]->list_patches;
	}

	auto isUnresolvedPatchesEmpty = [&unresolvedPatches]()
	{
		for (auto& itr : unresolvedPatches)
			if (!itr.list_unresolvedPatches.empty())
				return false;
		return true;
	};
	// resolve and relocate
	for (sint32 pass = 0; pass < 30; pass++)
	{
		bool isLastPass = (pass == 29);
		sint32 numResolvedEntries = _resolverPass(patchContext, unresolvedPatches, false);
		if (isUnresolvedPatchesEmpty())
			break;
		if (numResolvedEntries == 0 || isLastPass)
		{
			// stuck due to reference to undefined variable or unresolvable cross-references
			// iterate all remaining expressions and output them to log
			// execute another resolver pass but capture all the unresolved variables this time
			patchContext.unresolvedSymbols.clear();
			_resolverPass(patchContext, unresolvedPatches, true);
			// generate messages
			if(isLastPass)
				patchContext.errorHandler.printError(nullptr, -1, "Some symbols could not be resolved because the dependency chain is too deep");
			for (auto& itr : patchContext.unresolvedSymbols)
				patchContext.errorHandler.printError(itr.patchGroup, itr.lineNumber, fmt::format("Unresolved symbol: {}", itr.symbolName));
			patchContext.errorHandler.showStageErrorMessageBox();
			return;
		}
	}
	if (!isUnresolvedPatchesEmpty() || patchContext.errorHandler.hasError())
	{
		patchContext.errorHandler.showStageErrorMessageBox();
		return;
	}
	// apply relocated patches
	for (auto patchGroup : groups)
	{
		for (auto& patch : patchGroup->list_patches)
		{
			PatchEntryInstruction* patchInstruction = dynamic_cast<PatchEntryInstruction*>(patch);
			if (patchInstruction == nullptr)
				continue;
			patchInstruction->applyPatch();
		}
	}
	// mark groups as applied
	for (auto patchGroup : groups)
		patchGroup->setApplied();
}

void GraphicPack2::UndoPatchGroups(std::vector<PatchGroup*>& groups, const RPLModule* rpl)
{
	// restore original data
	for (auto patchGroup : groups)
	{
		if (!patchGroup->isApplied())
			continue;
		for (auto& patch : patchGroup->list_patches)
		{
			PatchEntryInstruction* patchInstruction = dynamic_cast<PatchEntryInstruction*>(patch);
			if (patchInstruction == nullptr)
				continue;
			patchInstruction->undoPatch();
		}
	}
	// mark groups as not applied
	for (auto patchGroup : groups)
		patchGroup->resetApplied();
}

void GraphicPack2::NotifyModuleLoaded(const RPLModule* rpl)
{
	cemuLog_log(LogType::Force, "Loaded module \'{}\' with checksum 0x{:08x}", rpl->moduleName2, rpl->patchCRC);

	std::lock_guard<std::recursive_mutex> lock(mtx_patches);
	list_modules.emplace_back(rpl);

	// todo - iterate all active graphic packs and apply any matching patch groups
}

void GraphicPack2::NotifyModuleUnloaded(const RPLModule* rpl)
{
	std::lock_guard<std::recursive_mutex> lock(mtx_patches);
	list_modules.erase(std::remove(list_modules.begin(), list_modules.end(), rpl), list_modules.end());
}
