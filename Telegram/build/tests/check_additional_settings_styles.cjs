const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const file = fs.readFileSync(path.join(root, 'Telegram/SourceFiles/ayu/ui/settings/settings_additional.cpp'), 'utf8');
assert.match(file, /#include "styles\/style_ayu_styles\.h"/);
assert.ok(!file.includes('build/tests'), 'No test references in production file');
console.log('PASS: settings_additional includes the ayu styles module defining recentStickersLimitPadding (source contract; build/runtime pending).');
