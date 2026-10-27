'use strict';
// Source-contract check (not a runtime test): Phase 1 document previews must
// keep routing local TEXT files through the markdown IV pipeline, converting
// them to markdown (csv/tsv tables, fenced code) before validation.

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const read = name => fs.readFileSync(path.join(root, name), 'utf8').replace(/\r\n/g, '\n');

const common = read('Telegram/SourceFiles/iv/markdown/iv_markdown_common.cpp');
const controller = read('Telegram/SourceFiles/iv/markdown/iv_markdown_controller.cpp');
const handoff = read('Telegram/SourceFiles/test/test_open_handoff.cpp');

// --- Extension acceptance: every Phase 1 format is registered, .md untouched.
const requiredExtensions = [
  '.txt', '.log', '.json', '.csv', '.tsv', '.xml', '.yml', '.yaml', '.ini',
  '.conf', '.toml', '.py', '.js', '.ts', '.jsx', '.tsx', '.cpp', '.h', '.hpp',
  '.c', '.cc', '.cs', '.java', '.kt', '.rs', '.go', '.sh', '.bat', '.ps1',
  '.sql', '.php', '.rb', '.swift', '.html', '.htm', '.svg', '.css',
];
for (const ext of requiredExtensions) {
  assert.ok(common.includes(`u"${ext}"_q`), `missing extension ${ext}`);
}
const mdFn = common.slice(
  common.indexOf('bool LooksLikeMarkdownFile('),
  common.indexOf('namespace {'));
assert.match(mdFn, /\.md"_q/);
assert.match(mdFn, /\.markdown"_q/);
assert.doesNotMatch(mdFn, /\.txt|\.json|\.csv|\.html/, 'markdown gate stays md-only');

// --- Controller: ReadLocalSource gates text-like files and rejects binaries.
const readLocal = controller.slice(
  controller.indexOf('[[nodiscard]] ReadSource ReadLocalSource('),
  controller.indexOf('[[nodiscard]] QString NormalizeFragmentId'));
assert.match(readLocal, /LooksLikeMarkdownFile\(name\)/);
assert.match(readLocal, /IsTextLikePreviewFile\(name\)/);
assert.match(readLocal, /info\.size\(\) > limits\.maxSourceBytes/);
assert.match(readLocal, /data\.left\(8 \* 1024\)\.indexOf\('\\0'\) >= 0/,
  'NUL probe over the first 8KB must reject probably-binary files');
assert.match(readLocal, /ValidateMarkdownSourceForIv\(\s*data,\s*ParseOptions\{ name \}\)/,
  'text-like sources decode through the markdown gate before conversion');
assert.match(readLocal, /ConvertTextLikePreviewSource\(/);

// --- Converter contract: csv/tsv -> quoted-cell-aware markdown table,
// json -> pretty-printed fenced block, everything else -> fenced code.
const converter = common.slice(
  common.indexOf('QString ConvertTextLikePreviewSource('),
  common.indexOf('} // namespace Iv::Markdown'));
assert.match(converter, /\.csv"_q \|\| suffix == u"\.tsv"_q/);
assert.match(converter, /CsvToMarkdownTable\(/);
assert.match(converter, /PrettyJsonOrEmpty\(source\)/);
assert.match(converter, /FenceCode\(source, suffix\.mid\(1\)\)/,
  'non-csv/tsv formats (incl. html/svg) render as fenced source, never raw');

const csv = common.slice(
  common.indexOf('QString CsvToMarkdownTable('),
  common.indexOf('[nodiscard]] QString FenceCode('));
assert.match(csv, /",;\\t\|"/, 'separator candidates, quote-aware');
assert.match(csv, /inQuotes/, 'quoted cells kept');
assert.match(csv, /!wasQuoted && cell\.trimmed\(\)\.isEmpty\(\)/,
  'a quote opens a quoted cell wherever the current cell is empty');
assert.match(csv, /cells\.size\(\) < kCsvMaxColumns/,
  'cells beyond the column cap dropped while parsing');
assert.match(csv, /rows\.size\(\) == kCsvMaxRows/,
  'row cap stops parsing early instead of storing everything');
assert.match(csv, /truncated/, 'truncation note emitted after the cap');
assert.match(csv, /columns < 2/, 'single-column files stay fenced code');
assert.match(csv, /kCsvMaxRows/, 'row cap applied');
assert.match(csv, /kCsvMaxColumns/, 'column cap applied');

const fence = common.slice(
  common.indexOf('QString FenceCode('),
  common.indexOf('QString PrettyJsonOrEmpty'));
assert.match(fence, /PureFenceRun/, 'pure-run scanner guards fence closure');
assert.match(fence, /std::max\(run \+ 1, 3\)/,
  'marker outgrows any pure fence run contained in the code');

const json = common.slice(
  common.indexOf('PrettyJsonOrEmpty'),
  common.indexOf('} // namespace Iv::Markdown'));
assert.match(json, /QJsonDocument::fromJson/, 'json pretty check present');

// Handoff test-agent prediction must cover the widened set.
assert.match(handoff, /IsTextLikePreviewFile|LooksLikeMarkdownFile/);

console.log('PASS: phase-1 doc preview source-contract check (not runtime).');
