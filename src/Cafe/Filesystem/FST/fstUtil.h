#pragma once
#include <wchar.h>

class parsedPathW
{
	static const int MAX_NODES = 32;

  public:
	parsedPathW(std::wstring_view path)
	{
		// init vars
		this->numNodes = 0;
		// init parsed path data
		if (path.front() == '/')
			path.remove_prefix(1);
		pathData.assign(path.begin(), path.end());
		pathData.push_back('\0');
		// start parsing
		sint32 offset = 0;
		sint32 startOffset = 0;
		if (offset < pathData.size() - 1)
		{
			this->nodeOffset[this->numNodes] = offset;
			this->numNodes++;
		}
		while (offset < pathData.size() - 1)
		{
			if (this->pathData[offset] == '/' || this->pathData[offset] == '\\')
			{
				this->pathData[offset] = '\0';
				offset++;
				// double slashes are ignored and instead are handled like a single slash
				if (this->pathData[offset] == '/' || this->pathData[offset] == '\\')
				{
					// if we're in the beginning and having a \\ it's a network path
					if (offset != 1)
					{
						this->pathData[offset] = '\0';
						offset++;
					}
				}
				// start new node
				if (this->numNodes < MAX_NODES)
				{
					if (offset < pathData.size() - 1)
					{
						this->nodeOffset[this->numNodes] = offset;
						this->numNodes++;
					}
				}
				continue;
			}
			offset++;
		}
		// handle special nodes like '.' or '..'
		sint32 nodeIndex = 0;
		while (nodeIndex < this->numNodes)
		{
			if (compareNodeName(nodeIndex, L".."))
			{
				assert(false);
			}
			else if (compareNodeName(nodeIndex, L"."))
			{
				removeNode(nodeIndex);
				continue;
			}
			nodeIndex++;
		}
	}

	// returns true if the names match (case sensitive)
	bool compareNodeName(sint32 index, std::wstring_view name)
	{
		if (index < 0 || index >= this->numNodes)
			return false;
		const wchar_t* nodeName = this->pathData.data() + this->nodeOffset[index];
		if (boost::iequals(nodeName, name))
			return true;
		return false;
	}

	static bool compareNodeName(std::wstring_view name1, std::wstring_view name2)
	{
		if (boost::iequals(name1, name2))
			return true;
		return false;
	}

	static bool compareNodeNameCaseInsensitive(std::wstring_view name1, std::wstring_view name2)
	{
		if (boost::iequals(name1, name2))
			return true;
		return false;
	}

	const wchar_t* getNodeName(sint32 index)
	{
		if (index < 0 || index >= this->numNodes)
			return nullptr;
		return this->pathData.data() + this->nodeOffset[index];
	}

	void removeNode(sint32 index)
	{
		if (index < 0 || index >= numNodes)
			return;
		numNodes--;
		for (sint32 i = 0; i < numNodes; i++)
		{
			nodeOffset[i] = nodeOffset[i + 1];
		}
		// remove empty space
		if (numNodes > 0)
			updateOffsets(nodeOffset[0]);
	}

	void prependNode(wchar_t* newNode)
	{
		if (numNodes >= MAX_NODES)
			return;
		sint32 len = (sint32)wcslen(newNode);
		updateOffsets(-(len + 1));
		numNodes++;
		for (sint32 i = numNodes - 1; i >= 1; i--)
		{
			nodeOffset[i] = nodeOffset[i - 1];
		}
		nodeOffset[0] = 0;
		memcpy(pathData.data() + 0, newNode, (len + 1) * sizeof(wchar_t));
	}

	void buildPathString(std::wstring& pathStr, bool appendSlash = false)
	{
		pathStr.resize(0);
		for (sint32 i = 0; i < numNodes; i++)
		{
			if (numNodes > 1)
				pathStr.append(L"/");
			pathStr.append(pathData.data() + nodeOffset[i]);
		}
		if (appendSlash)
			pathStr.append(L"/");
	}

  private:
	void updateOffsets(sint32 newBaseOffset)
	{
		if (numNodes == 0 || newBaseOffset == 0)
			return;
		cemu_assert_debug(newBaseOffset <= nodeOffset[0]);
		if (newBaseOffset > 0)
		{
			// decrease size
			memmove(pathData.data(), pathData.data() + newBaseOffset,
					(pathData.size() - newBaseOffset) * sizeof(wchar_t));
			pathData.resize(pathData.size() - newBaseOffset);
			// update node offsets
			for (sint32 i = 0; i < numNodes; i++)
			{
				nodeOffset[i] -= newBaseOffset;
			}
		}
		else
		{
			// increase size
			newBaseOffset = -newBaseOffset;
			pathData.resize(pathData.size() + newBaseOffset);
			memmove(pathData.data() + newBaseOffset, pathData.data(),
					(pathData.size() - newBaseOffset) * sizeof(wchar_t));
			// update node offsets
			for (sint32 i = 0; i < numNodes; i++)
			{
				nodeOffset[i] += newBaseOffset;
			}
		}
	}

