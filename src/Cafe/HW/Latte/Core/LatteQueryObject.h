#pragma once

class LatteQueryObject
{
public:
	virtual bool getResult(uint64& numSamplesPassed) = 0;
	virtual void begin() = 0;
	virtual void end() = 0;

	uint32 index{};
	bool queryEnded{}; // set to true once the query is ended, but the result may not be available for some time after this
	uint64 queryEventStart{};
	uint64 queryEventEnd{};
};