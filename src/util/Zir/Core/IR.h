#pragma once
#include <optional>

using f32 = float;
using f64 = double;

inline void zpir_debug_assert(bool _cond) 
{
	if(!_cond)
		assert_dbg();
}

namespace ZpIR
{
	//enum class ZpIRCmdForm : uint8
	//{
	//	FORM_VOID, // no-op
	//	FORM_ZERO, // opcode without operands
	//	FORM_1OP, // op0
	//	FORM_2OP, // op0, op1
	//	FORM_3OP, // op0, op1, op2
	//	FORM_4OP,  // op0, op1, op2, op3
	//	// todo - memory read + memory store
	//	FORM_MEM, // op0, opEA, offset, type
	//	// todo - function calls

	//};

	//enum class ZpIROpcodeDepr : uint8
	//{
	//	OP_VOID,
	//	// FORM_1OP
	//	OP_CALL,
	//	// FORM_2OP
	//	OP_ASSIGN, // copy/assignment
	//	OP_CAST_ZEROEXT, // cast type to another. If broadening then zero-extend (unsigned cast)
	//	OP_CAST_SIGNEXT, // cast type to another. If broadening then sign-extend (signed cast)
	//	// FORM_3OP
	//	OP_ADD, // op0 = op1 + op2
	//	OP_SUB, // op0 = op1 - op2
	//	OP_MUL, // op0 = op1 * op2
	//	OP_DIV, // op0 = op1 / op2
	//	// memory
	//	OP_MEM_READ,
	//	OP_MEM_WRITE,
	//};

	enum class DataType : uint8
	{
		NONE = 0x00,
		// integer
		U8 = 1,
		S8 = 2,
		U16 = 3,
		S16 = 4,
		U32 = 5,
		S32 = 6,
		U64 = 7,
		S64 = 8,
		// floating-point
		F32 = 0x10 + 0,
		F64 = 0x10 + 1,
		// special
		POINTER = 0x20, // dynamic width based on pointer width of target architecture
		// boolean
		BOOL = 0x30, // can hold false/true. Size depends on target architecture
	};

	typedef uint16 IRReg;
	typedef uint64 LocationSymbolName;
	typedef uint32 ZpIRPhysicalReg;

	inline bool isRegVar(IRReg r) { return r < 0x8000; };
	inline bool isConstVar(IRReg r) { return r >= 0x8000; };
	inline uint16 getRegIndex(IRReg r) { return (uint16)r & 0x7FFF; };

	namespace IR
	{
		enum class OpCode : uint8
		{
			UNDEF = 0, // undefined 
			// basic opcodes
			MOV,

			// basic arithmetic opcodes
			ADD, // addition
			SUB, // subtraction
			MUL, // multiplication
			DIV, // division

			// conversion
			BITCAST, // like MOV, but allows registers of different types. No value conversion happens, raw bit copy
			SWAP_ENDIAN, // swap endianness
			CONVERT_INT_TO_FLOAT,
			CONVERT_FLOAT_TO_INT,

			// misc
			IMPORT_SINGLE, // import into a single IRReg. Depr: Make this like EXPORT where there is a 1-4 regs variant and one for more
			IMPORT, // import from external/custom resource into 1-4 IRReg
			
			EXPORT, // export 1-4 registers to external/custom resource
			// EXPORT_MANY // for when more than 4 registers are needed
			


			// vector
			EXTRACT_ELEMENT, // extract a scalar type from a vector type
			// some notes: We need this for texture read instructions. Where the result is a vec4 (f32x4) and this is how we can extract individual registers from that
			//             update -> We may also instead just let the texture sample instruction specify 4 output registers
		};

		enum class OpForm : uint8
		{
			NONE = 0,
			RR = 1,
			RRR = 2,
			IMPORT_SINGLE = 3, // deprecated
			IMPORT = 4,
			EXPORT = 5,
		};

		// instruction base class
		class __InsBase
		{
		public:

			OpCode opcode;
			OpForm opform;
			__InsBase* next;
		protected:
			__InsBase(OpCode opcode, OpForm opform) : opcode(opcode), opform(opform) { };
		};

		// adapted base class, instruction forms inherit from this
		template<typename TInstr, OpForm TOpForm>
		class __InsBaseWithForm : public __InsBase
		{
		public:

