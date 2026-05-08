#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "util/ChunkedHeap/ChunkedHeap.h"
#include "util/helpers/fspinlock.h"
#include "config/ActiveSettings.h"

#define CACHE_PAGE_SIZE		0x400
#define CACHE_PAGE_SIZE_M1	(CACHE_PAGE_SIZE-1)

uint32 g_currentCacheChronon = 0;

template<typename TRangeType, typename TNodeObject>
class IntervalTree
{
	static constexpr TRangeType MAX_VALUE = std::numeric_limits<TRangeType>::max();
	static constexpr uint32 ROOT_NODE_INDEX = 0;
	static constexpr uint32 INVALID_NODE_INDEX = 0xFFFFFFFFu;

	static constexpr sint32 NUM_SLOTS = 16;
	static constexpr sint32 MIN_CHILDREN = 7; // try to merge nodes below this count

	struct TreeNode
	{
		TRangeType values[NUM_SLOTS];
		sint32 indices[NUM_SLOTS]; // for the second to last layer these are indices into value nodes vector. Otherwise its a relative byte offset to a tree node
		uint32 selfIndex{ INVALID_NODE_INDEX };
		uint32 parentNodeIndex{ INVALID_NODE_INDEX };
		uint8 parentSlot{ 0 };
		uint8 usedCount{ 0 };
	};

	struct ValueNode
	{
		ValueNode() = default;
		ValueNode(TNodeObject* _ptr, TRangeType _rangeBegin, TRangeType _rangeEnd, uint32 _selfIndex) : ptr(_ptr), rangeBegin(_rangeBegin), rangeEnd(_rangeEnd), selfIndex(_selfIndex) {}

		TNodeObject* ptr{ nullptr };
		TRangeType rangeBegin{};
		TRangeType rangeEnd{};
		uint32 selfIndex{ INVALID_NODE_INDEX };
		uint32 parentNodeIndex{ INVALID_NODE_INDEX };
		uint8 parentSlot{ 0 };
	};
public:
	IntervalTree()
	{
		// create root node
		m_treeNodes.push_back({});
		m_treeNodes[0].selfIndex = 0;
		m_freeTreeNodeIndices.push_back(0);
		TreeNode& rootNode = AllocateTreeNode(INVALID_NODE_INDEX, 0);
		cemu_assert_debug(rootNode.selfIndex == 0);
		ReserveNodes();
	}

	TNodeObject* GetRange(TRangeType rangeBegin)
	{
		cemu_assert_debug(rangeBegin < MAX_VALUE);
		if (IsEmpty())
			return nullptr;
		const ValueNode* valueNode = FindFloorValueNode(rangeBegin);
		if (valueNode == nullptr)
			return nullptr;
		if (rangeBegin >= valueNode->rangeEnd)
			return nullptr;
		return valueNode->ptr;
	}

	void GetOverlappingRanges(TRangeType rangeBegin, TRangeType rangeEnd, std::vector<TNodeObject*>& results)
	{
		results.clear();
		cemu_assert_debug(rangeBegin < rangeEnd);
		if (IsEmpty())
			return;
		if (rangeBegin >= rangeEnd)
			return;
		const ValueNode* valueNode = FindFloorValueNode(rangeBegin);
		if (!valueNode)
			valueNode = GetLeftmostValue();
		if (rangeBegin < valueNode->rangeEnd && rangeEnd > valueNode->rangeBegin)
			results.emplace_back(valueNode->ptr);
		valueNode = GetSuccessorValue(valueNode);
		while (valueNode != nullptr)
		{
			if (valueNode->rangeBegin >= rangeEnd)
				break;
			if (valueNode->rangeEnd > rangeBegin)
				results.emplace_back(valueNode->ptr);
			valueNode = GetSuccessorValue(valueNode);
		}
	}

	// will assert if no exact match was found
	void RemoveRange(TRangeType rangeBegin, TRangeType rangeEnd)
	{
		ValueNode* valueNode = FindFloorValueNode(rangeBegin);
		cemu_assert(valueNode);
		cemu_assert(valueNode->rangeBegin == rangeBegin && valueNode->rangeEnd == rangeEnd);
		TreeNode* parentNode = &m_treeNodes[valueNode->parentNodeIndex];
		sint32 parentSlot = valueNode->parentSlot;
		// remove value from parent
		ReduceUsedCount(parentNode, 1);
		for (sint32 i=parentSlot; i<parentNode->usedCount; i++)
			SetChildNode(*parentNode, i, m_valueNodes[parentNode->indices[i+1]]);
		if (parentSlot == 0 && parentNode->usedCount > 0)
			PropagateMinValue(parentNode);
		// release value
		ReleaseValueNode(valueNode->selfIndex);
		// if parent node now has few nodes then merge/redistribute it
		CollapseNode(parentNode, m_treeDepth-1);
		ShortenTreeIfPossible();
	}

	FORCE_INLINE TreeNode* GetTreeNodeChild(TreeNode* treeNode, sint32 slot)
	{
		cemu_assert_debug(slot >= 0 && slot < NUM_SLOTS);
		char* base = reinterpret_cast<char*>(treeNode);
		return reinterpret_cast<TreeNode*>(base + treeNode->indices[slot]);
	}

	void SetChildNode(TreeNode& parent, sint32 slot, TreeNode* child)
	{
		cemu_assert_debug(slot >= 0 && slot < NUM_SLOTS);
		cemu_assert_debug(child->usedCount > 0); // cant determine value if node has no children
		uint32 parentIndex = parent.selfIndex;
		child->parentNodeIndex = parentIndex;
		child->parentSlot = slot;
		parent.values[slot] = child->values[0];
		parent.indices[slot] = (sint32)(child - &parent) * (sint32)sizeof(TreeNode);
	}

	void SetChildNode(TreeNode& parent, sint32 slot, ValueNode& child)
	{
		cemu_assert_debug(slot >= 0 && slot < NUM_SLOTS);
		uint32 childIndex = child.selfIndex;
		uint32 parentIndex = parent.selfIndex;
		child.parentNodeIndex = parentIndex;
		child.parentSlot = slot;
		parent.values[slot] = child.rangeBegin;
		parent.indices[slot] = childIndex;
	}

	void AddRange(TRangeType rangeBegin, TRangeType rangeEnd, TNodeObject* nodeObject)
	{
		ReserveNodes();
		cemu_assert_debug(rangeBegin < rangeEnd);
		if (IsEmpty()) [[unlikely]]
		{
			// special case, insert into empty tree
			TreeNode& rootNode = GetRootNode();
			cemu_assert_debug(rootNode.usedCount == 0);
			cemu_assert_debug(m_treeDepth == 0);
			rootNode.usedCount = 1;
			SetChildNode(rootNode, 0, AllocateValueNode(rangeBegin, rangeEnd, nodeObject));
			m_treeDepth = 1;
			return;
		}
		// find the preceding value, we insert after it in the same parent
		TreeNode* insertNode = nullptr;
		sint32 insertSlotIndex = -1;
		{
			ValueNode* valueNode = FindFloorValueNode(rangeBegin);
			if (valueNode)
			{
				insertSlotIndex = valueNode->parentSlot+1; // insert after
			}
			else
			{
				// special case where we insert at the very left
				valueNode = GetLeftmostValue();
				insertSlotIndex = 0;
			}
			cemu_assert_debug(valueNode->parentNodeIndex != INVALID_NODE_INDEX);
			insertNode = &m_treeNodes[valueNode->parentNodeIndex];
		}
		// handle the case where the node can still fit more entries
		if (insertNode->usedCount < NUM_SLOTS)
		{
			for (sint32 i=insertNode->usedCount-1; i>=insertSlotIndex; i--)
				SetChildNode(*insertNode, i+1, m_valueNodes[insertNode->indices[i]]);
			insertNode->usedCount++;
			SetChildNode(*insertNode, insertSlotIndex, AllocateValueNode(rangeBegin, rangeEnd, nodeObject));
			if (insertSlotIndex == 0)
				PropagateMinValue(insertNode);
			return;
		}
		// split is necessary
		InsertNode(*insertNode, nullptr, &AllocateValueNode(rangeBegin, rangeEnd, nodeObject));
	}

	void WriteDotFile(const char* filePath)
	{
		std::ofstream outFile(filePath, std::ios::trunc);
		if (!outFile.is_open())
			return;
		outFile << "digraph IntervalTree {\n";
		outFile << "  rankdir=TB;\n";
		outFile << "  splines=polyline;\n";
		outFile << "  node [shape=record, fontname=\"Consolas\", fontsize=10];\n";
		outFile << "  edge [arrowhead=vee, arrowsize=0.8, penwidth=1.2];\n";
		if (IsEmpty())
		{
			outFile << "  t0 [label=\"{Tree 0|empty}\"];\n";
			outFile << "}\n";
			return;
		}
		WriteDotTreeNodeRecursive(outFile, GetRootNode(), m_treeDepth);
		outFile << "}\n";
		outFile.flush();
	}

