#pragma once
#include "Common/precompiled.h"

#include <string>
#include <queue>
#include <stack>
#include <unordered_map>
#include <memory>
#include <charconv>

#include "boost/functional/hash_fwd.hpp"
#include <fmt/format.h>

#ifdef __clang__
#include "Common/unix/fast_float.h"
 #define _EP_FROM_CHARS_DBL(...) _convFastFloatResult(fast_float::from_chars(__VA_ARGS__))

inline std::from_chars_result _convFastFloatResult(fast_float::from_chars_result r)
{
	std::from_chars_result nr;
	nr.ptr = r.ptr;
	nr.ec = r.ec;
	return nr;
}
#else
 #define _EP_FROM_CHARS_DBL(...) std::from_chars(__VA_ARGS__)
#endif

template<class TType = double>
class TExpressionParser
{
	// starting with Cemu 2.7 reloc modifiers (like @ha or @l) have been changed to match LLVM/GAS behavior
	// they may only be used once in an expression and get applied to the final result of the expression, regardless of their location
	// E.g. "var@ha + 0x20" was intepreted as ha(var) + 0x20 before, now it's interpreted as ha(var+0x20)
	enum class RelocModifier
	{
		None = 0,
		High, // @hi @h
		HighArithmetic, // @ha
		Low, // @lo @l
	};

public:
	static_assert(std::is_arithmetic_v<TType>);
	using ConstantCallback_t = TType(*)(std::string_view var_name);
	using FunctionCallback_t = TType(*)(std::string_view var_name, TType parameter);

	static TType ExpressionFuncHA(TType input)
	{
		uint32 addr = (uint32)input;
		addr = (((addr >> 16) + ((addr & 0x8000) ? 1 : 0)) & 0xffff);
		return (TType)addr;
	}

	static TType ExpressionFuncHI(TType input)
	{
		uint32 addr = (uint32)input;
		addr = (addr >> 16) & 0xffff;
		return (TType)addr;
	}

	static TType ExpressionFuncLO(TType input)
	{
		uint32 addr = (uint32)input;
		addr &= 0xffff;
		return (TType)addr;
	}

	template<typename T>
	T Evaluate(std::string_view expression) const
	{
		static_assert(std::is_arithmetic_v<T>, "type T must be an arithmetic type");
		return (T)Evaluate(expression);
	}

