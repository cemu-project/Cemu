#pragma once
#include <wchar.h>

#include <boost/container/small_vector.hpp>

#include "../fsc.h"

// path parser and utility class for Wii U paths
// optimized to be allocation-free for common path lengths
class FSCPath
{
	struct PathNode
	{
		PathNode(uint16 offset, uint16 len) : offset(offset), len(len) {};

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
		if (nameLen == 1 && *name == '.')
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
		return std::basic_string_view<char>(m_names.data() + m_nodes[index].offset, m_nodes[index].len);
	}

	// returns true if the node names match according to FSA case-insensitivity rules
	static bool MatchNodeName(std::string_view name1, std::string_view name2)
	{
		if (name1.size() != name2.size())
			return false;
		for (size_t i = 0; i < name1.size(); i++)
		{
			char c1 = name1[i];
			char c2 = name2[i];
			if (c1 >= 'A' && c1 <= 'Z')
				c1 += ('a' - 'A');
			if (c2 >= 'A' && c2 <= 'Z')
				c2 += ('a' - 'A');
			if (c1 != c2)
				return false;
		}
		return true;
	}

	bool MatchNodeName(sint32 index, std::string_view name) const
	{
		if (index < 0 || index >= (sint32)m_nodes.size())
			return false;
		auto nodeName = GetNodeName(index);
		return MatchNodeName(nodeName, name);
	}
};

template<typename F>
class FSAFileTree
{
  private:

	enum NODETYPE : uint8
	{
		NODETYPE_DIRECTORY,
		NODETYPE_FILE,
	};

	struct node_t
	{
		std::string name;
		std::vector<node_t*> subnodes;
		size_t fileSize;
		F* custom;
		NODETYPE type;
	};

	node_t* getByNodePath(FSCPath& p, sint32 numNodes, bool createAsDirectories)
	{
		node_t* currentNode = &rootNode;
		for (sint32 i = 0; i < numNodes; i++)
		{
			// find subnode by path
			node_t* foundSubnode = getSubnode(currentNode, p.GetNodeName(i));
			if (foundSubnode == nullptr)
			{
				// no subnode found -> create new directory node (if requested)
				if (createAsDirectories == false)
					return nullptr; // path not found
				currentNode = newNode(currentNode, NODETYPE_DIRECTORY, p.GetNodeName(i));
			}
			else
			{
				currentNode = foundSubnode;
			}
		}
		return currentNode;
	}

	node_t* getSubnode(node_t* parentNode, std::string_view name)
	{
		for (auto& sn : parentNode->subnodes)
		{
			if (FSCPath::MatchNodeName(sn->name, name))
				return sn;
		}
		return nullptr;
	}

	node_t* newNode(node_t* parentNode, NODETYPE type, std::string_view name)
	{
		node_t* newNode = new node_t;
		newNode->name.assign(name);
		newNode->type = type;
		newNode->custom = nullptr;
		parentNode->subnodes.push_back(newNode);
		return newNode;
	}

	class DirectoryIterator : public FSCVirtualFile
	{
	  public:
		DirectoryIterator(node_t* node)
			: m_node(node), m_subnodeIndex(0)
		{
		}

		sint32 fscGetType() override
		{
			return FSC_TYPE_DIRECTORY;
		}

		bool fscDirNext(FSCDirEntry* dirEntry) override
		{
			if (m_subnodeIndex >= m_node->subnodes.size())
				return false;

			const node_t* subnode = m_node->subnodes[m_subnodeIndex];

			strncpy(dirEntry->path, subnode->name.c_str(), sizeof(dirEntry->path) - 1);
			dirEntry->path[sizeof(dirEntry->path) - 1] = '\0';
			dirEntry->isDirectory = subnode->type == FSAFileTree::NODETYPE_DIRECTORY;
			dirEntry->isFile = subnode->type == FSAFileTree::NODETYPE_FILE;
			dirEntry->fileSize = subnode->type == FSAFileTree::NODETYPE_FILE ? subnode->fileSize : 0;

			++m_subnodeIndex;
			return true;
		}

		bool fscRewindDir() override
		{
			m_subnodeIndex = 0;
			return true;
		}

	  private:
		node_t* m_node;
		size_t m_subnodeIndex;
	};

public:
	FSAFileTree()
	{
		rootNode.type = NODETYPE_DIRECTORY;
	}