	void ValidateTree(TreeNode& treeNode, sint32 remainingTreeDepth, TRangeType& minChildValue, TRangeType& maxChildValue)
	{
		minChildValue = std::numeric_limits<TRangeType>::max();
		maxChildValue = std::numeric_limits<TRangeType>::min();
		// basic validation
		cemu_assert(treeNode.usedCount > 0); // empty nodes are not allowed
		for (uint32 i=0; i<treeNode.usedCount-1; i++)
		{
			cemu_assert(treeNode.values[i] < treeNode.values[i+1]);
		}
		// handle subnodes and check for disallowed overlaps
		cemu_assert_debug(remainingTreeDepth > 0);
		if (remainingTreeDepth == 1)
		{
			// children are value nodes (leaf)
			for (uint32 i=0; i<treeNode.usedCount; i++)
			{
				ValueNode& valueNode = m_valueNodes[treeNode.indices[i]];
				cemu_assert(treeNode.values[i] == valueNode.rangeBegin);
				minChildValue = std::min(minChildValue, valueNode.rangeBegin);
				maxChildValue = std::max(maxChildValue, valueNode.rangeEnd);
			}
		}
		else
		{
			// children are tree nodes (branch)
			for (uint32 i=0; i<treeNode.usedCount; i++)
			{
				TreeNode& childTreeNode = GetTreeNodeChild(treeNode, i);
				cemu_assert(childTreeNode.parentNodeIndex == treeNode.selfIndex);
				TRangeType currentChildMinVal, currentChildMaxVal;
				ValidateTree(childTreeNode, remainingTreeDepth-1, currentChildMinVal, currentChildMaxVal);
				cemu_assert(currentChildMinVal < currentChildMaxVal);
				cemu_assert(treeNode.values[i] == currentChildMinVal);
				if (i < (treeNode.usedCount-1))
				{
					cemu_assert(currentChildMaxVal <= treeNode.values[i+1]);
				}
				minChildValue = std::min(minChildValue, currentChildMinVal);
				maxChildValue = std::max(maxChildValue, currentChildMaxVal);
			}
			cemu_assert(treeNode.values[0] == minChildValue);
		}
	}

	void InsertNode(TreeNode& nodeToInsertInto, TreeNode* treeNode, ValueNode* valueNode)
	{
		cemu_assert_debug((treeNode != nullptr) != (valueNode != nullptr)); // either treeNode or valueNode can be set but never both or none
		TRangeType rangeBegin = treeNode ? treeNode->values[0] : valueNode->rangeBegin;
		if (nodeToInsertInto.usedCount == NUM_SLOTS)
		{
			// if the target node is full then try to move a child left
			TreeNode* leftNeighbor = GetTreeNodeLeftNeighbor(&nodeToInsertInto);
			if (leftNeighbor && leftNeighbor->usedCount < NUM_SLOTS)
			{
				cemu_assert_debug(rangeBegin > nodeToInsertInto.values[0]); // if this is not true we are not allowed to move the child
				MigrateSubnodesFromRight(&nodeToInsertInto, leftNeighbor, 1, !treeNode);
				cemu_assert_debug(rangeBegin > leftNeighbor->values[leftNeighbor->usedCount-1]);
			}
			else
			{
				// try offload into right neighbor if possible
				TreeNode* rightNeighbor = GetTreeNodeRightNeighbor(&nodeToInsertInto);
				if (rightNeighbor && rightNeighbor->usedCount < NUM_SLOTS)
				{
					cemu_assert_debug(nodeToInsertInto.usedCount == NUM_SLOTS);
					if (rangeBegin > nodeToInsertInto.values[NUM_SLOTS-1])
					{
						// insert into right neighbor instead
						InsertNode(*rightNeighbor, treeNode, valueNode);
						return;
					}
					// offload one child
					MigrateSubnodesFromLeft(&nodeToInsertInto, rightNeighbor, 1, !treeNode);
				}
			}
		}

		if (nodeToInsertInto.usedCount < NUM_SLOTS)
		{
			sint32 insertSlotIndex = 0;
			if (rangeBegin >= nodeToInsertInto.values[0])
				insertSlotIndex = (sint32)FindFloorElementIndexMinBound(nodeToInsertInto, rangeBegin) + 1;
			// we can determine from the parameter the children type of nodeToInsertInto. Tree nodes cannot have mixed tree/value nodes as children
			if (treeNode)
			{
				for (sint32 i=nodeToInsertInto.usedCount-1; i>=insertSlotIndex; i--)
					SetChildNode(nodeToInsertInto, i+1, GetTreeNodeChild(&nodeToInsertInto, i));
			}
			else
			{
				for (sint32 i=nodeToInsertInto.usedCount-1; i>=insertSlotIndex; i--)
					SetChildNode(nodeToInsertInto, i+1, m_valueNodes[nodeToInsertInto.indices[i]]);
			}
			nodeToInsertInto.usedCount++;
			if (treeNode)
				SetChildNode(nodeToInsertInto, insertSlotIndex, treeNode);
			else
				SetChildNode(nodeToInsertInto, insertSlotIndex, *valueNode);
			if (insertSlotIndex == 0)
				PropagateMinValue(&nodeToInsertInto);
			return;
		}
		// target node is full and we need to split
		if (nodeToInsertInto.parentNodeIndex == INVALID_NODE_INDEX)
		{
			// splitting root requires creating a new tree node inbetween and increasing tree depth
			// then we can split the new node like usual
			TreeNode& newTreeNode = AllocateTreeNode(INVALID_NODE_INDEX, 0);
			cemu_assert_debug(nodeToInsertInto.usedCount == NUM_SLOTS);
			cemu_assert_debug(m_treeDepth >= 0);
			if (valueNode)
			{
				for (int i=0; i<NUM_SLOTS; i++)
					SetChildNode(newTreeNode, i, m_valueNodes[nodeToInsertInto.indices[i]]);
			}
			else
			{
				for (int i=0; i<NUM_SLOTS; i++)
					SetChildNode(newTreeNode, i, GetTreeNodeChild(&nodeToInsertInto, i));
			}
			newTreeNode.usedCount = NUM_SLOTS;
			ReduceUsedCount(&nodeToInsertInto, nodeToInsertInto.usedCount - 1); // target count 1
			SetChildNode(nodeToInsertInto, 0, &newTreeNode);
			PropagateMinValue(&newTreeNode);
			m_treeDepth++;
			// try insert again but into the new non-root that can be split
			InsertNode(newTreeNode, treeNode, valueNode);
			return;
		}
		TreeNode& splitLeft = nodeToInsertInto;
		TreeNode& splitRight = AllocateTreeNode(INVALID_NODE_INDEX, 0);
		MigrateSubnodesFromLeft(&splitLeft, &splitRight, NUM_SLOTS/2, valueNode);
		// insert new node into left or right node
		TreeNode* insertNode = rangeBegin >= splitRight.values[0] ? &splitRight : &splitLeft;
		sint32 insertSlotIndex = 0;
		if (rangeBegin >= nodeToInsertInto.values[0])
			insertSlotIndex = (sint32)FindFloorElementIndexMinBound(*insertNode, rangeBegin) + 1;
		if (valueNode)
		{
			for (sint32 i=insertNode->usedCount-1; i>=insertSlotIndex; i--)
				SetChildNode(*insertNode, i+1, m_valueNodes[insertNode->indices[i]]);
		}
		else
		{
			for (sint32 i=insertNode->usedCount-1; i>=insertSlotIndex; i--)
				SetChildNode(*insertNode, i+1, GetTreeNodeChild(insertNode, i));
		}
		insertNode->usedCount++;
		if (treeNode)
			SetChildNode(*insertNode, insertSlotIndex, treeNode);
		else
			SetChildNode(*insertNode, insertSlotIndex, *valueNode);
		// propagate min value
		if (insertSlotIndex == 0 && insertNode == &splitLeft)
			PropagateMinValue(&splitLeft);
		// insert splitRight into parent
		InsertNode(m_treeNodes[splitLeft.parentNodeIndex], &splitRight, nullptr);
	}

	bool IsEmpty() const
	{
		return m_treeDepth == 0;
	}

	void PrintStats()
	{
		cemuLog_log(LogType::Force, "--- IntervalTree info ---");
		cemuLog_log(LogType::Force, "Depth: {}", m_treeDepth);
		size_t numTreeNodes = m_treeNodes.size() - m_freeTreeNodeIndices.size();
		size_t numValueNodes = m_valueNodes.size() - m_freeValueNodeIndices.size();
		cemuLog_log(LogType::Force, "NumTreeNodes: {}", numTreeNodes);
		cemuLog_log(LogType::Force, "NumValueNodes: {}", numValueNodes);
		size_t fillCountTotal = 0;
		for (auto& treeNode : m_treeNodes)
		{
			if (treeNode.usedCount == 0)
				continue;
			fillCountTotal += (size_t)treeNode.usedCount;
		}
		cemuLog_log(LogType::Force, "AvgFillAmount: {:.2f}", (double)fillCountTotal / (double)numTreeNodes);
	}
private:
	TreeNode& GetRootNode()
	{
		return m_treeNodes[ROOT_NODE_INDEX];
	}

	const TreeNode& GetRootNode() const
	{
		return m_treeNodes[ROOT_NODE_INDEX];
	}

	TreeNode& GetNode(uint32 nodeIndex)
	{
		cemu_assert_debug(nodeIndex < m_treeNodes.size());
		return m_treeNodes[nodeIndex];
	}

	const TreeNode& GetNode(uint32 nodeIndex) const
	{
		cemu_assert_debug(nodeIndex < m_treeNodes.size());
		return m_treeNodes[nodeIndex];
	}

	void InitializeTreeNode(TreeNode& node, uint32 selfIndex, uint32 parentNodeIndex, uint8 parentSlot)
	{
		node.selfIndex = selfIndex;
		node.parentNodeIndex = parentNodeIndex;
		node.parentSlot = parentSlot;
		node.usedCount = 0;
		for (auto& it : node.values)
			it = MAX_VALUE;
		for (auto& it : node.indices)
			it = 0;
	}