	[[nodiscard]] TType Evaluate(std::string_view expression) const
	{
		std::queue<std::shared_ptr<TokenBase>> output;
		std::stack<std::shared_ptr<TokenBase>> operators;
		RelocModifier relocModifier = RelocModifier::None;

		if (expression.empty())
		{
			throw std::runtime_error(fmt::format("empty expression is not allowed"));
		}

		bool last_operator_token = true;

		// tokenize and apply shunting-yard
		for (size_t i = 0; i < expression.size(); )
		{
			const char c = expression[i];

			// skip whitespaces
			if (isblank(c))
			{
				++i;
				continue;
			}

			// extract numbers
			if (isdigit(c) || (last_operator_token && (c == '-' || c == '+')))
			{
				const std::string_view view(expression.data() + i, expression.size() - i);
				size_t offset = 0;
				auto converted = (TType)ConvertString(view, &offset);
				output.emplace(std::make_shared<TokenNumber>(converted));
				i += offset;
				last_operator_token = false;
				// check for relocation modifier suffix (e.g. 74@ha)
				i += ParseRelocModifier(expression.substr(i), relocModifier, false); // can throw
				continue;
			}
			// check for variables
			if (isalpha(c) || c == '_' || c == '$')
			{
				size_t j;
				for (j = i + 1; j < expression.size(); ++j)
				{
					const char v = expression[j];
					if (!isalpha(v) && !isdigit(v) && v != '_' && v != '@' && v != '.')
					{
						break;
					}
				}

				const size_t len = j - i;
				std::string_view view = expression.substr(i, len);

				// check for relocation modifier
				if (auto suffixPos = view.find_last_of('@'); suffixPos != std::string::npos)
				{
					sint32 modifierLength = ParseRelocModifier(view.substr(suffixPos), relocModifier, true);
					view.remove_suffix(modifierLength);
				}
				// check for function
				if (m_function_callback)
				{
					// todo skip whitespaces
					if (j < expression.size() && expression[j] == '(')
					{
						operators.emplace(std::make_shared<TokenUnaryOperator>(TokenUnaryOperator::kFunction, view));
						operators.emplace(std::make_shared<TokenParenthese>());
						i += len + 1;

						last_operator_token = true;
						continue;
					}
				}

				TType value;
				const auto it = m_constants.find(std::string{ view });
				if (it == m_constants.cend())
				{
					if (m_constant_callback == nullptr)
						throw std::runtime_error(fmt::format("unknown constant found \"{}\" in expression: {}", view, expression));

					value = m_constant_callback(view);
				}
				else
					value = it->second;

				output.emplace(std::make_shared<TokenNumber>(value));
				i += len;

				last_operator_token = false;
				continue;
			}

			// parenthese
			if (c == '(')
			{
				operators.emplace(std::make_shared<TokenParenthese>());
				++i;

				last_operator_token = true;
				continue;
			}

			if (c == ')')
			{
				bool match = false;
				while (!operators.empty())
				{
					const auto op = std::dynamic_pointer_cast<TokenParenthese>(operators.top());
					if (!op)
					{
						output.emplace(operators.top());
						operators.pop();
						continue;
					}

					operators.pop();
					match = true;
					break;
				}

				if (!match && operators.empty())
				{
					throw std::runtime_error(fmt::format("parentheses mismatch: {}", expression));
				}

				++i;
				continue;
			}

			// supported operations
			std::shared_ptr<TokenOperator> operator_token;
			switch (c)
			{
			case '+':
				operator_token = std::make_shared<TokenOperator>(TokenOperator::kAddition, 2);
				break;
			case '-':
				operator_token = std::make_shared<TokenOperator>(TokenOperator::kSubtraction, 2);
				break;
			case '*':
				operator_token = std::make_shared<TokenOperator>(TokenOperator::kMultiplication, 3);
				break;
			case '/':
				operator_token = std::make_shared<TokenOperator>(TokenOperator::kDivision, 3);
				break;
			case '^':
				operator_token = std::make_shared<TokenOperator>(TokenOperator::kPow, 4, true);
				break;
			case '%':
				operator_token = std::make_shared<TokenOperator>(TokenOperator::kModulo, 3);
				break;
			case '=':
				if ((i + 1) < expression.size() && expression[i + 1] == '=')
				{
					operator_token = std::make_shared<TokenOperator>(TokenOperator::kEqual, 1);
					++i;
				}
				break;
			case '!':
				if ((i + 1) < expression.size() && expression[i + 1] == '=')
				{
					operator_token = std::make_shared<TokenOperator>(TokenOperator::kNotEqual, 1);
					++i;
				}
				break;
			case '<':
				if ((i + 1) < expression.size() && expression[i + 1] == '=')
				{
					operator_token = std::make_shared<TokenOperator>(TokenOperator::kLessOrEqual, 1);
					++i;
				}
				else
				{
					operator_token = std::make_shared<TokenOperator>(TokenOperator::kLessThan, 1);
				}
				break;
			case '>':
				if ((i + 1) < expression.size() && expression[i + 1] == '=')
				{
					operator_token = std::make_shared<TokenOperator>(TokenOperator::kGreaterOrEqual, 1);
					++i;
				}
				else
				{
					operator_token = std::make_shared<TokenOperator>(TokenOperator::kGreaterThan, 1);
				}
				break;
			}

			if (!operator_token)
				throw std::runtime_error(fmt::format("invalid operator found in expression: {}", expression));

			while (!operators.empty())
			{
				const auto op = std::dynamic_pointer_cast<TokenOperator>(operators.top());
				if (op && ((!operator_token->right_ass && operator_token->prec <= op->prec) || (operator_token->right_ass && operator_token->prec < op->prec)))
				{
					output.emplace(operators.top());
					operators.pop();
					continue;
				}

				break;
			}

			operators.emplace(operator_token);
			++i;
			last_operator_token = true;
		}

		while (!operators.empty())
		{
			const auto parenthese = std::dynamic_pointer_cast<TokenParenthese>(operators.top());
			if (parenthese)
			{
				throw std::runtime_error(fmt::format("parentheses mismatch in expression: {}", expression));
			}

			output.push(operators.top());
			operators.pop();
		}

		// evaluate tokens in output queue
		std::stack<TType> evaluation;
		while (!output.empty())
		{
			const auto number = std::dynamic_pointer_cast<TokenNumber>(output.front());
			if (number)
			{
				evaluation.emplace(number->number);
				output.pop();
				continue;
			}

			if (const auto unop = std::dynamic_pointer_cast<TokenUnaryOperator>(output.front()))
			{
				if (evaluation.size() < 1)
					throw std::runtime_error("not enough parameters for equation");

				const auto parameter = evaluation.top();
				evaluation.pop();

				switch (unop->op)
				{
				case TokenUnaryOperator::kFunction:
					evaluation.emplace(m_function_callback(unop->symbol, parameter));
					break;
				default:
					throw std::runtime_error("unsupported operation constant");
				}

				output.pop();
				continue;
			}
			else if (const auto op = std::dynamic_pointer_cast<TokenOperator>(output.front()))
			{
				if (evaluation.size() < 2)
					throw std::runtime_error("not enough parameters for equation");

				const auto rhs = evaluation.top();
				evaluation.pop();

				const auto lhs = evaluation.top();
				evaluation.pop();

				switch (op->op)
				{
				case TokenOperator::kAddition:
					evaluation.emplace(lhs + rhs);
					break;
				case TokenOperator::kSubtraction:
					evaluation.emplace(lhs - rhs);
					break;
				case TokenOperator::kMultiplication:
					evaluation.emplace(lhs * rhs);
					break;
				case TokenOperator::kDivision:
					if (rhs == 0.0) evaluation.emplace((TType)0.0);
					else evaluation.emplace(lhs / rhs);
					break;
				case TokenOperator::kPow:
					evaluation.emplace((TType)std::pow(lhs, rhs));
					break;
				case TokenOperator::kModulo:
					if (std::round(rhs) == 0.0)
						evaluation.emplace((TType)0.0);
					else
						evaluation.emplace((TType)((sint64)std::round(lhs) % (sint64)std::round(rhs)));
					break;
				case TokenOperator::kEqual:
					if constexpr (std::is_floating_point_v<TType>)
						evaluation.emplace((TType)(std::abs(lhs - rhs) <= 0.0000000001 ? 1 : 0));
					else
						evaluation.emplace(lhs == rhs ? 1 : 0);
					break;
				case TokenOperator::kNotEqual:
					if constexpr (std::is_floating_point_v<TType>)
						evaluation.emplace((TType)(std::abs(lhs - rhs) > 0.0000000001 ? 1 : 0));
					else
						evaluation.emplace(lhs != rhs ? 1 : 0);
					break;
				case TokenOperator::kLessThan:
					evaluation.emplace((TType)(lhs < rhs ? 1 : 0));
					break;
				case TokenOperator::kLessOrEqual:
					evaluation.emplace((TType)(lhs <= rhs ? 1 : 0));
					break;
				case TokenOperator::kGreaterThan:
					evaluation.emplace((TType)(lhs > rhs ? 1 : 0));
					break;
				case TokenOperator::kGreaterOrEqual:
					evaluation.emplace((TType)(lhs >= rhs ? 1 : 0));
					break;
				default:
					throw std::runtime_error("unsupported operation constant");
				}

				output.pop();
				continue;
			}
		}

		if (relocModifier == RelocModifier::High)
			evaluation.top() = ExpressionFuncHI(evaluation.top());
		else if (relocModifier == RelocModifier::HighArithmetic)
			evaluation.top() = ExpressionFuncHA(evaluation.top());
		else if (relocModifier == RelocModifier::Low)
			evaluation.top() = ExpressionFuncLO(evaluation.top());

		return evaluation.top();
	}

