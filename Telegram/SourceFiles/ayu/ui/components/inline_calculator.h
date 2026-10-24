// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ui/rp_widget.h"

namespace Ayu {

[[nodiscard]] QString FormatMathResult(double value);

// Evaluates a math expression like "2+3*(4-1)^2".
// Supports + - * / % ^, parentheses, unary +/-,
// functions sqrt abs sin cos tan log ln round floor ceil,
// constants pi and e.
[[nodiscard]] bool EvaluateMathExpression(
	const QString &expression,
	double &result);

// Floating hint panel above the message input field,
// shown while the field text looks like "=<expression>".
class InlineCalculatorPanel final : public Ui::RpWidget {
public:
	explicit InlineCalculatorPanel(not_null<Ui::RpWidget*> parent);

	void updateResult(const QString &expression, const QString &result);
	[[nodiscard]] rpl::producer<QString> applyRequests() const;
	[[nodiscard]] bool active() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	QString _expression;
	QString _result;
	bool _pressed = false;
	rpl::event_stream<QString> _applyRequests;

};

} // namespace Ayu
