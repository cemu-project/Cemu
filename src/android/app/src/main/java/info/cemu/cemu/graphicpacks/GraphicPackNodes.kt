package info.cemu.cemu.graphicpacks

import info.cemu.cemu.nativeinterface.NativeGraphicPacks
import kotlin.math.max

sealed class GraphicPackNode(
    val name: String?,
    val parent: GraphicPackSectionNode?
) {
    var titleIdInstalled: Boolean = false
        protected set

    fun isRoot() = parent == null
}

class GraphicPackSectionNode : GraphicPackNode {
    constructor() : super(null, null)

    constructor(
        name: String,
        titleIdInstalled: Boolean,
        parent: GraphicPackSectionNode
    ) : super(name, parent) {
        this.titleIdInstalled = titleIdInstalled
    }

    var enabledGraphicPacksCount: Int = 0
        private set
    var children: ArrayList<GraphicPackNode> = ArrayList()
        private set

    fun clear() {
        children.clear()
    }

    fun addGraphicPackDataByTokens(
        graphicPackBasicInfo: NativeGraphicPacks.GraphicPackBasicInfo,
        titleIdInstalled: Boolean
    ) {
        var node = this
        val tokens = graphicPackBasicInfo.virtualPath.split("/")
        if (tokens.isEmpty()) {
            return
        }
        for (token in tokens.subList(0, tokens.size - 1)) {
            node = getOrAddSectionByToken(node, token, node.children, titleIdInstalled)
        }
        if (graphicPackBasicInfo.enabled) {
            node.updateEnabledCount(true)
        }
        node.children.add(
            GraphicPackDataNode(
                graphicPackBasicInfo.id,
                tokens.last(),
                graphicPackBasicInfo.virtualPath,
                graphicPackBasicInfo.enabled,
                titleIdInstalled,
                node
            )
        )
    }

    private fun getOrAddSectionByToken(
        parent: GraphicPackSectionNode,
        token: String,
        graphicPackNodes: ArrayList<GraphicPackNode>,
        titleIdInstalled: Boolean
    ): GraphicPackSectionNode {
        val existingSectionNode =
            graphicPackNodes.firstOrNull { it is GraphicPackSectionNode && it.name == token }
        if (existingSectionNode != null) {
            return existingSectionNode as GraphicPackSectionNode
        }
        val sectionNode = GraphicPackSectionNode(token, titleIdInstalled, parent)
        graphicPackNodes.add(sectionNode)
        return sectionNode
    }

    fun sort() {
        children.forEach { if (it is GraphicPackSectionNode) it.sort() }
        children.sortBy { it.name }
    }

    fun updateEnabledCount(enabled: Boolean) {
        enabledGraphicPacksCount = max(0, enabledGraphicPacksCount + if (enabled) 1 else -1)
        parent?.updateEnabledCount(enabled)
    }
}

class GraphicPackDataNode(
    val id: Long,
    name: String,
    val path: String,
    enabled: Boolean,
    parent: GraphicPackSectionNode,
) : GraphicPackNode(name, parent) {
    constructor(
        id: Long,
        name: String,
        path: String,
        enabled: Boolean,
        titleIdInstalled: Boolean,
        parentNode: GraphicPackSectionNode,
    ) : this(id, name, path, enabled, parentNode) {
        this.titleIdInstalled = titleIdInstalled
    }

    var enabled: Boolean = enabled
        set(value) {
            if (field == value) {
                return
            }
            field = value
            parent?.updateEnabledCount(value)
        }
}