			//OpCode opcode;
			//OpForm opform;
			//__InsBase* next;

			static const OpForm getForm()
			{
				return TOpForm;
			}

			static TInstr* getIfForm(__InsBase* instructionBase)
			{
				if (instructionBase->opform != TOpForm)
					return nullptr;
				return (TInstr*)instructionBase;
			}

		protected:
			__InsBaseWithForm(OpCode opcode) : __InsBase(opcode, TOpForm) { };
		};

		class InsRR : public __InsBaseWithForm<InsRR, OpForm::RR>
		{
		public:
			InsRR(OpCode opcode, IRReg rA, IRReg rB) : __InsBaseWithForm(opcode), rA(rA), rB(rB) {};

			IRReg rA;
			IRReg rB;
		};

		class InsRRR : public __InsBaseWithForm<InsRRR, OpForm::RRR>
		{
		public:
			InsRRR(OpCode opcode, IRReg rA, IRReg rB, IRReg rC) : __InsBaseWithForm(opcode), rA(rA), rB(rB), rC(rC) {};

			IRReg rA;
			IRReg rB;
			IRReg rC;
		};

		// should we support RRI format with 32bit signed integer as a way to avoid having to generate dozens of IR const regs for stuff like shift and other logical instructions with constant rhs?
		// and if we do, should it be a 32bit signed integer or should the type match the instruction type?

		class InsEXPORT : public __InsBaseWithForm<InsEXPORT, OpForm::EXPORT>
		{
		public:
			InsEXPORT(LocationSymbolName exportSymbol, IRReg r) : __InsBaseWithForm(OpCode::EXPORT), exportSymbol(exportSymbol)
			{
				regArray[0] = r;
				count = 1;
			};

			InsEXPORT(LocationSymbolName exportSymbol, IRReg r0, IRReg r1) : __InsBaseWithForm(OpCode::EXPORT), exportSymbol(exportSymbol)
			{
				regArray[0] = r0; regArray[1] = r1;
				count = 2;
			};

			InsEXPORT(LocationSymbolName exportSymbol, IRReg r0, IRReg r1, IRReg r2) : __InsBaseWithForm(OpCode::EXPORT), exportSymbol(exportSymbol)
			{
				regArray[0] = r0; regArray[1] = r1; regArray[2] = r2;
				count = 3;
			};

			InsEXPORT(LocationSymbolName exportSymbol, IRReg r0, IRReg r1, IRReg r2, IRReg r3) : __InsBaseWithForm(OpCode::EXPORT), exportSymbol(exportSymbol)
			{
				regArray[0] = r0;
				regArray[1] = r1;
				regArray[2] = r2;
				regArray[3] = r3;
				count = 4;
			};

			InsEXPORT(LocationSymbolName exportSymbol, std::span<IRReg> regs) : __InsBaseWithForm(OpCode::EXPORT), exportSymbol(exportSymbol)
			{
				zpir_debug_assert(regs.size() <= 4);
				for(size_t i=0; i<regs.size(); i++)
					regArray[i] = regs[i];
				count = (uint16)regs.size();
			};

			uint16 count;
			IRReg regArray[4]; // up to 4 registers
			LocationSymbolName exportSymbol;
		};

		class InsIMPORT : public __InsBaseWithForm<InsIMPORT, OpForm::IMPORT>
		{
		public:
			InsIMPORT(LocationSymbolName importSymbol, IRReg r) : __InsBaseWithForm(OpCode::IMPORT), importSymbol(importSymbol)
			{
				regArray[0] = r;
				count = 1;
			};

			InsIMPORT(LocationSymbolName importSymbol, IRReg r0, IRReg r1) : __InsBaseWithForm(OpCode::IMPORT), importSymbol(importSymbol)
			{
				regArray[0] = r0; regArray[1] = r1;
				count = 2;
			};

			InsIMPORT(LocationSymbolName importSymbol, IRReg r0, IRReg r1, IRReg r2) : __InsBaseWithForm(OpCode::IMPORT), importSymbol(importSymbol)
			{
				regArray[0] = r0; regArray[1] = r1; regArray[2] = r2;
				count = 3;
			};