	[[nodiscard]] bool IsValidExpression(std::string_view expression) const
	{
		try
		{
			TExpressionParser<TType> ep;
			ep.AddConstantCallback(
				[](std::string_view sv) -> TType
				{
					return (TType)1;
				});
			static_cast<void>(ep.Evaluate(expression));
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	[[nodiscard]] bool IsConstantExpression(std::string_view expression) const
	{
		try
		{
			static_cast<void>(this->Evaluate(expression));
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void AddConstant(std::string_view name, TType value)
	{
		m_constants[std::string(name)] = value;
	}

	// only adds constant if not added yet
	void TryAddConstant(std::string_view name, TType value)
	{
		m_constants.try_emplace(std::string{ name }, value);
	}

	void AddConstant(std::string_view name, std::string_view value)
	{
		AddConstant(name, ConvertString(value));
	}

	void AddConstantCallback(ConstantCallback_t callback)
	{
		m_constant_callback = callback;
	}

	void SetFunctionCallback(FunctionCallback_t callback)
	{
		m_function_callback = callback;
	}
private:
	std::unordered_map<std::string, TType> m_constants;
	ConstantCallback_t m_constant_callback = nullptr;
	FunctionCallback_t m_function_callback = nullptr;

	static bool _isNumberWithDecimalPoint(std::string_view str)
	{
		return str.find('.') != std::string_view::npos;
	}

	// update current relocation modifier for the expression
	void SetRelocationModifier(RelocModifier& relocModifier, RelocModifier newModifier) const
	{
		if (relocModifier == newModifier)
			return;
		// catch mismatching relocation modifiers (e.g. sym@ha + sym2@lo)
		if (relocModifier != RelocModifier::None)
			throw std::runtime_error(fmt::format("Mismatching relocation modifiers (suffix @..) in expression"));
		relocModifier = newModifier;
	}

	// parse modifiers like @ha, @l. Throws on parse error (unless ignored). Returns length of parsed suffix including @ symbol
	sint32 ParseRelocModifier(std::string_view str, RelocModifier& relocModifier, bool ignoreParseError) const
	{
		auto origStr = str;
		if (str.empty() || str[0] != '@')
			return 0;
		// skip the @
		str.remove_prefix(1);
		if (str.empty())
			return 0;
		char c0 = std::tolower(str[0]);
		// check for two character modifiers: lo, ha, hi
		if (str.size() >= 2)
		{
			char c1 = std::tolower(str[1]);
			if (c0 == 'h' && c1 == 'a')
			{
				SetRelocationModifier(relocModifier, RelocModifier::HighArithmetic);
				return 3;
			}
			else if (c0 == 'h' && c1 == 'i')
			{
				SetRelocationModifier(relocModifier, RelocModifier::High);
				return 3;
			}
			else if (c0 == 'l' && c1 == 'o')
			{
				SetRelocationModifier(relocModifier, RelocModifier::Low);
				return 3;
			}
		}
		// check for single character modifiers
		if (c0 == 'h')
		{
			SetRelocationModifier(relocModifier, RelocModifier::High);
			return 2;
		}
		else if (c0 == 'l')
		{
			SetRelocationModifier(relocModifier, RelocModifier::Low);
			return 2;
		}
		if (!ignoreParseError)
			throw std::runtime_error(fmt::format("Unknown relocation modifier (only @lo, @hi, @ha, @l, @h are supported) at: {}", origStr));
		return 0;
	}

	double ConvertString(std::string_view str, size_t* index_after = nullptr) const
	{
		const char* strInitial = str.data();
		if (str.empty())
			throw std::runtime_error("can't parse empty number");

		double result;
		std::from_chars_result chars_result;

		const bool is_negative = str[0] == '-';
		if (is_negative || str[0] == '+')
			str = str.substr(1);

		if (str.size() >= 3 && str[0] == '0' && tolower(str[1]) == 'x')
		{
			str = str.substr(2);
			// find end of number
			const auto end = std::find_if_not(str.cbegin(), str.cend(), isxdigit);

			int64_t tmp;
			chars_result = std::from_chars(str.data(), str.data() + std::distance(str.cbegin(), end), tmp, 16);
			result = (double)tmp;
		}
		// a number prefixed with 0 and no decimal point is considered a hex number
		else if (str.size() >= 2 && str[0] == '0' && isxdigit(str[1]) && !_isNumberWithDecimalPoint(str))
		{
			str = str.substr(1);
			// find end of number
			const auto end = std::find_if_not(str.cbegin(), str.cend(), isxdigit);

			int64_t tmp;
			chars_result = std::from_chars(str.data(), str.data() + std::distance(str.cbegin(), end), tmp, 16);
			result = (double)tmp;
		}
		else
		{
			if (str[0] == '+')
				str = str.substr(1);

			bool has_point = false;
			bool has_exponent = false;
			const auto end = std::find_if_not(str.cbegin(), str.cend(),
				[&has_point, &has_exponent](char c) -> bool
				{
					if (isdigit(c))
						return true;

					if (c == '.' && !has_point)
					{
						has_point = true;
						return true;
					}

					if (tolower(c) == 'e' && !has_exponent)
					{
						has_exponent = true;
						// TODO: after exponent might be + and - allowed?
						return true;
					}
					return false;
				});
			chars_result = _EP_FROM_CHARS_DBL(str.data(), str.data() + std::distance(str.cbegin(), end), result);
		}

		if (chars_result.ec == std::errc())
		{
			if (index_after)
				*index_after = chars_result.ptr - strInitial;

			if (is_negative)
				result *= -1.0;

			return result;
		}
		else
			throw std::runtime_error(fmt::format("can't parse number: {}", str));
	}

	struct TokenBase
	{
		virtual ~TokenBase() = default;
	};

	struct TokenUnaryOperator : TokenBase
	{
		enum Operator
		{
			kFunction,
		};

		TokenUnaryOperator(Operator op) : op(op) {}
		TokenUnaryOperator(Operator op, std::string_view symbol) : op(op), symbol(symbol) {}

		Operator op;
		std::string_view symbol;
	};

	struct TokenOperator : TokenBase
	{
		enum Operator
		{
			kAddition,
			kSubtraction,
			kMultiplication,
			kDivision,
			kPow,
			kModulo,
			kEqual,
			kNotEqual,
			kLessThan,
			kLessOrEqual,
			kGreaterThan,
			kGreaterOrEqual,
		};

		TokenOperator(Operator op, int prec, bool right_ass = false) : op(op), prec(prec), right_ass(right_ass) {}
		Operator op;
		int prec;
		bool right_ass;
	};

	struct TokenParenthese : TokenBase {};

	struct TokenNumber : TokenBase
	{
		TokenNumber(TType number) : number(number) {}
		TType number;
	};
};

class ExpressionParser : public TExpressionParser<double> {};

void ExpressionParser_test();