	void ReduceUsedCount(TreeNode* node, sint32 num)
	{
		cemu_assert_debug(num >= 0);
		cemu_assert_debug(num <= node->usedCount);
		node->usedCount -= (uint8)num;
		for (int i=0; i<num; i++)
		{
			node->values[node->usedCount + i] = MAX_VALUE;
		}
	}

	// assumes value >= node.values[0]
	FORCE_INLINE ptrdiff_t FindFloorElementIndexMinBound(TreeNode& node, TRangeType value)
	{
		static_assert(NUM_SLOTS == 16); // this function needs to be updated if the count changes
		cemu_assert_debug(value >= node.values[0]);
		TRangeType* ptr = node.values;
		if (value >= ptr[8])
			ptr += 8;
		if (value >= ptr[4])
			ptr += 4;
		if (value >= ptr[2])
			ptr += 2;
		if (value >= ptr[1])
			ptr += 1;
		return (ptr - node.values);
	}

	void ReserveNodes()
	{
		// this function guarantees a minimum amount of available TreeNode and ValueNode in the pool, enough to avoid vector invalidation in AddRange()
		if (m_freeTreeNodeIndices.size() < 16)
		{
			uint32 curNodeCount = (uint32)m_treeNodes.size();
			m_treeNodes.resize(curNodeCount + 64);
			for (uint32 i=0; i<64; i++)
				m_treeNodes[curNodeCount + i].selfIndex = curNodeCount + i;
			uint32 freeIndexCount = (uint32)m_freeTreeNodeIndices.size();
			m_freeTreeNodeIndices.resize(freeIndexCount + 64);
			for (uint32 i=0; i<64; i++)
				m_freeTreeNodeIndices[freeIndexCount + i] = curNodeCount + i;
		}
		if (m_freeValueNodeIndices.size() < 16)
		{
			uint32 curNodeCount = (uint32)m_valueNodes.size();
			m_valueNodes.resize(curNodeCount + 128);
			for (uint32 i=0; i<128; i++)
				m_valueNodes[curNodeCount + i].selfIndex = curNodeCount + i;
			uint32 freeIndexCount = (uint32)m_freeValueNodeIndices.size();
			m_freeValueNodeIndices.resize(freeIndexCount + 128);
			for (uint32 i=0; i<128; i++)
				m_freeValueNodeIndices[freeIndexCount + i] = curNodeCount + i;
		}
	}

	void WriteDotTreeNodeRecursive(std::ofstream& outFile, TreeNode& treeNode, sint32 remainingDepth)
	{
		auto writeHex = [&outFile](TRangeType value)
		{
			outFile << "0x" << std::hex << static_cast<uint64>(value) << std::dec;
		};
		outFile << "  t" << treeNode.selfIndex << " [label=\"";
		for (uint32 i = 0; i < NUM_SLOTS; i++)
		{
			if (i != 0)
				outFile << "|";
			outFile << "<s" << i << ">";
			if (i < treeNode.usedCount)
				writeHex(treeNode.values[i]);
		}
		outFile << "\"];\n";
		if (remainingDepth <= 1)
		{
			for (uint32 i = 0; i < treeNode.usedCount; i++)
			{
				const ValueNode& valueNode = m_valueNodes[treeNode.indices[i]];
				outFile << "  v" << valueNode.selfIndex << " [shape=box, label=\"";
				writeHex(valueNode.rangeBegin);
				outFile << "\\n";
				writeHex(valueNode.rangeEnd);
				outFile << "\"];\n";
				outFile << "  t" << treeNode.selfIndex << ":s" << i << ":s -> v" << valueNode.selfIndex << ":nw;\n";
			}
			return;
		}
		for (uint32 i = 0; i < treeNode.usedCount; i++)
		{
			TreeNode* childNode = GetTreeNodeChild(&treeNode, i);
			outFile << "  t" << treeNode.selfIndex << ":s" << i << ":s -> t" << childNode->selfIndex << ":nw;\n";
			WriteDotTreeNodeRecursive(outFile, *childNode, remainingDepth - 1);
		}
	}

	TreeNode& AllocateTreeNode(uint32 parentNodeIndex, uint8 parentSlot)
	{
		cemu_assert(!m_freeTreeNodeIndices.empty());
		uint32 nodeIndex;
		nodeIndex = m_freeTreeNodeIndices.back();
		m_freeTreeNodeIndices.pop_back();
		InitializeTreeNode(m_treeNodes[nodeIndex], nodeIndex, parentNodeIndex, parentSlot);
		return m_treeNodes[nodeIndex];
	}

	void ReleaseTreeNode(TreeNode* treeNode, bool unlinkFromParent)
	{
		uint32 nodeIndex = treeNode->selfIndex;
		if (unlinkFromParent)
		{
			cemu_assert_debug(treeNode->parentNodeIndex != INVALID_NODE_INDEX);
			TreeNode& parentNode = m_treeNodes[treeNode->parentNodeIndex];
			sint32 slotIndex = treeNode->parentSlot;
			ReduceUsedCount(&parentNode, 1);
			for (sint32 i=slotIndex; i<parentNode.usedCount; i++)
				SetChildNode(parentNode, i, GetTreeNodeChild(&parentNode, i + 1));
		}
		cemu_assert_debug(nodeIndex != ROOT_NODE_INDEX);
		if (nodeIndex == ROOT_NODE_INDEX || nodeIndex >= m_treeNodes.size())
			return;
		InitializeTreeNode(m_treeNodes[nodeIndex], nodeIndex, INVALID_NODE_INDEX, 0);
		m_freeTreeNodeIndices.emplace_back(nodeIndex);
	}

	ValueNode& AllocateValueNode(TRangeType beginValue, TRangeType endValue, TNodeObject* nodeObject)
	{
		cemu_assert(!m_freeValueNodeIndices.empty());
		uint32 valueIndex;
		valueIndex = m_freeValueNodeIndices.back();
		m_freeValueNodeIndices.pop_back();
		ValueNode& valueNode = m_valueNodes[valueIndex];
		valueNode.ptr = nodeObject;
		valueNode.rangeBegin = beginValue;
		valueNode.rangeEnd = endValue;
		valueNode.selfIndex = valueIndex;
		valueNode.parentNodeIndex = INVALID_NODE_INDEX;
		valueNode.parentSlot = 0;
		return valueNode;
	}

	void ReleaseValueNode(uint32 valueIndex)
	{
		cemu_assert_debug(valueIndex < m_valueNodes.size());
		ValueNode& valueNode = m_valueNodes[valueIndex];
		valueNode.ptr = nullptr;
		valueNode.rangeBegin = TRangeType{};
		valueNode.rangeEnd = TRangeType{};
		valueNode.parentNodeIndex = INVALID_NODE_INDEX;
		valueNode.parentSlot = 0;
		m_freeValueNodeIndices.emplace_back(valueIndex);
	}

	ValueNode* GetLeftmostValue()
	{
		cemu_assert_debug(!IsEmpty());
		TreeNode* currentNode = &GetRootNode();
		// traverse branches
		for (sint32 i=0; i<m_treeDepth-1; i++)
		{
			currentNode = GetTreeNodeChild(currentNode, 0);
		}
		// get value node
		ValueNode* valueNode = &m_valueNodes[currentNode->indices[0]];
		return valueNode;
	}

	// find the node with the highest rangeBegin that satisfies node->rangeBegin <= beginValue, or null if none exists
	ValueNode* FindFloorValueNode(TRangeType beginValue)
	{
		cemu_assert_debug(beginValue != MAX_VALUE);
		cemu_assert_debug(!IsEmpty());
		TreeNode* currentNode = &GetRootNode();
		if (beginValue < currentNode->values[0])
			return nullptr;
		ptrdiff_t diff = 0;
		auto remainingDepth = m_treeDepth;
		do
		{
			currentNode = reinterpret_cast<TreeNode*>(reinterpret_cast<char*>(currentNode) + diff);
			ptrdiff_t floorSlot = FindFloorElementIndexMinBound(*currentNode, beginValue);
			ASSUME(floorSlot >= 0 && floorSlot < NUM_SLOTS);
			diff = currentNode->indices[floorSlot];
		}
		while (--remainingDepth);
		return &m_valueNodes[diff];
	}

	ValueNode* GetSuccessorValue(const ValueNode* valueNode)
	{
		cemu_assert_debug(valueNode);
		cemu_assert_debug(valueNode->parentNodeIndex != INVALID_NODE_INDEX);
		TreeNode* currentNode = &m_treeNodes[valueNode->parentNodeIndex];
		// straight forward case: There is another value node in the parent tree node
		uint32 currentSlot = valueNode->parentSlot;
		if (currentSlot + 1 < currentNode->usedCount)
			return &m_valueNodes[currentNode->indices[currentSlot + 1]];
		// otherwise traverse towards root until we find a right branch
		sint32 remainingTreeDepth = 0;
		while (true)
		{
			if (currentNode->parentNodeIndex == INVALID_NODE_INDEX)
				return nullptr; // root reached
			currentSlot = currentNode->parentSlot;
			currentNode = &m_treeNodes[currentNode->parentNodeIndex];
			if (currentSlot+1 < currentNode->usedCount)
			{
				currentNode = GetTreeNodeChild(currentNode, currentSlot + 1);
				break;
			}
			remainingTreeDepth++;
		}
		// and then traverse always left to get the value
		for (sint32 i=0; i<remainingTreeDepth; i++)
			currentNode = GetTreeNodeChild(currentNode, 0);
		return &m_valueNodes[currentNode->indices[0]];
	}

