/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_common.h"
#include "iv/markdown/iv_markdown_parse.h"
#include "base/basic_types.h"

#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>
#include <QtCore/QStringList>

#include <algorithm>
#include <array>

namespace Iv::Markdown {

bool IsReadableLocalFile(const QFileInfo &info) {
	return info.exists() && info.isFile() && info.isReadable();
}

bool LooksLikeMarkdownFile(const QString &fileName, const QString &mimeType) {
	const auto suffixes = std::array{
		u".md"_q,
		u".markdown"_q,
	};
	auto suffixMatches = false;
	for (const auto &suffix : suffixes) {
		if (fileName.endsWith(suffix, Qt::CaseInsensitive)) {
			suffixMatches = true;
			break;
		}
	}
	if (!suffixMatches) {
		return false;
	}
	if (mimeType.isEmpty()) {
		return true;
	}

	const auto normalized = mimeType.trimmed().toLower();
	const auto acceptedMimeTypes = std::array{
		u"text/markdown"_q,
		u"text/x-markdown"_q,
		u"text/plain"_q,
	};
	for (const auto &accepted : acceptedMimeTypes) {
		if (normalized == accepted) {
			return true;
		}
	}
	return false;
}

namespace {

const auto kTextLikePreviewExtensions = std::array{
	u".txt"_q,
	u".log"_q,
	u".json"_q,
	u".csv"_q,
	u".tsv"_q,
	u".xml"_q,
	u".yml"_q,
	u".yaml"_q,
	u".ini"_q,
	u".conf"_q,
	u".toml"_q,
	u".py"_q,
	u".js"_q,
	u".ts"_q,
	u".jsx"_q,
	u".tsx"_q,
	u".cpp"_q,
	u".h"_q,
	u".hpp"_q,
	u".c"_q,
	u".cc"_q,
	u".cs"_q,
	u".java"_q,
	u".kt"_q,
	u".rs"_q,
	u".go"_q,
	u".sh"_q,
	u".bat"_q,
	u".ps1"_q,
	u".sql"_q,
	u".php"_q,
	u".rb"_q,
	u".swift"_q,
	u".html"_q,
	u".htm"_q,
	u".svg"_q,
	u".css"_q,
};

constexpr auto kCsvMaxRows = 1000;
constexpr auto kCsvMaxColumns = 40;

[[nodiscard]] QString TextLikeSuffix(const QString &fileName) {
	for (const auto &suffix : kTextLikePreviewExtensions) {
		if (fileName.endsWith(suffix, Qt::CaseInsensitive)) {
			return suffix;
		}
	}
	return QString();
}

[[nodiscard]] QString EscapedTableCell(QString cell) {
	cell = cell.replace(QChar('\\'), u"\\\\"_q).replace(QChar('|'), u"\\|"_q);
	return cell.replace(QChar('\r'), QChar(' ')).replace(QChar('\n'), QChar(' '));
}

[[nodiscard]] QString CsvToMarkdownTable(
		const QString &source,
		QChar separator) {
	if (separator.isNull()) {
		// Count candidates outside quoted cells, like the mobile viewer does.
		auto counts = std::array<int, 4>{};
		auto inQuotes = false;
		for (const auto &ch : source) {
			if (ch == '"') {
				inQuotes = !inQuotes;
				continue;
			}
			auto index = -1;
			if (ch == ',') {
				index = 0;
			} else if (ch == ';') {
				index = 1;
			} else if (ch == '\t') {
				index = 2;
			} else if (ch == '|') {
				index = 3;
			}
			if (index >= 0 && !inQuotes) {
				++counts[index];
			}
		}
		auto best = 0;
		for (auto i = 1; i != 4; ++i) {
			if (counts[i] > counts[best]) {
				best = i;
			}
		}
		separator = (counts[best] > 0)
			? QChar(u",;\t|"_q[best])
			: QChar();
	}
	if (separator.isNull()) {
		return QString();
	}

	auto rows = std::vector<QStringList>();
	auto cells = QStringList();
	auto cell = QString();
	auto inQuotes = false;
	auto wasQuoted = false;
	auto truncated = false;
	auto columns = 0;
	const auto finishCell = [&] {
		if (cells.size() < kCsvMaxColumns) {
			cells.push_back(wasQuoted ? cell : cell.trimmed());
		}
		cell.clear();
		wasQuoted = false;
	};
	const auto finishRow = [&] {
		columns = std::max(columns, int(cells.size()));
		if (rows.size() == kCsvMaxRows) {
			// Stop early: a full 4 MB CSV would otherwise all be parsed.
			truncated = true;
			return;
		}
		rows.push_back(std::move(cells));
		cells = QStringList();
	};
	for (auto i = qsizetype(0); !truncated && i < source.size(); ++i) {
		const auto ch = source[i];
		if (inQuotes) {
			if (ch == '"') {
				if (i + 1 < source.size() && source[i + 1] == '"') {
					cell += '"';
					++i;
				} else {
					inQuotes = false;
				}
			} else {
				cell += ch;
			}
		} else if (ch == '"' && !wasQuoted && cell.trimmed().isEmpty()) {
			inQuotes = true;
			wasQuoted = true;
		} else if (ch == separator) {
			finishCell();
		} else if (ch == '\n' || ch == '\r') {
			if (ch == '\r' && i + 1 < source.size() && source[i + 1] == '\n') {
				++i;
			}
			if (!cells.isEmpty() || !wasQuoted || !cell.trimmed().isEmpty()) {
				finishCell();
				finishRow();
			}
		} else {
			cell += ch;
		}
	}
	if (!truncated
		&& (!cells.isEmpty() || !wasQuoted || !cell.trimmed().isEmpty())) {
		finishCell();
		finishRow();
	}
	if (columns < 2) {
		// A single column is cheaper to read as a fenced block.
		return QString();
	}
	auto out = QString();
	const auto emitRow = [&](const QStringList &row) {
		out += u"| "_q;
		for (auto i = 0; i != columns; ++i) {
			if (i) {
				out += u" | "_q;
			}
			out += EscapedTableCell(i < row.size() ? row[i] : QString());
		}
		out += u" |\n"_q;
	};
	emitRow(rows.front());
	out += u"|"_q;
	for (auto i = 0; i != columns; ++i) {
		out += u" --- |"_q;
	}
	out += QChar('\n');
	for (auto i = rows.begin() + 1; i != rows.end(); ++i) {
		emitRow(*i);
	}
	if (truncated) {
		out += u"\n*… truncated after %1 rows*\n"_q.arg(kCsvMaxRows);
	}
	return out;
}

[[nodiscard]] int PureFenceRun(const QString &line, QChar ch) {
	auto run = 0;
	while (run < line.size() && line[run] == ch) {
		++run;
	}
	if (run == 0) {
		return 0;
	}
	for (auto i = run; i < line.size(); ++i) {
		if (!line[i].isSpace()) {
			return 0;
		}
	}
	return run;
}

[[nodiscard]] QString FenceCode(const QString &text, const QString &language) {
	// A fence closes only on a pure run of its own character at least as
	// long as the opener, so pick the char with the smaller pure-run max.
	auto backticks = 0;
	auto tildes = 0;
	for (const auto &line : text.split(QChar('\n'))) {
		backticks = std::max(backticks, PureFenceRun(line, QChar(0x60)));
		tildes = std::max(tildes, PureFenceRun(line, QChar(0x7E)));
	}
	const auto useBackticks = (backticks <= tildes);
	const auto run = useBackticks ? backticks : tildes;
	const auto marker = QString(
		std::max(run + 1, 3),
		useBackticks ? QChar(0x60) : QChar(0x7E));
	return marker + language + QChar('\n')
		+ text
		+ (text.endsWith(QChar('\n')) ? QString() : QString(QChar('\n')))
		+ marker + QChar('\n');
}

[[nodiscard]] QString PrettyJsonOrEmpty(QString text) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError) {
		return QString();
	}
	text = QString::fromUtf8(document.toJson(QJsonDocument::Indented));
	return (text.size() > ParseLimitsForIv().maxSourceBytes)
		? QString()
		: text;
}

} // namespace

bool IsTextLikePreviewFile(const QString &fileName) {
	return !TextLikeSuffix(fileName).isEmpty();
}

QString ConvertTextLikePreviewSource(
		const QString &fileName,
		const QString &source) {
	const auto suffix = TextLikeSuffix(fileName);
	if (suffix == u".csv"_q || suffix == u".tsv"_q) {
		auto table = CsvToMarkdownTable(
			source,
			(suffix == u".tsv"_q) ? QChar('\t') : QChar());
		if (!table.isEmpty()) {
			return table;
		}
	} else if (suffix == u".json"_q) {
		if (auto pretty = PrettyJsonOrEmpty(source); !pretty.isEmpty()) {
			return FenceCode(pretty, u"json"_q);
		}
	}
	return FenceCode(source, suffix.mid(1));
}

} // namespace Iv::Markdown
