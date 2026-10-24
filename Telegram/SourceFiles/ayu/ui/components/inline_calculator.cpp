// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/components/inline_calculator.h"

#include "ui/painter.h"
#include "styles/style_basic.h"

#include <cmath>
#include <QMouseEvent>

namespace Ayu {
namespace {

constexpr auto kPi = 3.14159265358979323846;
constexpr auto kE = 2.71828182845904523536;

class Parser {
public:
	explicit Parser(const QString &text)
	: _text(text)
	, _p(text.constData())
	, _end(text.constData() + text.size()) {
	}

	bool evaluate(double &out) {
		return parseExpression(out) && atEnd();
	}

private:
	const QString &_text;
	const QChar *_p = nullptr;
	const QChar *_end = nullptr;

	void skipSpaces() {
		while (_p != _end && _p->isSpace()) {
			++_p;
		}
	}

	[[nodiscard]] bool atEnd() const {
		skipSpacesConst();
		return _p == _end;
	}

	void skipSpacesConst() const {
		// const view only, safe to ignore constness for skipping
		const_cast<Parser*>(this)->skipSpaces();
	}

	bool match(QChar ch) {
		skipSpaces();
		if (_p != _end && *_p == ch) {
			++_p;
			return true;
		}
		return false;
	}

	bool peek(QChar ch) {
		skipSpaces();
		return _p != _end && *_p == ch;
	}

	bool parseExpression(double &out) {
		if (!parseTerm(out)) {
			return false;
		}
		for (;;) {
			if (peek('+')) {
				++_p;
				double rhs = 0.;
				if (!parseTerm(rhs)) {
					return false;
				}
				out += rhs;
			} else if (peek('-')) {
				++_p;
				double rhs = 0.;
				if (!parseTerm(rhs)) {
					return false;
				}
				out -= rhs;
			} else {
				return true;
			}
		}
	}

	bool parseTerm(double &out) {
		if (!parsePower(out)) {
			return false;
		}
		for (;;) {
			if (peek('*')) {
				++_p;
				double rhs = 0.;
				if (!parsePower(rhs)) {
					return false;
				}
				out *= rhs;
			} else if (peek('/')) {
				++_p;
				double rhs = 0.;
				if (!parsePower(rhs)) {
					return false;
				}
				if (rhs == 0.) {
					return false;
				}
				out /= rhs;
			} else if (peek('%')) {
				++_p;
				double rhs = 0.;
				if (!parsePower(rhs)) {
					return false;
				}
				if (rhs == 0.) {
					return false;
				}
				out = std::fmod(out, rhs);
			} else {
				return true;
			}
		}
	}

	bool parsePower(double &out) {
		if (!parseUnary(out)) {
			return false;
		}
		if (peek('^')) {
			++_p;
			double rhs = 0.;
			if (!parsePower(rhs)) { // right associative
				return false;
			}
			out = std::pow(out, rhs);
			if (std::isnan(out) || std::isinf(out)) {
				return false;
			}
		}
		return true;
	}

	bool parseUnary(double &out) {
		if (match('-')) {
			double inner = 0.;
			if (!parseUnary(inner)) {
				return false;
			}
			out = -inner;
			return true;
		} else if (match('+')) {
			return parseUnary(out);
		}
		return parsePrimary(out);
	}

	bool parsePrimary(double &out) {
		skipSpaces();
		if (_p == _end) {
			return false;
		}
		if (*_p == '(') {
			++_p;
			if (!parseExpression(out)) {
				return false;
			}
			return match(')');
		}
		if (_p->isLetter() || *_p == '_') {
			return parseIdentifier(out);
		}
		return parseNumber(out);
	}

	bool parseIdentifier(double &out) {
		const auto start = _p;
		while (_p != _end && (_p->isLetterOrNumber() || *_p == '_')) {
			++_p;
		}
		const auto name = QString(start, _p - start);
		const auto equals = [&](const char16_t *word) {
			return name.compare(QString(word), Qt::CaseInsensitive) == 0;
		};
		if (equals(u"pi")) {
			out = kPi;
			return true;
		} else if (equals(u"e")) {
			out = kE;
			return true;
		}
		if (!match('(')) {
			return false;
		}
		double arg = 0.;
		if (!parseExpression(arg)) {
			return false;
		}
		if (!match(')')) {
			return false;
		}
		return applyFunction(name, arg, out);
	}