	// get the immediate left tree node on the same tree depth, this can be a node on a different parent branch as long as its the same depth
	TreeNode* GetTreeNodeLeftNeighbor(const TreeNode* treeNode)
	{
		cemu_assert_debug(treeNode);
		if (treeNode->parentNodeIndex == INVALID_NODE_INDEX)
			return nullptr; // root has no neighbors
		TreeNode* currentNode = &GetNode(treeNode->parentNodeIndex);
		// straight forward case: There is another tree node in the parent tree node
		uint32 currentSlot = treeNode->parentSlot;
		if (currentSlot > 0)
			return GetTreeNodeChild(currentNode, currentSlot - 1);
		// otherwise traverse towards root until we find a left branch
		sint32 remainingTreeDepth = 0;
		while (true)
		{
			if (currentNode->parentNodeIndex == INVALID_NODE_INDEX)
				return nullptr; // root reached
			currentSlot = currentNode->parentSlot;
			currentNode = &m_treeNodes[currentNode->parentNodeIndex];
			if (currentSlot > 0)
			{
				currentNode = GetTreeNodeChild(currentNode, currentSlot - 1);
				break;
			}
			remainingTreeDepth++;
		}
		// and then traverse always right
		for (sint32 i=0; i<remainingTreeDepth; i++)
			currentNode = GetTreeNodeChild(currentNode, currentNode->usedCount - 1);
		return GetTreeNodeChild(currentNode, currentNode->usedCount - 1);
	}

	// get the immediate right tree node on the same tree depth, this can be a node on a different parent branch as long as its the same depth
	TreeNode* GetTreeNodeRightNeighbor(TreeNode* treeNode)
	{
		cemu_assert_debug(treeNode);
		if (treeNode->parentNodeIndex == INVALID_NODE_INDEX)
			return nullptr; // root has no neighbors
		TreeNode* currentNode = &GetNode(treeNode->parentNodeIndex);
		// straight forward case: There is another tree node in the parent tree node
		uint32 currentSlot = treeNode->parentSlot;
		if (currentSlot + 1 < currentNode->usedCount)
			return GetTreeNodeChild(currentNode, currentSlot + 1);
		// otherwise traverse towards root until we find a right branch
		sint32 remainingTreeDepth = 0;
		while (true)
		{
			if (currentNode->parentNodeIndex == INVALID_NODE_INDEX)
				return nullptr; // root reached
			currentSlot = currentNode->parentSlot;
			currentNode = &m_treeNodes[currentNode->parentNodeIndex];
			if (currentSlot+1 < currentNode->usedCount)
			{
				currentNode = GetTreeNodeChild(currentNode, currentSlot + 1);
				break;
			}
			remainingTreeDepth++;
		}
		// and then traverse always left
		for (sint32 i=0; i<remainingTreeDepth; i++)
			currentNode = GetTreeNodeChild(currentNode, 0);
		return GetTreeNodeChild(currentNode, 0);
	}

	// merge or delete a node if it has less than MIN_CHILDREN. Can decrease tree length
	void CollapseNode(TreeNode* treeNode, sint32 depth)
	{
		if (treeNode->usedCount >= MIN_CHILDREN)
		{
			PropagateMinValue(treeNode);
			return; // node full enough to not collapse
		}
		bool hasValueNodeChildren = (depth == m_treeDepth-1);
		// check if we can merge with left node
		TreeNode* leftNeighbor = GetTreeNodeLeftNeighbor(treeNode);
		if (leftNeighbor && (leftNeighbor->usedCount + treeNode->usedCount) <= NUM_SLOTS && treeNode->usedCount > 0)
		{
			cemu_assert_debug(treeNode->usedCount > 0); // if there are neighbors then empty nodes should not be possible
			// merge left into self
			MigrateSubnodesFromLeft(leftNeighbor, treeNode, leftNeighbor->usedCount, hasValueNodeChildren); // also propagates new min value
			// left neighbor is now empty so remove it
			TreeNode* leftNeighborParent = &m_treeNodes[leftNeighbor->parentNodeIndex];
			ReleaseTreeNode(leftNeighbor, true);
			if (leftNeighbor->parentSlot == 0)
				PropagateMinValue(leftNeighborParent);
			CollapseNode(leftNeighborParent, depth - 1);
			PropagateMinValue(treeNode);
			return;
		}
		// check if we can merge with right node
		TreeNode* rightNeighbor = GetTreeNodeRightNeighbor(treeNode);
		if (rightNeighbor && (rightNeighbor->usedCount + treeNode->usedCount) <= NUM_SLOTS && treeNode->usedCount > 0)
		{
			cemu_assert_debug(treeNode->usedCount > 0);
			// merge right into self
			MigrateSubnodesFromRight(rightNeighbor, treeNode, rightNeighbor->usedCount, hasValueNodeChildren);
			// right neighbor is now empty so remove it
			TreeNode* rightNeighborParent = &m_treeNodes[rightNeighbor->parentNodeIndex];
			ReleaseTreeNode(rightNeighbor, true);
			if (rightNeighbor->parentSlot == 0)
				PropagateMinValue(rightNeighborParent);
			CollapseNode(rightNeighborParent, depth - 1);
			return;
		}
		if (!leftNeighbor && !rightNeighbor)
		{
			// no neighbors
			if (treeNode->usedCount > 0)
			{
				PropagateMinValue(treeNode);
				return;
			}
			// if this node is empty and there are no neighbors then the tree is empty, collapse nodes and eventually root
			if (treeNode->parentNodeIndex == INVALID_NODE_INDEX)
			{
				cemu_assert_debug(treeNode->selfIndex == ROOT_NODE_INDEX);
				m_treeDepth = 0;
				cemu_assert_debug(m_freeValueNodeIndices.size() == m_valueNodes.size());
				cemu_assert_debug(m_freeTreeNodeIndices.size() == m_treeNodes.size()-1); // -1 for the root node which is always reserved
				cemu_assert_debug(GetRootNode().usedCount == 0);
				return;
			}
			else
			{
				// unlink empty node and continue collapsing parent chain
				TreeNode* parentNode = &m_treeNodes[treeNode->parentNodeIndex];
				ReleaseTreeNode(treeNode, true);
				CollapseNode(parentNode, depth - 1);
				return;
			}
		}
		// if merging is not possible then only redistribute values
		if (treeNode->usedCount == 0)
		{
			TreeNode* treeNodeParent = (treeNode->parentNodeIndex != INVALID_NODE_INDEX) ? &m_treeNodes[treeNode->parentNodeIndex] : nullptr;
			ReleaseTreeNode(treeNode, true);
			if (treeNodeParent)
				CollapseNode(treeNodeParent, depth - 1);
			return;
		}
		// todo - redistribute values
		PropagateMinValue(treeNode);
	}

	// helper for CollapseNode(), moves children from left neighbor to center node. Changes left and center node min value
	void MigrateSubnodesFromLeft(TreeNode* leftNode, TreeNode* targetNode, sint32 count, bool hasValueNodes)
	{
		cemu_assert_debug(count > 0);
		cemu_assert_debug(leftNode->usedCount >= count);
		cemu_assert_debug((targetNode->usedCount + count) <= NUM_SLOTS);
		sint32 targetCount = targetNode->usedCount;
		sint32 leftStart = leftNode->usedCount - count;
		if (hasValueNodes)
		{
			for (sint32 i=targetCount-1; i>=0; i--)
				SetChildNode(*targetNode, count + i, m_valueNodes[targetNode->indices[i]]);
			for (sint32 i=0; i<count; i++)
				SetChildNode(*targetNode, i, m_valueNodes[leftNode->indices[leftStart + i]]);
		}
		else
		{
			for (sint32 i=targetCount-1; i>=0; i--)
				SetChildNode(*targetNode, count + i, GetTreeNodeChild(targetNode, i));
			for (sint32 i=0; i<count; i++)
				SetChildNode(*targetNode, i, GetTreeNodeChild(leftNode, leftStart + i));
		}
		ReduceUsedCount(leftNode, count);
		targetNode->usedCount += count;
		PropagateMinValue(targetNode);
	}

	// helper for CollapseNode(), moves children from right neighbor to center node. Changes right node min value
	void MigrateSubnodesFromRight(TreeNode* rightNode, TreeNode* targetNode, sint32 count, bool hasValueNodes)
	{
		cemu_assert_debug(rightNode && targetNode);
		cemu_assert_debug(rightNode != targetNode);
		cemu_assert_debug(count >= 0);
		if (count <= 0)
			return;

		cemu_assert_debug(rightNode->usedCount >= count);
		cemu_assert_debug((targetNode->usedCount + count) <= NUM_SLOTS);

		sint32 targetCount = targetNode->usedCount;
		sint32 rightCount = rightNode->usedCount;
		sint32 remainingCount = rightCount - count;

		if (hasValueNodes)
		{
			for (sint32 i=0; i<count; i++)
				SetChildNode(*targetNode, targetCount + i, m_valueNodes[rightNode->indices[i]]);
			for (sint32 i=0; i<remainingCount; i++)
				SetChildNode(*rightNode, i, m_valueNodes[rightNode->indices[count + i]]);
		}
		else
		{
			for (sint32 i=0; i<count; i++)
				SetChildNode(*targetNode, targetCount + i, GetTreeNodeChild(rightNode, i));
			for (sint32 i=0; i<remainingCount; i++)
				SetChildNode(*rightNode, i, GetTreeNodeChild(rightNode, count + i));
		}
		ReduceUsedCount(rightNode, count);
		targetNode->usedCount += count;
		if (rightNode->usedCount > 0)
			PropagateMinValue(rightNode);
	}