			InsIMPORT(LocationSymbolName importSymbol, IRReg r0, IRReg r1, IRReg r2, IRReg r3) : __InsBaseWithForm(OpCode::IMPORT), importSymbol(importSymbol)
			{
				regArray[0] = r0;
				regArray[1] = r1;
				regArray[2] = r2;
				regArray[3] = r3;
				count = 4;
			};

			InsIMPORT(LocationSymbolName importSymbol, std::span<IRReg> regs) : __InsBaseWithForm(OpCode::IMPORT), importSymbol(importSymbol)
			{
				zpir_debug_assert(regs.size() <= 4);
				for (size_t i = 0; i < regs.size(); i++)
					regArray[i] = regs[i];
				count = (uint16)regs.size();
			};

			uint16 count;
			IRReg regArray[4]; // up to 4 registers
			LocationSymbolName importSymbol;
		};
	};

	// IR register definition stored in basic block
	struct IRRegDef 
	{
		IRRegDef(DataType type, uint8 elementCount) : type(type), elementCount(elementCount) {};

		DataType type;
		uint8 elementCount; // 1 = scalar
		ZpIRPhysicalReg physicalRegister{ std::numeric_limits<ZpIRPhysicalReg>::max()};
		// todo - information about spilling location? (it depends on the architecture so we should keep this out of the core IR)
		bool hasAssignedPhysicalRegister() const
		{
			return physicalRegister != std::numeric_limits<ZpIRPhysicalReg>::max();
		}

		void assignPhysicalRegister(ZpIRPhysicalReg physReg)
		{
			physicalRegister = physReg;
		}
	};

	// IR register constant definition stored in basic block
	struct IRRegConstDef
	{
		IRRegConstDef() = default;
		// todo - support for constants with more than one element?

		IRRegConstDef& setU32(uint32 v) { value_u32 = v; type = DataType::U32; return *this; };
		IRRegConstDef& setS32(sint32 v) { value_s32 = v; type = DataType::S32; return *this; };
		IRRegConstDef& setF32(f32 v) { value_f32 = v; type = DataType::F32; return *this; };
		IRRegConstDef& setPtr(void* v) { value_ptr = v; type = DataType::POINTER; return *this; };
		IRRegConstDef& setRaw(uint32 v, DataType regType) { value_u32 = v; type = regType; return *this; };

		DataType type{ DataType::NONE };
		union
		{
			uint32 value_u32;
			sint32 value_s32;
			sint64 value_s64;
			uint64 value_u64;
			void* value_ptr;
			f32 value_f32;
			f64 value_f64;
		};
	};

	struct ZpIRBasicBlock
	{
		friend class ZpIRBuilder;

		struct IRBBImport
		{
			IRBBImport(IRReg reg, LocationSymbolName name) : reg(reg), name(name) {};

			IRReg reg;
			LocationSymbolName name;
		};

		struct IRBBExport
		{
			IRBBExport(IRReg reg, LocationSymbolName name) : reg(reg), name(name) {};

			IRReg reg;
			LocationSymbolName name;
		};

		IR::__InsBase* m_instructionFirst{};
		IR::__InsBase* m_instructionLast{};
		std::vector<IRRegDef> m_regs;
		std::vector<IRRegConstDef> m_consts;
		std::vector<IRBBImport> m_imports;
		std::vector<IRBBExport> m_exports;
		ZpIRBasicBlock* m_branchNotTaken{ nullptr }; // next block if branch not taken or no branch present
		ZpIRBasicBlock* m_branchTaken{ nullptr }; // next block if branch is taken

		void* m_workbuffer{}; // can be used as temporary storage for information

		void appendInstruction(IR::__InsBase* ins)
		{
			if (m_instructionFirst == nullptr)
			{
				m_instructionFirst = ins;
				m_instructionLast = ins;
				ins->next = nullptr;
				return;
			}
			m_instructionLast->next = ins;
			m_instructionLast = ins;
			ins->next = nullptr;
		}

		IRReg createRegister(DataType type, uint8 elementCount = 1)
		{
			uint32 index = (uint32)m_regs.size();
			cemu_assert_debug(index < 0x8000);
			m_regs.emplace_back(type, elementCount);
			return (IRReg)index;
		}

		IRReg createConstantU32(uint32 value)
		{
			uint32 index = (uint32)m_consts.size();
			cemu_assert_debug(index < 0x8000);
			m_consts.emplace_back().setU32(value);
			return (IRReg)((uint16)index + 0x8000);
		}