	bool addFile(std::string_view path, size_t fileSize, F* custom)
	{
		FSCPath p(path);
		if (p.GetNodeCount() == 0)
			return false;
		node_t* directoryNode = getByNodePath(p, p.GetNodeCount() - 1, true);
		// check if a node with same name already exists
		if (getSubnode(directoryNode, p.GetNodeName(p.GetNodeCount() - 1)) != nullptr)
			return false; // node already exists
		// add file node
		node_t* fileNode = newNode(directoryNode, NODETYPE_FILE, p.GetNodeName(p.GetNodeCount() - 1));
		fileNode->fileSize = fileSize;
		fileNode->custom = custom;
		return true;
	}

	bool getFile(std::string_view path, F* &custom)
	{
		FSCPath p(path);
		if (p.GetNodeCount() == 0)
			return false;
		node_t* node = getByNodePath(p, p.GetNodeCount(), false);
		if (node == nullptr)
			return false;
		if (node->type != NODETYPE_FILE)
			return false;
		custom = node->custom;
		return true;
	}

	bool getDirectory(std::string_view path, FSCVirtualFile*& dirIterator)
	{
		FSCPath p(path);
		if (p.GetNodeCount() == 0)
			return false;
		node_t* node = getByNodePath(p, p.GetNodeCount(), false);
		if (node == nullptr)
			return false;
		if (node->type != NODETYPE_DIRECTORY)
			return false;
		dirIterator = new DirectoryIterator(node);
		return true;
	}

	bool removeFile(std::string_view path)
	{
		FSCPath p(path);
		if (p.GetNodeCount() == 0)
			return false;
		node_t* directoryNode = getByNodePath(p, p.GetNodeCount() - 1, false);
		if (directoryNode == nullptr)
			return false;
		// find node
		node_t* fileNode = getSubnode(directoryNode, p.GetNodeName(p.GetNodeCount() - 1));
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
		directoryNode->subnodes.erase(std::remove(directoryNode->subnodes.begin(), directoryNode->subnodes.end(), fileNode), directoryNode->subnodes.end());
		// delete node
		delete fileNode;
		return true;
	}

	template<typename TFunc>
	bool listDirectory(std::string_view path, TFunc fn)
	{
		FSCPath p(path);
		node_t* node = getByNodePath(p, p.GetNodeCount(), false);
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

static void FSTPathUnitTest()
{
	// test 1
	FSCPath p1("/vol/content");
	cemu_assert_debug(p1.GetNodeCount() == 2);
	cemu_assert_debug(p1.MatchNodeName(0, "tst") == false);
	cemu_assert_debug(p1.MatchNodeName(0, "vol"));
	cemu_assert_debug(p1.MatchNodeName(1, "CONTENT"));
	// test 2
	FSCPath p2("/vol/content/");
	cemu_assert_debug(p2.GetNodeCount() == 2);
	cemu_assert_debug(p2.MatchNodeName(0, "vol"));
	cemu_assert_debug(p2.MatchNodeName(1, "content"));
	// test 3
	FSCPath p3("/vol//content/\\/");
	cemu_assert_debug(p3.GetNodeCount() == 2);
	cemu_assert_debug(p3.MatchNodeName(0, "vol"));
	cemu_assert_debug(p3.MatchNodeName(1, "content"));
	// test 4
	FSCPath p4("vol/content/");
	cemu_assert_debug(p4.GetNodeCount() == 2);
	// test 5
	FSCPath p5("/vol/content/test.bin");
	cemu_assert_debug(p5.GetNodeCount() == 3);
	cemu_assert_debug(p5.MatchNodeName(0, "vol"));
	cemu_assert_debug(p5.MatchNodeName(1, "content"));
	cemu_assert_debug(p5.MatchNodeName(2, "TEST.BIN"));
	// test 6 - empty paths
	FSCPath p6("");
	cemu_assert_debug(p6.GetNodeCount() == 0);
	p6 = FSCPath("/////////////");
	cemu_assert_debug(p6.GetNodeCount() == 0);
	// test 7 - periods in path
	FSCPath p7("/vol/content/./..");
	cemu_assert_debug(p7.GetNodeCount() == 3);
	cemu_assert_debug(p7.MatchNodeName(0, "vol"));
	cemu_assert_debug(p7.MatchNodeName(1, "content"));
	cemu_assert_debug(p7.MatchNodeName(2, ".."));
}






