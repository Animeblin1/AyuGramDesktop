// Link with the project's inline_calculator.cpp and its normal Qt/UI dependencies.
#include "ayu/ui/components/inline_calculator.h"

#include <cmath>
#include <cstdlib>

int main() {
	const auto check = [](const char16_t *expression, double expected) {
		auto result = 0.;
		if (!Ayu::EvaluateMathExpression(QString(expression), result)
			|| std::abs(result - expected) > 1e-9) {
			std::abort();
		}
	};
	check(u"2+3*(4-1)^2", 29.);
	check(u"-2^2", -4.);
	check(u"(-2)^2", 4.);
	check(u"2^-2", .25);
	check(u"2^3^2", 512.);
	check(u"sqrt(9)+abs(-4)", 7.);
	check(u"1,5+2.5", 4.);
	for (const auto &expression : {
		QString(u"1/0"),
		QString(u"sqrt(-1)"),
		QString(u"2+"),
		QString(u"1 garbage"),
		QString(20000, QChar('(')) + u"1" + QString(20000, QChar(')')),
		QString(20000, QChar('-')) + u"1",
		QString(u"1^").repeated(20000) + u"1",
	}) {
		auto result = 123.;
		if (Ayu::EvaluateMathExpression(expression, result) || result != 123.) {
			std::abort();
		}
	}
}
