#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "Common/GLInclude/GLInclude.h"

class LatteQueryObjectGL : public LatteQueryObject
{
	friend class OpenGLRenderer;

	bool getResult(uint64& numSamplesPassed) override;
	void begin() override;
	void end() override;

private:
	GLuint m_queryId{};
	GLenum m_glTarget{};
};

LatteQueryObject* OpenGLRenderer::occlusionQuery_create()
{
	if (!list_queryCacheOcclusion.empty())
	{
		LatteQueryObjectGL* queryObject = list_queryCacheOcclusion.front();
		list_queryCacheOcclusion.erase(list_queryCacheOcclusion.begin() + 0);
		queryObject->m_glTarget = GL_SAMPLES_PASSED;
		queryObject->queryEnded = false;
		queryObject->queryEventStart = 0;
		queryObject->queryEventEnd = 0;
		return queryObject;
	}
	// no query object available in cache, create new query
	LatteQueryObjectGL* queryObject = new LatteQueryObjectGL();
	glGenQueries(1, &queryObject->m_queryId);
	queryObject->m_glTarget = GL_SAMPLES_PASSED;
	queryObject->queryEnded = false;
	queryObject->index = 0;
	queryObject->queryEventStart = 0;
	queryObject->queryEventEnd = 0;
	catchOpenGLError();
	return queryObject;
}

void OpenGLRenderer::occlusionQuery_destroy(LatteQueryObject* queryObj)
{
	list_queryCacheOcclusion.emplace_back(static_cast<LatteQueryObjectGL*>(queryObj));
}

void OpenGLRenderer::occlusionQuery_flush()
{
	glFlush();
}

bool LatteQueryObjectGL::getResult(uint64& numSamplesPassed)
{
	GLint resultAvailable = 0;
	catchOpenGLError();
	glGetQueryObjectiv(this->m_queryId, GL_QUERY_RESULT_AVAILABLE, &resultAvailable);
	if (resultAvailable == 0)
		return false;
	catchOpenGLError();
	GLint64 queryResult = 0;
	glGetQueryObjecti64v(this->m_queryId, GL_QUERY_RESULT, &queryResult);
	numSamplesPassed = queryResult;
	return true;
}

void LatteQueryObjectGL::begin()
{
	catchOpenGLError();
	glBeginQueryIndexed(this->m_glTarget, this->index, this->m_queryId);
	catchOpenGLError();
}

void LatteQueryObjectGL::end()
{
	glEndQueryIndexed(this->m_glTarget, this->index);
	catchOpenGLError();
}