	void ShortenTreeIfPossible()
	{
		if (m_treeDepth == 0)
			return;
		TreeNode& rootNode = GetRootNode();
		cemu_assert_debug(rootNode.usedCount > 0);
		while (m_treeDepth > 1 && rootNode.usedCount == 1) // if root has exactly one tree child, absorb it and reduce depth
		{
			TreeNode* onlyChild = GetTreeNodeChild(&rootNode, 0);
			cemu_assert_debug(onlyChild->usedCount > 0);
			bool childHasValueNodes = (m_treeDepth == 2);
			sint32 childCount = onlyChild->usedCount;
			if (childHasValueNodes)
			{
				for (sint32 i=0; i<childCount; i++)
					SetChildNode(rootNode, i, m_valueNodes[onlyChild->indices[i]]);
			}
			else
			{
				for (sint32 i=0; i<childCount; i++)
					SetChildNode(rootNode, i, GetTreeNodeChild(onlyChild, i));
			}
			rootNode.usedCount = (uint8)childCount;
			for (sint32 i=rootNode.usedCount; i<NUM_SLOTS; i++)
				rootNode.values[i] = MAX_VALUE;
			ReleaseTreeNode(onlyChild, false);
			m_treeDepth--;
		}
	}

	// new minimum value can propagate up the tree
	void PropagateMinValue(TreeNode* node)
	{
		while (true)
		{
			TRangeType minValue = node->values[0];
			if (node->parentNodeIndex == INVALID_NODE_INDEX)
				break; // reached root
			TreeNode* parentNode = &m_treeNodes[node->parentNodeIndex];
			parentNode->values[node->parentSlot] = minValue;
			if (node->parentSlot != 0)
				break; // value wont further propagate
			node = parentNode;
		}
	}

	std::vector<TreeNode> m_treeNodes;
	std::vector<uint32> m_freeTreeNodeIndices;
	std::vector<ValueNode> m_valueNodes;
	std::vector<uint32> m_freeValueNodeIndices;
	sint32 m_treeDepth{0};
};

std::unique_ptr<VHeap> g_gpuBufferHeap = nullptr;
std::vector<uint8> s_pageUploadBuffer;
std::vector<class BufferCacheNode*> s_allCacheNodes;

void LatteBufferCache_removeSingleNodeFromTree(BufferCacheNode* node);

class BufferCacheNode
{
	static inline constexpr uint64 c_streamoutSig0 = 0xF0F0F0F0155C5B6Aull;
	static inline constexpr uint64 c_streamoutSig1 = 0x8BE6336411814F4Full;

public:
	~BufferCacheNode()
	{
		if (m_hasCacheAlloc)
			g_deallocateQueue.emplace_back(m_cacheOffset); // release after current drawcall
		// remove from array
		auto temp = s_allCacheNodes.back();
		s_allCacheNodes.pop_back();
		if (this != temp)
		{
			s_allCacheNodes[m_arrayIndex] = temp;
			temp->m_arrayIndex = m_arrayIndex;
		}
	}

	// returns false if not enough space is available
	bool allocateCacheMemory()
	{
		cemu_assert_debug(m_hasCacheAlloc == false);
		cemu_assert_debug(m_rangeEnd > m_rangeBegin);
		m_hasCacheAlloc = g_gpuBufferHeap->allocOffset(m_rangeEnd - m_rangeBegin, CACHE_PAGE_SIZE, m_cacheOffset);
		return m_hasCacheAlloc;
	}

	void ReleaseCacheMemoryImmediately()
	{
		if (m_hasCacheAlloc)
		{
			g_gpuBufferHeap->freeOffset(m_cacheOffset);
			m_hasCacheAlloc = false;
		}
	}

	uint32 getBufferOffset(MPTR physAddr) const
	{
		cemu_assert_debug(m_hasCacheAlloc);
		cemu_assert_debug(physAddr >= m_rangeBegin);
		cemu_assert_debug(physAddr < m_rangeEnd);
		uint32 relOffset = physAddr - m_rangeBegin;
		return m_cacheOffset + relOffset;
	}

	void writeStreamout(MPTR rangeBegin, MPTR rangeEnd)
	{
		if ((rangeBegin & 0xF))
		{
			cemuLog_logDebugOnce(LogType::Force, "writeStreamout(): RangeBegin not aligned to 16. Begin {:08x} End {:08x}", rangeBegin, rangeEnd);
			rangeBegin = (rangeBegin + 0xF) & ~0xF;
			rangeEnd = std::max(rangeBegin, rangeEnd);
		}
		if (rangeEnd & 0xF)
		{
			// todo - add support for 4 byte granularity for streamout writes and cache
			// used by Affordable Space Adventures and YWW Level 1-8
			// also used by CoD Ghosts (8 byte granularity)
			//cemuLog_logDebug(LogType::Force, "Streamout write size is not aligned to 16 bytes");
			rangeEnd &= ~0xF;
		}
		//cemu_assert_debug((rangeEnd & 0xF) == 0);
		rangeBegin = std::max(rangeBegin, m_rangeBegin);
		rangeEnd = std::min(rangeEnd, m_rangeEnd);
		if (rangeBegin >= rangeEnd)
			return;
		sint32 numPages = getPageCountFromRange(rangeBegin, rangeEnd);
		sint32 pageIndex = getPageIndexFromAddr(rangeBegin);

		cemu_assert_debug((m_rangeBegin + pageIndex * CACHE_PAGE_SIZE) <= rangeBegin);
		cemu_assert_debug((m_rangeBegin + (pageIndex + numPages) * CACHE_PAGE_SIZE) >= rangeEnd);

		for (sint32 i = 0; i < numPages; i++)
		{
			pageWriteStreamoutSignatures(pageIndex, rangeBegin, rangeEnd);
			pageIndex++;
		}
		if (numPages > 0)
			m_hasStreamoutData = true;
	}

	void checkAndSyncModifications(MPTR rangeBegin, MPTR rangeEnd, bool uploadData)
	{
		cemu_assert_debug(rangeBegin >= m_rangeBegin);
		cemu_assert_debug(rangeEnd <= m_rangeEnd);
		cemu_assert_debug(rangeBegin < m_rangeEnd);
		cemu_assert_debug((rangeBegin % CACHE_PAGE_SIZE) == 0);
		cemu_assert_debug((rangeEnd % CACHE_PAGE_SIZE) == 0);

		sint32 basePageIndex = getPageIndexFromAddrAligned(rangeBegin);
		sint32 numPages = getPageCountFromRangeAligned(rangeBegin, rangeEnd);
		uint8* pagePtr = memory_getPointerFromPhysicalOffset(rangeBegin);
		sint32 uploadPageBegin = -1;
		CachePageInfo* pageInfo = m_pageInfo.data() + basePageIndex;
		for (sint32 i = 0; i < numPages; i++)
		{
			if (pageInfo->hasStreamoutData)
			{
				// first upload any pending sequence of pages
				if (uploadPageBegin != -1)
				{
					// upload range
					if (uploadData)
						uploadPages(uploadPageBegin, basePageIndex + i);
					uploadPageBegin = -1;
				}
				// check if hash changed
				uint64 pageHash = hashPage(pagePtr);
				if (pageInfo->hash != pageHash)
				{
					pageInfo->hash = pageHash;
					// for pages that contain streamout data we do uploads with a much smaller granularity
					// and skip uploading any data that is marked with streamout filler bytes
					if (!uploadPageWithStreamoutFiltered(basePageIndex + i))
						pageInfo->hasStreamoutData = false; // all streamout data was replaced
				}
				pagePtr += CACHE_PAGE_SIZE;
				pageInfo++;
				continue;
			}

			uint64 pageHash = hashPage(pagePtr);
			pagePtr += CACHE_PAGE_SIZE;
			if (pageInfo->hash != pageHash)
			{
				if (uploadPageBegin == -1)
					uploadPageBegin = i + basePageIndex;
				pageInfo->hash = pageHash;
			}
			else
			{
				if (uploadPageBegin != -1)
				{
					// upload range
					if (uploadData)
						uploadPages(uploadPageBegin, basePageIndex + i);
					uploadPageBegin = -1;
				}
			}
			pageInfo++;
		}
		if (uploadPageBegin != -1)
		{
			if (uploadData)
				uploadPages(uploadPageBegin, basePageIndex + numPages);
		}
	}

	void checkAndSyncModifications(bool uploadData)
	{
		checkAndSyncModifications(m_rangeBegin, m_rangeEnd, uploadData);
		m_lastModifyCheckCronon = g_currentCacheChronon;
		m_hasInvalidation = false;
	}

	void checkAndSyncModificationsIfChrononChanged(MPTR reservePhysAddress, uint32 reserveSize)
	{
		if (m_lastModifyCheckCronon != g_currentCacheChronon)
		{
			m_lastModifyCheckCronon = g_currentCacheChronon;
			checkAndSyncModifications(m_rangeBegin, m_rangeEnd, true);
			m_hasInvalidation = false;
		}
		if (m_hasInvalidation)
		{
			// ideally we would only upload the pages that intersect both the reserve range and the invalidation range
			// but this would require complex per-page tracking of invalidation. Since this is on a hot path we do a cheap approximation
			// where we only track one continuous invalidation range

			// try to bound uploads to the reserve range within the invalidation
			uint32 resRangeBegin = reservePhysAddress & ~CACHE_PAGE_SIZE_M1;
			uint32 resRangeEnd = ((reservePhysAddress + reserveSize) + CACHE_PAGE_SIZE_M1) & ~CACHE_PAGE_SIZE_M1;

			uint32 uploadBegin = std::max(m_invalidationRangeBegin, resRangeBegin);
			uint32 uploadEnd = std::min(resRangeEnd, m_invalidationRangeEnd);

			if (uploadBegin >= uploadEnd)
				return; // reserve range not within invalidation or range is zero sized

			if (uploadBegin == m_invalidationRangeBegin)
			{
				m_invalidationRangeBegin = uploadEnd;
				checkAndSyncModifications(uploadBegin, uploadEnd, true);
			}
			if (uploadEnd == m_invalidationRangeEnd)
			{
				m_invalidationRangeEnd = uploadBegin;
				checkAndSyncModifications(uploadBegin, uploadEnd, true);
			}
			else
			{
				// upload all of invalidation
				checkAndSyncModifications(m_invalidationRangeBegin, m_invalidationRangeEnd, true);
				m_invalidationRangeBegin = m_invalidationRangeEnd;
			}
			if(m_invalidationRangeEnd <= m_invalidationRangeBegin)
				m_hasInvalidation = false;
		}
	}