  private:
	// std::wstring pathData;
	std::vector<wchar_t> pathData;
	sint32 nodeOffset[MAX_NODES + 1];

  public:
	sint32 numNodes;
};

template<typename F, bool isCaseSensitive>
class FileTree
{
  public:
  private:
	enum NODETYPE : uint8
	{
		NODETYPE_DIRECTORY,
		NODETYPE_FILE,
	};

	typedef struct _node_t
	{
		std::wstring name;
		std::vector<_node_t*> subnodes;
		F* custom;
		NODETYPE type;
	} node_t;

	node_t* getByNodePath(parsedPathW& p, sint32 numNodes, bool createAsDirectories)
	{
		node_t* currentNode = &rootNode;
		for (sint32 i = 0; i < numNodes; i++)
		{
			// find subnode by path
			node_t* foundSubnode = getSubnode(currentNode, p.getNodeName(i));
			if (foundSubnode == nullptr)
			{
				// no subnode found -> create new directory node (if requested)
				if (createAsDirectories == false)
					return nullptr; // path not found
				currentNode = newNode(currentNode, NODETYPE_DIRECTORY, p.getNodeName(i));
			}
			else
			{
				currentNode = foundSubnode;
			}
		}
		return currentNode;
	}

	node_t* getSubnode(node_t* parentNode, std::wstring_view name)
	{
		for (auto& sn : parentNode->subnodes)
		{
			if constexpr (isCaseSensitive)
			{
				if (parsedPathW::compareNodeName(sn->name.c_str(), name))
					return sn;
			}
			else
			{
				if (parsedPathW::compareNodeNameCaseInsensitive(sn->name.c_str(), name))
					return sn;
			}
		}
		return nullptr;
	}

	node_t* newNode(node_t* parentNode, NODETYPE type, std::wstring_view name)
	{
		node_t* newNode = new node_t;
		newNode->name = std::wstring(name);
		newNode->type = type;
		newNode->custom = nullptr;
		parentNode->subnodes.push_back(newNode);
		return newNode;
	}

  public:
	FileTree()
	{
		rootNode.type = NODETYPE_DIRECTORY;
	}

	bool addFile(const wchar_t* path, F* custom)
	{
		parsedPathW p(path);
		if (p.numNodes == 0)
			return false;
		node_t* directoryNode = getByNodePath(p, p.numNodes - 1, true);
		// check if a node with same name already exists
		if (getSubnode(directoryNode, p.getNodeName(p.numNodes - 1)) != nullptr)
			return false; // node already exists
		// add file node
		node_t* fileNode = newNode(directoryNode, NODETYPE_FILE, p.getNodeName(p.numNodes - 1));
		fileNode->custom = custom;
		return true;
	}

	bool getFile(std::wstring_view path, F*& custom)
	{
		parsedPathW p(path);
		if (p.numNodes == 0)
			return false;
		node_t* node = getByNodePath(p, p.numNodes, false);
		if (node == nullptr)
			return false;
		if (node->type != NODETYPE_FILE)
			return false;
		custom = node->custom;
		return true;
	}

	bool removeFile(std::wstring_view path)
	{
		parsedPathW p(path);
		if (p.numNodes == 0)
			return false;
		node_t* directoryNode = getByNodePath(p, p.numNodes - 1, false);
		if (directoryNode == nullptr)
			return false;
		// find node
		node_t* fileNode = getSubnode(directoryNode, p.getNodeName(p.numNodes - 1));
		if (fileNode == nullptr)
			return false;
		if (fileNode->type != NODETYPE_FILE)
			return false;
		if (fileNode->subnodes.empty() == false)
		{
			// files shouldn't have subnodes
			assert(false);
		}
		// remove node from parent
		directoryNode->subnodes.erase(
			std::remove(directoryNode->subnodes.begin(), directoryNode->subnodes.end(), fileNode),
			directoryNode->subnodes.end());
		// delete node
		delete fileNode;
		return true;
	}

	template<typename TFunc>
	bool listDirectory(const wchar_t* path, TFunc fn)
	{
		parsedPathW p(path);
		node_t* node = getByNodePath(p, p.numNodes, false);
		if (node == nullptr)
			return false;
		if (node->type != NODETYPE_DIRECTORY)
			return false;
		for (auto& it : node->subnodes)
		{
			if (it->type == NODETYPE_DIRECTORY)
			{
				fn(it->name, true, it->custom);
			}
			else if (it->type == NODETYPE_FILE)
			{
				fn(it->name, false, it->custom);
			}
		}
		return true;
	}

  private:
	node_t rootNode;
};

