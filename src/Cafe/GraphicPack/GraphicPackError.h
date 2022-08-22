#pragma once

class PatchErrorHandler
{
public:
	enum class STAGE
	{
		PARSER,
		APPLY,
	};


	void setCurrentGraphicPack(class GraphicPack2* gp)
	{
		m_gp = gp;
	}

	void setStage(STAGE s)
	{
		m_stage = s;
	}

	void printError(class PatchGroup* patchGroup, sint32 lineNumber, std::string_view errorMsg);
	void showStageErrorMessageBox();

	bool hasError() const { return m_anyErrorTriggered; };

	class GraphicPack2* m_gp{};
	bool m_anyErrorTriggered{};
	STAGE m_stage{ STAGE::PARSER };
	std::vector<std::string> errorMessages; // if patch logging is enabled also remember error msgs for the message box
};