	void invalidate(MPTR rangeBegin, MPTR rangeEnd)
	{
		rangeBegin = std::max(rangeBegin, m_rangeBegin);
		rangeEnd = std::min(rangeEnd, m_rangeEnd);
		if (rangeBegin >= rangeEnd)
			return;
		if (m_hasInvalidation)
		{
			m_invalidationRangeBegin = std::min(m_invalidationRangeBegin, rangeBegin);
			m_invalidationRangeEnd = std::max(m_invalidationRangeEnd, rangeEnd);
		}
		else
		{
			m_invalidationRangeBegin = rangeBegin;
			m_invalidationRangeEnd = rangeEnd;
			m_hasInvalidation = true;
		}
		cemu_assert_debug(m_invalidationRangeBegin >= m_rangeBegin);
		cemu_assert_debug(m_invalidationRangeEnd <= m_rangeEnd);
		cemu_assert_debug(m_invalidationRangeBegin < m_invalidationRangeEnd);
		m_invalidationRangeBegin = m_invalidationRangeBegin & ~CACHE_PAGE_SIZE_M1;
		m_invalidationRangeEnd = (m_invalidationRangeEnd + CACHE_PAGE_SIZE_M1) & ~CACHE_PAGE_SIZE_M1;
	}

	void flagInUse()
	{
		m_lastDrawcall = LatteGPUState.drawCallCounter;
		m_lastFrame = LatteGPUState.frameCounter;
	}

	bool isInUse() const
	{
		return m_lastDrawcall == LatteGPUState.drawCallCounter;
	}

	// returns true if the range does not contain any GPU-cache-only data and can be fully restored from RAM
	bool isRAMOnly() const
	{
		return !m_hasStreamoutData;
	}

	MPTR GetRangeBegin() const { return m_rangeBegin; }
	MPTR GetRangeEnd() const { return m_rangeEnd; }

	uint32 GetDrawcallAge() const { return LatteGPUState.drawCallCounter - m_lastDrawcall; };
	uint32 GetFrameAge() const { return LatteGPUState.frameCounter - m_lastFrame; };

	bool HasStreamoutData() const { return m_hasStreamoutData; };

private:
	struct CachePageInfo
	{
		uint64 hash{ 0 };
		bool hasStreamoutData{ false };
	};

	MPTR m_rangeBegin;
	MPTR m_rangeEnd; // (exclusive)
	bool m_hasCacheAlloc{ false };
	uint32 m_cacheOffset{ 0 };
	// usage
	uint32 m_lastDrawcall;
	uint32 m_lastFrame;
	uint32 m_arrayIndex;
	// state tracking
	uint32 m_lastModifyCheckCronon{ g_currentCacheChronon - 1 };
	std::vector<CachePageInfo> m_pageInfo;
	bool m_hasStreamoutData{ false };
	// invalidation
	bool m_hasInvalidation{false};
	MPTR m_invalidationRangeBegin;
	MPTR m_invalidationRangeEnd;

	BufferCacheNode(MPTR rangeBegin, MPTR rangeEnd): m_rangeBegin(rangeBegin), m_rangeEnd(rangeEnd)
	{
		flagInUse();
		cemu_assert_debug(rangeBegin < rangeEnd);
		size_t numPages = getPageCountFromRangeAligned(rangeBegin, rangeEnd);
		m_pageInfo.resize(numPages);
		// append to array
		m_arrayIndex = (uint32)s_allCacheNodes.size();
		s_allCacheNodes.emplace_back(this);
	};

	uint32 getPageIndexFromAddrAligned(uint32 offset) const
	{
		cemu_assert_debug((offset % CACHE_PAGE_SIZE) == 0);
		return (offset - m_rangeBegin) / CACHE_PAGE_SIZE;
	}

	uint32 getPageIndexFromAddr(uint32 offset) const
	{
		offset &= ~CACHE_PAGE_SIZE_M1;
		return (offset - m_rangeBegin) / CACHE_PAGE_SIZE;
	}

	uint32 getPageCountFromRangeAligned(MPTR rangeBegin, MPTR rangeEnd) const
	{
		cemu_assert_debug((rangeBegin % CACHE_PAGE_SIZE) == 0);
		cemu_assert_debug((rangeEnd % CACHE_PAGE_SIZE) == 0);
		cemu_assert_debug(rangeBegin <= rangeEnd);
		return (rangeEnd - rangeBegin) / CACHE_PAGE_SIZE;
	}

	uint32 getPageCountFromRange(MPTR rangeBegin, MPTR rangeEnd) const
	{
		rangeEnd = (rangeEnd + CACHE_PAGE_SIZE_M1) & ~CACHE_PAGE_SIZE_M1;
		rangeBegin &= ~CACHE_PAGE_SIZE_M1;
		cemu_assert_debug(rangeBegin <= rangeEnd);
		return (rangeEnd - rangeBegin) / CACHE_PAGE_SIZE;
	}

	void syncFromRAM(MPTR rangeBegin, MPTR rangeEnd)
	{
		cemu_assert_debug(rangeBegin >= m_rangeBegin);
		cemu_assert_debug(rangeEnd <= m_rangeEnd);
		cemu_assert_debug(rangeEnd > rangeBegin);
		cemu_assert_debug(m_hasCacheAlloc);

		// reset write tracking
		checkAndSyncModifications(rangeBegin, rangeEnd, false);

		g_renderer->bufferCache_upload(memory_getPointerFromPhysicalOffset(rangeBegin), rangeEnd - rangeBegin, getBufferOffset(rangeBegin));
	}

	void syncFromNode(BufferCacheNode* srcNode)
	{
		// get shared range
		MPTR rangeBegin = std::max(m_rangeBegin, srcNode->m_rangeBegin);
		MPTR rangeEnd = std::min(m_rangeEnd, srcNode->m_rangeEnd);
		cemu_assert_debug(rangeBegin < rangeEnd);
		g_renderer->bufferCache_copy(srcNode->getBufferOffset(rangeBegin), this->getBufferOffset(rangeBegin), rangeEnd - rangeBegin);
		// copy page checksums and information
		sint32 numPages = getPageCountFromRangeAligned(rangeBegin, rangeEnd);
		CachePageInfo* pageInfoDst = this->m_pageInfo.data() + this->getPageIndexFromAddrAligned(rangeBegin);
		CachePageInfo* pageInfoSrc = srcNode->m_pageInfo.data() + srcNode->getPageIndexFromAddrAligned(rangeBegin);
		for (sint32 i = 0; i < numPages; i++)
		{
			pageInfoDst[i] = pageInfoSrc[i];
			if (pageInfoSrc[i].hasStreamoutData)
				m_hasStreamoutData = true;
		}
	}

	void uploadPages(uint32 firstPage, uint32 lastPagePlusOne)
	{
		cemu_assert_debug(lastPagePlusOne > firstPage);
		uint32 uploadRangeBegin = m_rangeBegin + firstPage * CACHE_PAGE_SIZE;
		uint32 uploadRangeEnd = m_rangeBegin + lastPagePlusOne * CACHE_PAGE_SIZE;
		cemu_assert_debug(uploadRangeEnd > uploadRangeBegin);
		// make sure uploaded pages and hashes match
		uint32 numPages = lastPagePlusOne - firstPage;
		if (s_pageUploadBuffer.size() < (numPages * CACHE_PAGE_SIZE))
			s_pageUploadBuffer.resize(numPages * CACHE_PAGE_SIZE);
		// todo - improve performance by merging memcpy + hashPage() ?
		memcpy(s_pageUploadBuffer.data(), memory_getPointerFromPhysicalOffset(uploadRangeBegin), numPages * CACHE_PAGE_SIZE);
		for (uint32 i = 0; i < numPages; i++)
		{
			m_pageInfo[firstPage + i].hash = hashPage(s_pageUploadBuffer.data() + i * CACHE_PAGE_SIZE);
		}
		g_renderer->bufferCache_upload(s_pageUploadBuffer.data(), uploadRangeEnd - uploadRangeBegin, getBufferOffset(uploadRangeBegin));
	}