#include <boost/container/small_vector.hpp>

// path parser and utility class for Wii U paths
// optimized to be allocation-free for common path lengths
class FSCPath
{
	struct PathNode
	{
		PathNode(uint16 offset, uint16 len) : offset(offset), len(len){};

		uint16 offset;
		uint16 len;
	};

	boost::container::small_vector<PathNode, 8> m_nodes;
	boost::container::small_vector<char, 64> m_names;
	bool m_isAbsolute{};

	inline bool isSlash(char c)
	{
		return c == '\\' || c == '/';
	}

	void appendNode(const char* name, uint16 nameLen)
	{
		if (m_names.size() > 0xFFFF)
			return;
		m_nodes.emplace_back((uint16)m_names.size(), nameLen);
		m_names.insert(m_names.end(), name, name + nameLen);
	}

  public:
	FSCPath(std::string_view path)
	{
		if (path.empty())
			return;
		if (isSlash(path.front()))
		{
			m_isAbsolute = true;
			path.remove_prefix(1);
			// skip any additional leading slashes
			while (!path.empty() && isSlash(path.front()))
				path.remove_prefix(1);
		}
		// parse nodes
		size_t n = 0;
		size_t nodeNameStartIndex = 0;
		while (n < path.size())
		{
			if (isSlash(path[n]))
			{
				size_t nodeNameLen = n - nodeNameStartIndex;
				if (nodeNameLen > 0xFFFF)
					nodeNameLen = 0xFFFF; // truncate suspiciously long node names
				cemu_assert_debug(nodeNameLen > 0);
				appendNode(path.data() + nodeNameStartIndex, (uint16)nodeNameLen);
				// skip any repeating slashes
				while (n < path.size() && isSlash(path[n]))
					n++;
				nodeNameStartIndex = n;
				continue;
			}
			n++;
		}
		if (nodeNameStartIndex < n)
		{
			size_t nodeNameLen = n - nodeNameStartIndex;
			if (nodeNameLen > 0xFFFF)
				nodeNameLen = 0xFFFF; // truncate suspiciously long node names
			appendNode(path.data() + nodeNameStartIndex, (uint16)nodeNameLen);
		}
	}

	size_t GetNodeCount() const
	{
		return m_nodes.size();
	}

	std::string_view GetNodeName(size_t index) const
	{
		if (index < 0 || index >= m_nodes.size())
			return std::basic_string_view<char>();
		return std::basic_string_view<char>(m_names.data() + m_nodes[index].offset,
											m_nodes[index].len);
	}

	bool MatchNode(sint32 index, std::string_view name) const
	{
		if (index < 0 || index >= (sint32)m_nodes.size())
			return false;
		auto nodeName = GetNodeName(index);
		if (nodeName.size() != name.size())
			return false;
		for (size_t i = 0; i < nodeName.size(); i++)
		{
			char c1 = nodeName[i];
			char c2 = name[i];
			if (c1 >= 'A' && c1 <= 'Z')
				c1 += ('a' - 'A');
			if (c2 >= 'A' && c2 <= 'Z')
				c2 += ('a' - 'A');
			if (c1 != c2)
				return false;
		}
		return true;
	}
};

static void FSTPathUnitTest()
{
	// test 1
	FSCPath p1("/vol/content");
	cemu_assert_debug(p1.GetNodeCount() == 2);
	cemu_assert_debug(p1.MatchNode(0, "tst") == false);
	cemu_assert_debug(p1.MatchNode(0, "vol"));
	cemu_assert_debug(p1.MatchNode(1, "CONTENT"));
	// test 2
	FSCPath p2("/vol/content/");
	cemu_assert_debug(p2.GetNodeCount() == 2);
	cemu_assert_debug(p2.MatchNode(0, "vol"));
	cemu_assert_debug(p2.MatchNode(1, "content"));
	// test 3
	FSCPath p3("/vol//content/\\/");
	cemu_assert_debug(p3.GetNodeCount() == 2);
	cemu_assert_debug(p3.MatchNode(0, "vol"));
	cemu_assert_debug(p3.MatchNode(1, "content"));
	// test 4
	FSCPath p4("vol/content/");
	cemu_assert_debug(p4.GetNodeCount() == 2);
	// test 5
	FSCPath p5("/vol/content/test.bin");
	cemu_assert_debug(p5.GetNodeCount() == 3);
	cemu_assert_debug(p5.MatchNode(0, "vol"));
	cemu_assert_debug(p5.MatchNode(1, "content"));
	cemu_assert_debug(p5.MatchNode(2, "TEST.BIN"));
	// test 6 - empty paths
	FSCPath p6("");
	cemu_assert_debug(p6.GetNodeCount() == 0);
	p6 = FSCPath("/////////////");
	cemu_assert_debug(p6.GetNodeCount() == 0);
}