	static bool applyFunction(
			const QString &name,
			double arg,
			double &out) {
		const auto equals = [&](const char16_t *word) {
			return name.compare(QString(word), Qt::CaseInsensitive) == 0;
		};
		double next = 0;
		if (equals(u"sqrt")) {
			next = std::sqrt(arg);
		} else if (equals(u"abs")) {
			next = std::abs(arg);
		} else if (equals(u"sin")) {
			next = std::sin(arg);
		} else if (equals(u"cos")) {
			next = std::cos(arg);
		} else if (equals(u"tan")) {
			next = std::tan(arg);
		} else if (equals(u"ln") || equals(u"log")) {
			next = std::log(arg);
		} else if (equals(u"log10")) {
			next = std::log10(arg);
		} else if (equals(u"round")) {
			next = std::round(arg);
		} else if (equals(u"floor")) {
			next = std::floor(arg);
		} else if (equals(u"ceil")) {
			next = std::ceil(arg);
		} else {
			return false;
		}
		if (std::isnan(next) || std::isinf(next)) {
			return false;
		}
		out = next;
		return true;
	}

	bool parseNumber(double &out) {
		skipSpaces();
		const auto start = _p;
		while (_p != _end && _p->isDigit()) {
			++_p;
		}
		if (_p != _end && *_p == '.') {
			++_p;
			while (_p != _end && _p->isDigit()) {
				++_p;
			}
		}
		if (_p == start) {
			return false;
		}
		bool ok = false;
		out = QStringView(start, _p - start).toDouble(&ok);
		return ok;
	}

};

} // namespace

QString FormatMathResult(double value) {
	const auto rounded = std::round(value);
	if (std::abs(value - rounded) < 1e-9 && std::abs(value) < 1e15) {
		return QString::number(rounded, 'f', 0);
	}
	return QString::number(value, 'g', 12);
}

bool EvaluateMathExpression(
		const QString &expression,
		double &result) {
	auto text = expression;
	text.replace(',', '.');
	auto parser = Parser(text);
	double value = 0.;
	if (!parser.evaluate(value)) {
		return false;
	}
	if (std::isnan(value) || std::isinf(value)) {
		return false;
	}
	result = value;
	return true;
}

InlineCalculatorPanel::InlineCalculatorPanel(
	not_null<Ui::RpWidget*> parent)
: Ui::RpWidget(parent) {
	hide();
}

void InlineCalculatorPanel::updateResult(
		const QString &expression,
		const QString &result) {
	const auto &exprFont = st::normalFont;
	const auto &resFont = st::semiboldFont;
	const auto pad = st::roundRadiusLarge;
	const auto sep = QStringLiteral(" = ");

	const auto available = parentWidget()->width()
		- 4 * pad
		- exprFont->width(sep)
		- resFont->width(result);
	auto expr = expression;
	if (exprFont->width(expr) > available && available > 0) {
		while (!expr.isEmpty()
			&& exprFont->width(expr + QStringLiteral("…")) > available) {
			expr.chop(1);
		}
		expr += QStringLiteral("…");
	}
	_expression = expr;
	_result = result;

	const auto w = pad * 2
		+ exprFont->width(_expression)
		+ exprFont->width(sep)
		+ resFont->width(_result);
	const auto h = 2 * pad + exprFont->height;
	resize(w, h);
	update();
}

rpl::producer<QString> InlineCalculatorPanel::applyRequests() const {
	return _applyRequests.events();
}

bool InlineCalculatorPanel::active() const {
	return isVisible();
}

void InlineCalculatorPanel::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	PainterHighQualityEnabler hq(p);

	const auto pad = st::roundRadiusLarge;
	const auto radius = pad;
	const auto inner = rect().marginsRemoved(QMargins(2, 2, 2, 2));
	p.setPen(QPen(st::windowBgRipple, st::lineWidth));
	p.setBrush(st::windowBg);
	p.drawRoundedRect(inner, radius, radius);

	const auto &exprFont = st::normalFont;
	const auto &resFont = st::semiboldFont;
	const auto sep = QStringLiteral(" = ");
	const auto textTop = (height() - exprFont->height) / 2
		+ exprFont->ascent;

	auto x = pad;
	p.setFont(exprFont);
	p.setPen(st::windowSubTextFg);
	p.drawText(x, textTop, _expression);

	x += exprFont->width(_expression);
	p.drawText(x, textTop, sep);
	x += exprFont->width(sep);

	p.setFont(resFont);
	p.setPen(st::windowActiveTextFg);
	p.drawText(x, textTop, _result);
}

void InlineCalculatorPanel::mousePressEvent(QMouseEvent *e) {
	_pressed = true;
	e->accept();
}

void InlineCalculatorPanel::mouseReleaseEvent(QMouseEvent *e) {
	const auto pressed = std::exchange(_pressed, false);
	if (pressed && rect().contains(e->pos())) {
		_applyRequests.fire_copy(_result);
	}
	e->accept();
}

} // namespace Ayu