	// upload only non-streamout data of a single page
	// returns true if at least one streamout 16-byte block is present
	// also updates the page hash to match the uploaded data (strict match)
	sint32 uploadPageWithStreamoutFiltered(uint32 pageIndex)
	{
		uint8 pageCopy[CACHE_PAGE_SIZE];
		memcpy(pageCopy, memory_getPointerFromPhysicalOffset(m_rangeBegin + pageIndex * CACHE_PAGE_SIZE), CACHE_PAGE_SIZE);

		MPTR pageBase = m_rangeBegin + pageIndex * CACHE_PAGE_SIZE;

		sint32 blockBegin = -1;
		uint64* pagePtrU64 = (uint64*)pageCopy;
		m_pageInfo[pageIndex].hash = hashPage(pageCopy);
		bool hasStreamoutBlocks = false;
		for (sint32 i = 0; i < CACHE_PAGE_SIZE / 16; i++)
		{
			if (pagePtrU64[0] == c_streamoutSig0 && pagePtrU64[1] == c_streamoutSig1)
			{
				hasStreamoutBlocks = true;
				if (blockBegin != -1)
				{
					uint32 uploadRelRangeBegin = blockBegin * 16;
					uint32 uploadRelRangeEnd = i * 16;
					cemu_assert_debug(uploadRelRangeEnd > uploadRelRangeBegin);
					g_renderer->bufferCache_upload(pageCopy + uploadRelRangeBegin, uploadRelRangeEnd - uploadRelRangeBegin, getBufferOffset(pageBase + uploadRelRangeBegin));
					blockBegin = -1;
				}
				pagePtrU64 += 2;
				continue;
			}
			else if (blockBegin == -1)
				blockBegin = i;
			pagePtrU64 += 2;
		}
		if (blockBegin != -1)
		{
			uint32 uploadRelRangeBegin = blockBegin * 16;
			uint32 uploadRelRangeEnd = CACHE_PAGE_SIZE;
			cemu_assert_debug(uploadRelRangeEnd > uploadRelRangeBegin);
			g_renderer->bufferCache_upload(pageCopy + uploadRelRangeBegin, uploadRelRangeEnd - uploadRelRangeBegin, getBufferOffset(pageBase + uploadRelRangeBegin));
			blockBegin = -1;
		}
		return hasStreamoutBlocks;
	}

	void shrink(MPTR newRangeBegin, MPTR newRangeEnd)
	{
		cemu_assert_debug(newRangeBegin >= m_rangeBegin);
		cemu_assert_debug(newRangeEnd >= m_rangeEnd);
		cemu_assert_debug(newRangeEnd > m_rangeBegin);
		assert_dbg(); // todo (resize page array)
		m_rangeBegin = newRangeBegin;
		m_rangeEnd = newRangeEnd;
	}

	static uint64 hashPage(uint8* mem)
	{
		static const uint64 k0 = 0x55F23EAD;
		static const uint64 k1 = 0x185FDC6D;
		static const uint64 k2 = 0xF7431F49;
		static const uint64 k3 = 0xA4C7AE9D;

		cemu_assert_debug((CACHE_PAGE_SIZE % 32) == 0);
		const uint64* ptr = (const uint64*)mem;
		const uint64* end = ptr + (CACHE_PAGE_SIZE / sizeof(uint64));

		uint64 h0 = 0;
		uint64 h1 = 0;
		uint64 h2 = 0;
		uint64 h3 = 0;
		while (ptr < end)
		{
			h0 = std::rotr(h0, 7);
			h1 = std::rotr(h1, 7);
			h2 = std::rotr(h2, 7);
			h3 = std::rotr(h3, 7);

			h0 += ptr[0] * k0;
			h1 += ptr[1] * k1;
			h2 += ptr[2] * k2;
			h3 += ptr[3] * k3;
			ptr += 4;
		}

		return h0 + h1 + h2 + h3;
	}

	// flag page as having streamout data, also write streamout signatures to page memory
	// also incrementally updates the page hash to include the written signatures, this prevents signature writes from triggering a data upload
	void pageWriteStreamoutSignatures(uint32 pageIndex, MPTR rangeBegin, MPTR rangeEnd)
	{
		uint32 pageRangeBegin = m_rangeBegin + pageIndex * CACHE_PAGE_SIZE;
		uint32 pageRangeEnd = pageRangeBegin + CACHE_PAGE_SIZE;
		rangeBegin = std::max(pageRangeBegin, rangeBegin);
		rangeEnd = std::min(pageRangeEnd, rangeEnd);
		cemu_assert_debug(rangeEnd > rangeBegin);
		cemu_assert_debug(rangeBegin >= pageRangeBegin);
		cemu_assert_debug(rangeEnd <= pageRangeEnd);
		cemu_assert_debug((rangeBegin & 0xF) == 0);
		cemu_assert_debug((rangeEnd & 0xF) == 0);

		auto pageInfo = m_pageInfo.data() + pageIndex;
		pageInfo->hasStreamoutData = true;

		// if the whole page is replaced we can use a cached hash
		if (pageRangeBegin == rangeBegin && pageRangeEnd == rangeEnd)
		{
			uint64* pageMem = (uint64*)memory_getPointerFromPhysicalOffset(rangeBegin);
			uint32 numBlocks = (rangeEnd - rangeBegin) / 16;
			for (uint32 i = 0; i < numBlocks; i++)
			{
				pageMem[0] = c_streamoutSig0;
				pageMem[1] = c_streamoutSig1;
				pageMem += 2;
			}

			pageInfo->hash = c_fullStreamoutPageHash;
			return;
		}

		uint64* pageMem = (uint64*)memory_getPointerFromPhysicalOffset(rangeBegin);
		uint32 numBlocks = (rangeEnd - rangeBegin) / 16;
		uint32 indexHashBlock = (rangeBegin - pageRangeBegin) / sizeof(uint64);
		for (uint32 i = 0; i < numBlocks; i++)
		{
			pageMem[0] = c_streamoutSig0;
			pageMem[1] = c_streamoutSig1;
			pageMem += 2;
		}
		pageInfo->hash = 0; // reset hash
	}

	static uint64 genStreamoutPageHash()
	{
		uint8 pageMem[CACHE_PAGE_SIZE];
		uint64* pageMemU64 = (uint64*)pageMem;
		for (uint32 i = 0; i < sizeof(pageMem) / sizeof(uint64) / 2; i++)
		{
			pageMemU64[0] = c_streamoutSig0;
			pageMemU64[1] = c_streamoutSig1;
			pageMemU64 += 2;
		}

		return hashPage(pageMem);
	}

	static inline uint64 c_fullStreamoutPageHash = genStreamoutPageHash();
	static std::vector<uint32> g_deallocateQueue;

public:
    static void UnloadAll()
    {
        size_t i = 0;
        while (i < s_allCacheNodes.size())
        {
            BufferCacheNode* node = s_allCacheNodes[i];
            node->ReleaseCacheMemoryImmediately();
            LatteBufferCache_removeSingleNodeFromTree(node);
            delete node;
        }
        for(auto& it : s_allCacheNodes)
            delete it;
        s_allCacheNodes.clear();
        g_deallocateQueue.clear();
    }

	static void ProcessDeallocations()
	{
		for(auto& itr : g_deallocateQueue)
			g_gpuBufferHeap->freeOffset(itr);
		g_deallocateQueue.clear();
	}

	// drops everything from the cache that isn't considered in use or unrestorable due to containing streamout data
	static void CleanupCacheAggressive(MPTR excludedRangeBegin, MPTR excludedRangeEnd)
	{
		size_t i = 0;
		while (i < s_allCacheNodes.size())
		{
			BufferCacheNode* node = s_allCacheNodes[i];
			if (node->isInUse())
			{
				i++;
				continue;
			}
			if(!node->isRAMOnly())
			{
				i++;
				continue;
			}
			if(node->GetRangeBegin() < excludedRangeEnd && node->GetRangeEnd() > excludedRangeBegin)
			{
				i++;
				continue;
			}
			// delete range
			if (node->m_hasCacheAlloc)
				cemu_assert_debug(!node->isInUse());
			node->ReleaseCacheMemoryImmediately();
			LatteBufferCache_removeSingleNodeFromTree(node);
			delete node;
		}
	}

	static BufferCacheNode* Create(MPTR rangeBegin, MPTR rangeEnd, std::span<BufferCacheNode*> overlappingObjects)
	{
		auto newRange = new BufferCacheNode(rangeBegin, rangeEnd);
		if (!newRange->allocateCacheMemory())
		{
			// not enough memory available, try to drop ram-only ranges from the ones we replace
			for (size_t i = 0; i < overlappingObjects.size(); i++)
			{
				BufferCacheNode* nodeItr = overlappingObjects[i];
				if (!nodeItr->isInUse() && nodeItr->isRAMOnly())
				{
					nodeItr->ReleaseCacheMemoryImmediately();
					delete nodeItr;
					overlappingObjects[i] = nullptr;
				}
			}
			// retry allocation
			if (!newRange->allocateCacheMemory())
			{
				cemuLog_log(LogType::Force, "Out-of-memory in GPU buffer (trying to allocate: {}KB) Cleaning up cache...", (rangeEnd - rangeBegin + 1023) / 1024);
				CleanupCacheAggressive(rangeBegin, rangeEnd);
				if (!newRange->allocateCacheMemory())
				{
					cemuLog_log(LogType::Force, "Failed to free enough memory in GPU buffer");
					cemu_assert(false);
				}
			}
		}
		newRange->syncFromRAM(rangeBegin, rangeEnd); // possible small optimization: only load the ranges from RAM which are not overwritten by ->syncFromNode()
		for (auto itr : overlappingObjects)
		{
			if(itr == nullptr)
				continue;
			newRange->syncFromNode(itr);
			delete itr;
		}
		return newRange;
	}

	static void Resize(BufferCacheNode* nodeObject, MPTR rangeBegin, MPTR rangeEnd)
	{
		nodeObject->shrink(rangeBegin, rangeEnd);
	}
};