		IRReg createTypedConstant(uint32 value, DataType type)
		{
			uint32 index = (uint32)m_consts.size();
			cemu_assert_debug(index < 0x8000);
			m_consts.emplace_back().setRaw(value, type);
			return (IRReg)((uint16)index + 0x8000);
		}

		IRReg createConstantS32(uint32 value)
		{
			uint32 index = (uint32)m_consts.size();
			cemu_assert_debug(index < 0x8000);
			m_consts.emplace_back().setS32(value);
			return (IRReg)((uint16)index + 0x8000);
		}

		IRReg createConstantF32(f32 value)
		{
			uint32 index = (uint32)m_consts.size();
			cemu_assert_debug(index < 0x8000);
			m_consts.emplace_back().setF32(value);
			return (IRReg)((uint16)index + 0x8000);
		}

		IRReg createConstantPointer(void* value)
		{
			uint32 index = (uint32)m_consts.size();
			cemu_assert_debug(index < 0x8000);
			m_consts.emplace_back().setPtr(value);
			return (IRReg)((uint16)index + 0x8000);
		}

		void addImport(IRReg reg, LocationSymbolName importName)
		{
			m_imports.emplace_back(reg, importName);
		}

		void addExport(IRReg reg, LocationSymbolName importName)
		{
			m_exports.emplace_back(reg, importName);
		}

		void setWorkbuffer(void* buffer)
		{
			if (buffer != nullptr)
			{
				if (m_workbuffer)
					assert_dbg();
			}
			m_workbuffer = buffer;
		}

		void* getWorkbuffer()
		{
			return m_workbuffer;
		}


		DataType getRegType(IRReg reg)
		{
			uint32 index = (uint32)reg;
			if (index >= 0x8000)
			{
				index -= 0x8000;
				cemu_assert_debug(index < m_consts.size());
				return m_consts[index].type;
			}
			return m_regs[index].type;
		}

		IRRegConstDef* getConstant(IRReg reg)
		{
			uint32 index = (uint32)reg;
			if (index < 0x8000)
				return nullptr;
			index -= 0x8000;
			if (index >= m_consts.size())
				return nullptr;
			return m_consts.data() + index;
		}

		std::optional<sint32> getConstantS32(IRReg reg)
		{
			uint32 index = (uint32)reg;
			if (index < 0x8000)
				return std::nullopt;
			index -= 0x8000;
			if (index >= m_consts.size())
				return std::nullopt;
			if (m_consts[index].type == DataType::U32)
				return (sint32)m_consts[index].value_u32;
			else if (m_consts[index].type == DataType::POINTER)
				assert_dbg();
			else if (m_consts[index].type == DataType::U64)
			{
				if (m_consts[index].value_u64 >= 0x80000000ull)
					assert_dbg();
				return (sint32)m_consts[index].value_u64;
			}
			else
				assert_dbg();
			return std::nullopt;
		}

		std::optional<uint64> getConstantU64(IRReg reg)
		{
			auto constReg = getConstant(reg);
			if (!constReg)
				return std::nullopt;
			if (constReg->type == DataType::U64)
				return constReg->value_u64;
			else
				assert_dbg();
			return std::nullopt;
		}
	};

	struct ZpIRFunction
	{
		std::vector<ZpIRBasicBlock*> m_basicBlocks;
		std::vector<ZpIRBasicBlock*> m_entryBlocks;
		std::vector<ZpIRBasicBlock*> m_exitBlocks;

		struct  
		{
			bool registersAllocated{false};
		}state;
	};

	// helpers for shader code
	namespace ShaderSubset
	{
		class ShaderImportLocation
		{
			enum LOC_TYPE : uint8
			{
				LOC_TYPE_UNIFORM_REGISTER = 1,
				LOC_TYPE_UNIFORM_BUFFER = 2,
				LOC_TYPE_ATTRIBUTE = 3,
			};
		public:
			ShaderImportLocation() = default;
			ShaderImportLocation(LocationSymbolName loc) 
			{
				uint64 v = (uint64)loc;

				m_locType = (LOC_TYPE)(v >> 56);
				m_indexA = (uint16)(v >> 0);
				m_indexB = (uint16)(v >> 16);
			}

