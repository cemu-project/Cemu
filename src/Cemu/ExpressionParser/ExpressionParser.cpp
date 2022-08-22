#include "ExpressionParser.h"

template<typename T, typename TInternalType = double>
T _testEvaluateToType(const char* expr)
{
	TExpressionParser<TInternalType> ep;
	T result = (T)ep.Evaluate(expr);
	return result;
}

void ExpressionParser_test()
{
	ExpressionParser ep;
	cemu_assert_debug(_testEvaluateToType<sint64>("0x100005E0") == 0x100005E0);
	cemu_assert_debug(_testEvaluateToType<uint64>("0x10+0x20+0x40") == 0x70);
	cemu_assert_debug(_testEvaluateToType<sint64>("0+-3") == -3);
	cemu_assert_debug(_testEvaluateToType<sint64>("0x0-3") == -3);
	cemu_assert_debug(_testEvaluateToType<sint64>("01C+0x10") == 0x2C);
	cemu_assert_debug(_testEvaluateToType<sint64>("+0x10") == 0x10);
	cemu_assert_debug(_testEvaluateToType<sint64>("01C++0x10") == 0x2C);
	cemu_assert_debug(_testEvaluateToType<sint64>("01C+(+0x10)") == 0x2C);

	// arithmetic
	cemu_assert_debug(_testEvaluateToType<sint64>("(62156250 / 1000) * 30") == 1864687); // truncated 1864687.5

	// arithmetic with internal type sint64
	cemu_assert_debug(_testEvaluateToType<sint64, sint64>("(62156250 / 1000) * 30") == 1864680);

	// comparison operators
	cemu_assert_debug(_testEvaluateToType<sint64>("5 > 3") == 0x1);
	cemu_assert_debug(_testEvaluateToType<sint64>("5 < -10") == 0x0);
	cemu_assert_debug(_testEvaluateToType<float>("5 > 3") == 1.0f);
	cemu_assert_debug(_testEvaluateToType<float>("-5 > 3") == 0.0f);
	cemu_assert_debug(_testEvaluateToType<float>("5 < 10") == 1.0f);
	cemu_assert_debug(_testEvaluateToType<float>("5 <= 5") == 1.0f);
	cemu_assert_debug(_testEvaluateToType<float>("5 <= 4.999") == 0.0f);
	cemu_assert_debug(_testEvaluateToType<float>("5 <= (4+0.999)") == 0.0f);
	cemu_assert_debug(_testEvaluateToType<float>("5 >= 3") == 1.0f);
	cemu_assert_debug(_testEvaluateToType<float>("5 >= 10") == 0.0f);
	cemu_assert_debug(_testEvaluateToType<float>("10 >= 5") == 1.0f);

	// complex operations
	cemu_assert_debug(_testEvaluateToType<float>("5 > 4 > 3 > 2") == 0.0f); // this should evaluate the operations from left to right, (5 > 4) -> 0.0, (0.0 > 4) -> 0.0, (0.0 > 3) -> 0.0, (0.0 > 2) -> 0.0
	cemu_assert_debug(_testEvaluateToType<float>("5 > 4 > 3 > -2") == 1.0f); // this should evaluate the operations from left to right, (5 > 4) -> 0.0, (0.0 > 4) -> 0.0, (0.0 > 3) -> 0.0, (0.0 > -2) -> 1.0
	cemu_assert_debug(_testEvaluateToType<float>("(5 == 5) > (5 == 6)") == 1.0f);
}

// Cemuhook exports

DLLEXPORT ExpressionParser* ExpressionParser_Create()
{
	return new ExpressionParser();
}

DLLEXPORT ExpressionParser* ExpressionParser_CreateCopy(ExpressionParser* ep)
{
	return new ExpressionParser((ExpressionParser&)*ep);
}

DLLEXPORT void ExpressionParser_Delete(ExpressionParser* ep)
{
	delete ep;
}

DLLEXPORT void ExpressionParser_AddConstantDouble(ExpressionParser* ep, const char* name, double value)
{
	ep->AddConstant(name, value);
}

DLLEXPORT void ExpressionParser_AddConstantString(ExpressionParser* ep, const char* name, const char* value)
{
	ep->AddConstant(name, value);
}

DLLEXPORT bool ExpressionParser_EvaluateToDouble(ExpressionParser* ep, const char* expression, double* result)
{
	try
	{
		const double temp = ep->Evaluate(std::string(expression));
		if (result)
			*result = temp;

		return true;
	}
	catch (const std::exception& ex)
	{
		if( result )
			forceLog_printf("Unable to evaluate expression: %s", ex.what());
		return false;
	}
}