IntervalTree<MPTR, BufferCacheNode> g_gpuBufferCache;
std::vector<BufferCacheNode*> s_gpuCacheQueryResult; // keep vector for query results around to reduce runtime allocations
std::vector<uint32> BufferCacheNode::g_deallocateQueue;

void LatteBufferCache_removeSingleNodeFromTree(BufferCacheNode* node)
{
	g_gpuBufferCache.RemoveRange(node->GetRangeBegin(), node->GetRangeEnd());
}

BufferCacheNode* LatteBufferCache_reserveRange(MPTR physAddress, uint32 size)
{
	MPTR rangeStart = physAddress - (physAddress % CACHE_PAGE_SIZE);
	MPTR rangeEnd = (physAddress + size + CACHE_PAGE_SIZE_M1) & ~CACHE_PAGE_SIZE_M1;

	BufferCacheNode* range = g_gpuBufferCache.GetRange(physAddress);
	if (range && physAddress >= range->GetRangeBegin() && (physAddress+size) <= range->GetRangeEnd())
		return range;
	// no containing range found, we need to create a range and potentially merge with any overlapping ranges
	g_gpuBufferCache.GetOverlappingRanges(rangeStart, rangeEnd, s_gpuCacheQueryResult);
	if (s_gpuCacheQueryResult.empty())
	{
		// no overlaps we can just create a new blank range
		BufferCacheNode* newRange = BufferCacheNode::Create(rangeStart, rangeEnd, s_gpuCacheQueryResult);
		g_gpuBufferCache.AddRange(rangeStart, rangeEnd, newRange);
		return newRange;
	}
	else
	{
		// merge with overlapping ranges
		uint32 mergedRangeStart = std::min<uint32>(rangeStart, s_gpuCacheQueryResult.front()->GetRangeBegin());
		uint32 mergedRangeEnd = std::max<uint32>(rangeEnd, s_gpuCacheQueryResult.back()->GetRangeEnd());
		for (auto& it : s_gpuCacheQueryResult)
			g_gpuBufferCache.RemoveRange(it->GetRangeBegin(), it->GetRangeEnd()); // remove from interval tree, BufferCacheNode::Create below will delete the range objects
		BufferCacheNode* newRange = BufferCacheNode::Create(mergedRangeStart, mergedRangeEnd, s_gpuCacheQueryResult);
		g_gpuBufferCache.AddRange(mergedRangeStart, mergedRangeEnd, newRange);
		return newRange;
	}
}

uint32 LatteBufferCache_retrieveDataInCache(MPTR physAddress, uint32 size)
{
	auto range = LatteBufferCache_reserveRange(physAddress, size);
	range->flagInUse();

	range->checkAndSyncModificationsIfChrononChanged(physAddress, size);

	return range->getBufferOffset(physAddress);
}

void LatteBufferCache_copyStreamoutDataToCache(MPTR physAddress, uint32 size, uint32 streamoutBufferOffset)
{
	if (size == 0)
		return;
	cemu_assert_debug(size >= 16);

	auto range = LatteBufferCache_reserveRange(physAddress, size);
	range->flagInUse();

	g_renderer->bufferCache_copyStreamoutToMainBuffer(streamoutBufferOffset, range->getBufferOffset(physAddress), size);

	// write streamout signatures, flag affected pages
	range->writeStreamout(physAddress, (physAddress + size));
}

void LatteBufferCache_invalidate(MPTR physAddress, uint32 size)
{
	if (size == 0)
		return;
	if (physAddress >= 0xFFFFF000)
		return;
	if ((physAddress+size) < physAddress)
		return;
	g_gpuBufferCache.GetOverlappingRanges(physAddress, physAddress+size, s_gpuCacheQueryResult);
	for (auto& range : s_gpuCacheQueryResult)
	{
		cemu_assert_debug(physAddress < range->GetRangeEnd() && (physAddress + size) > range->GetRangeBegin());
		range->invalidate(physAddress, physAddress + size);
	}
}

// optimized version of LatteBufferCache_invalidate() if physAddress points to the beginning of a page
void LatteBufferCache_invalidatePage(MPTR physAddress)
{
	cemu_assert_debug((physAddress & CACHE_PAGE_SIZE_M1) == 0);
	BufferCacheNode* node = g_gpuBufferCache.GetRange(physAddress);
	if (node)
	{
		cemu_assert_debug(physAddress >= node->GetRangeBegin() && physAddress < node->GetRangeEnd());
		node->invalidate(physAddress, physAddress+CACHE_PAGE_SIZE);
	}
}

void LatteBufferCache_processDeallocations()
{
	BufferCacheNode::ProcessDeallocations();
}

void LatteBufferCache_init(size_t bufferSize)
{
	cemu_assert_debug(g_gpuBufferCache.IsEmpty());
	g_gpuBufferHeap.reset(new VHeap(nullptr, (uint32)bufferSize));
	g_renderer->bufferCache_init((uint32)bufferSize);
}

void LatteBufferCache_UnloadAll()
{
    BufferCacheNode::UnloadAll();
}

void LatteBufferCache_getStats(uint32& heapSize, uint32& allocationSize, uint32& allocNum)
{
	g_gpuBufferHeap->getStats(heapSize, allocationSize, allocNum);
}

class SparseBitset
{
	static inline constexpr size_t TABLE_MASK = 0xFF;

public:
	bool Empty() const
	{
		return m_numNonEmptyVectors == 0;
	}

	void Set(uint32 index)
	{
		auto& v = m_bits[index & TABLE_MASK];
		if (std::find(v.cbegin(), v.cend(), index) != v.end())
			return;
		if (v.empty())
		{
			m_nonEmptyVectors[m_numNonEmptyVectors] = &v;
			m_numNonEmptyVectors++;
		}
		v.emplace_back(index);
	}

	template<typename TFunc>
	void ForAllAndClear(TFunc callbackFunc)
	{
		auto vCurrent = m_nonEmptyVectors + 0;
		auto vEnd = m_nonEmptyVectors + m_numNonEmptyVectors;
		while (vCurrent < vEnd)
		{
			std::vector<uint32>* vec = *vCurrent;
			vCurrent++;
			for (const auto& it : *vec)
				callbackFunc(it);
			vec->clear();
		}
		m_numNonEmptyVectors = 0;
	}

	void Clear()
	{
		auto vCurrent = m_nonEmptyVectors + 0;
		auto vEnd = m_nonEmptyVectors + m_numNonEmptyVectors;
		while (vCurrent < vEnd)
		{
			std::vector<uint32>* vec = *vCurrent;
			vCurrent++;
			vec->clear();
		}
		m_numNonEmptyVectors = 0;
	}

private:
	std::vector<uint32> m_bits[TABLE_MASK + 1];
	std::vector<uint32>* m_nonEmptyVectors[TABLE_MASK + 1];
	size_t m_numNonEmptyVectors{ 0 };
};

FSpinlock g_spinlockDCFlushQueue;
SparseBitset* s_DCFlushQueue = new SparseBitset();
SparseBitset* s_DCFlushQueueAlternate = new SparseBitset();

void LatteBufferCache_notifyDCFlush(MPTR address, uint32 size)
{
	if (address == 0 || size == 0xFFFFFFFF)
		return; // global flushes are ignored for now

	uint32 firstPage = address / CACHE_PAGE_SIZE;
	uint32 lastPage = (address + size - 1) / CACHE_PAGE_SIZE;
	g_spinlockDCFlushQueue.lock();
	for (uint32 i = firstPage; i <= lastPage; i++)
		s_DCFlushQueue->Set(i);
	g_spinlockDCFlushQueue.unlock();
}

void LatteBufferCache_processDCFlushQueue()
{
	if (s_DCFlushQueue->Empty()) // quick check to avoid locking if there is no work to do
		return;
	g_spinlockDCFlushQueue.lock();
	std::swap(s_DCFlushQueue, s_DCFlushQueueAlternate);
	g_spinlockDCFlushQueue.unlock();
	s_DCFlushQueueAlternate->ForAllAndClear([](uint32 index) {LatteBufferCache_invalidatePage(index * CACHE_PAGE_SIZE); });
}

void LatteBufferCache_notifyDrawDone()
{

}

void LatteBufferCache_notifySwapTVScanBuffer()
{
	if( ActiveSettings::FlushGPUCacheOnSwap() )
		g_currentCacheChronon++;
}

void LatteBufferCache_incrementalCleanup()
{
	static uint32 s_counter = 0;

	if (s_allCacheNodes.empty())
		return;

	s_counter++;
	s_counter %= (uint32)s_allCacheNodes.size();

	auto range = s_allCacheNodes[s_counter];

	if (range->HasStreamoutData() && range->GetFrameAge() < 120)
	{
		// todo - proper way to check if streamout data has been overwritten (in RAM) and whether it can be invalidated from GPU cache
		return;
	}

	uint32 heapSize;
	uint32 allocationSize;
	uint32 allocNum;
	g_gpuBufferHeap->getStats(heapSize, allocationSize, allocNum);

	sint32 evictionFrameAge;
	if (allocationSize >= (heapSize * 4 / 5)) // heap is 80% filled
		evictionFrameAge = 2;
	else if (allocationSize >= (heapSize * 3 / 4)) // heap is 75-100% filled
		evictionFrameAge = 4;
	else if (allocationSize >= (heapSize / 2)) // if heap is 50-75% filled
		evictionFrameAge = 20;
	else // heap is under 50% capacity
		evictionFrameAge = 500;
	// evict range if above threshold
	if (range->GetFrameAge() >= evictionFrameAge)
	{
		g_gpuBufferCache.RemoveRange(range->GetRangeBegin(), range->GetRangeEnd());
		delete range;
	}
}