			ShaderImportLocation& SetUniformRegister(uint16 index)
			{
				m_locType = LOC_TYPE_UNIFORM_REGISTER;
				m_indexA = index;
				m_indexB = 0;
				return *this;
			}

			ShaderImportLocation& SetVertexAttribute(uint16 attributeIndex, uint16 channelIndex)
			{
				m_locType = LOC_TYPE_ATTRIBUTE;
				m_indexA = attributeIndex;
				m_indexB = channelIndex;
				return *this;
			}

			bool IsUniformRegister() const
			{
				return m_locType == LOC_TYPE_UNIFORM_REGISTER;
			}

			bool IsVertexAttribute() const
			{
				return m_locType == LOC_TYPE_ATTRIBUTE;
			}

			void GetUniformRegister(uint16& index)
			{
				index = m_indexA;
			}

			void GetVertexAttribute(uint16& attributeIndex, uint16& channelIndex) const
			{
				attributeIndex = m_indexA;
				channelIndex = m_indexB;
			}

			operator LocationSymbolName() const 
			{ 
				uint64 v = 0;
				v |= ((uint64)m_locType << 56);
				v |= ((uint64)m_indexA << 0);
				v |= ((uint64)m_indexB << 16);

				return (LocationSymbolName)v;
			}

			std::string GetDebugName()
			{
				const char elementTable[] = { 'x' , 'y', 'z', 'w' };

				if (m_locType == LOC_TYPE_UNIFORM_REGISTER)
					return fmt::format("UniformReg[{0}].{1}", m_indexA >> 2, elementTable[m_indexA & 3]);
				if (m_locType == LOC_TYPE_ATTRIBUTE)
					return fmt::format("VertexAttribute[{0}].{1}", m_indexA, elementTable[m_indexB]);

				return "Unknown";
			}

		private:
			LOC_TYPE m_locType{};
			uint16 m_indexA{};
			uint16 m_indexB{};
			//LocationSymbolName m_symbolName{};
			static_assert(sizeof(LocationSymbolName) == 8);
		};

		class ShaderExportLocation
		{
			enum LOC_TYPE : uint8
			{
				LOC_TYPE_POSITION = 1,
				LOC_TYPE_OUTPUT = 2,
			};
		public:
			ShaderExportLocation() = default;
			ShaderExportLocation(LocationSymbolName loc)
			{
				uint64 v = (uint64)loc;

				m_locType = (LOC_TYPE)(v >> 56);
				m_indexA = (uint16)(v >> 0);
				m_indexB = (uint16)(v >> 16);
			}

			ShaderExportLocation& SetPosition()
			{
				m_locType = LOC_TYPE_POSITION;
				m_indexA = 0;
				m_indexB = 0;
				return *this;
			}

			ShaderExportLocation& SetOutputAttribute(uint16 attributeIndex) // todo - channel mask?
			{
				m_locType = LOC_TYPE_OUTPUT;
				m_indexA = attributeIndex;
				m_indexB = 0;
				return *this;
			}

			bool IsPosition() const
			{
				return m_locType == LOC_TYPE_POSITION;
			}

			bool IsOutputAttribute() const
			{
				return m_locType == LOC_TYPE_OUTPUT;
			}

			void GetOutputAttribute(uint16& attributeIndex) const
			{
				attributeIndex = m_indexA;
			}

			operator LocationSymbolName() const
			{
				uint64 v = 0;
				v |= ((uint64)m_locType << 56);
				v |= ((uint64)m_indexA << 0);
				v |= ((uint64)m_indexB << 16);

				return (LocationSymbolName)v;
			}

			std::string GetDebugName()
			{
				const char elementTable[] = { 'x' , 'y', 'z', 'w' };

				//if (m_locType == LOC_TYPE_UNIFORM_REGISTER)
				//	return fmt::format("UniformReg[{0}].{1}", m_indexA >> 2, elementTable[m_indexA & 3]);
				//if (m_locType == LOC_TYPE_ATTRIBUTE)
				//	return fmt::format("VertexAttribute[{0}].{1}", m_indexA, elementTable[m_indexB]);

				return "Unknown";
			}

		private:
			LOC_TYPE m_locType{};
			uint16 m_indexA{};
			uint16 m_indexB{};
			static_assert(sizeof(LocationSymbolName) == 8);
		};

	};
}
