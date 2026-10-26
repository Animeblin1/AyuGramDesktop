'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const read = name => fs.readFileSync(path.join(root, name), 'utf8').replace(/\r\n/g, '\n');
const body = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, signature);
  const open = source.indexOf('{', start);
  let depth = 1;
  let end = open + 1;
  for (; depth; ++end) {
    if (source[end] === '{') ++depth;
    if (source[end] === '}') --depth;
    assert(end < source.length, signature);
  }
  return source.slice(open + 1, end - 1);
};

for (const [file, type, actual] of [
  ['scroll_area.cpp', 'ScrollArea', 'verticalScrollBar()->value()'],
  ['elastic_scroll.cpp', 'ElasticScroll', '_state.visibleFrom'],
]) {
  const source = read(`Telegram/lib_ui/ui/widgets/${file}`);
  const jump = body(source, type === 'ScrollArea' ? 'void ScrollArea::scrollToY(' : 'void ElasticScroll::scrollTo(int');
  assert.match(jump, /_smoothActive = false;/);
  assert.match(jump, /_smoothWheelTimer.cancel\(\);/);
  const start = body(source, `void ${type}::startSmoothWheel(`);
  const init = body(start, 'if (!_smoothActive)');
  assert.match(init, /_smoothLast = crl::now\(\);/);
  assert.equal((start.match(/_smoothLast\s*=/g) || []).length, 1);
  const tick = body(source, `void ${type}::smoothWheelTick(`);
  const comparison = tick.match(new RegExp(
    actual.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
      + ' (==|!=) int\\(std::lround\\(_smoothCurrent\\)\\)'));
  assert(comparison, file);
  const stalled = (value, requested) => comparison[1] === '!='
    ? value !== requested : value === requested;
  assert.equal(stalled(20, 20), false, 'successful motion must not stall');
  assert.equal(stalled(100, 120), true, 'clamped motion must stall');
  assert.match(tick, /_smoothLast = now;/);

  // Model the production follower at timer cadence with dense input between ticks.
  let current = 0, target = 0, last = 0, stalls = 0, active = true;
  for (let time = 1; time <= 160; ++time) {
    target += 5;
    if (time % 16) continue;
    const dt = Math.min(Math.max(time - last, 1), 100);
    last = time;
    current += (target - current) * (1 - Math.exp(-dt / 85));
    const requested = Math.round(current);
    stalls = stalled(requested, requested) ? stalls + 1 : 0;
    if (stalls > 3) active = false;
    assert.equal(dt, 16, 'wheel arrivals must not reset frame time');
  }
  assert(active, 'sustained successful scrolling must remain active');
  assert(current > 400, 'dense events must make forward progress');
}

const content = read('Telegram/SourceFiles/info/info_content_widget.cpp');
const topbar = read('Telegram/SourceFiles/info/profile/info_profile_top_bar.cpp');
const gradient = read('Telegram/SourceFiles/ui/top_background_gradient.cpp');
const gradientHeader = read('Telegram/SourceFiles/ui/top_background_gradient.h');
assert.doesNotMatch(content, /AvatarGradient|ProfileGradient|QLinearGradient/);
assert.doesNotMatch(gradient + gradientHeader + topbar, /ProfileGradientScroll/);
assert.match(topbar, /dynamic_cast<ContentWidget\*>\(parent.get\(\)\)/);
assert.match(topbar, /content->scrollTopValue\(/);
const paint = body(topbar, 'void TopBar::paintEvent(');
assert.match(paint, /paintEdges\(p, clipBounds\);\n\t\tp.save\(\);/);
assert.match(paint, /setClipPath\(_cachedClipPath, Qt::IntersectClip\)/);
assert.match(paint, /p.drawImage\(x, y, _cachedGradient\);\n\t\tp.restore\(\);/);
assert.doesNotMatch(paint, /setClipping\(false\)|paintBand|mainRect/);
assert.match(paint, /p.setOpacity\(_ayuGradientColors\n\t\t\t\? _ayuGradientFade\n\t\t\t: 1\.\);/);
assert.match(gradient, /const auto key = peer->userpicUniqueKey\(view\);/);
assert.match(gradient, /base::flat_map<InMemoryKey, AvatarGradient>/);
assert.match(topbar, /AvatarGradientColorsFor\(_peer\) != _ayuGradientColors/);
assert.match(topbar, /Data::PeerUpdate::Flag::Photo/);
const fade = top => Math.min(Math.max(1 - top / 600, 0), 1);
assert.deepEqual([-50, 0, 300, 600, 900].map(fade), [1, 1, 0.5, 0, 0]);
console.log('PASS: scroll/gradient source contracts and timing model (not Qt runtime or pixel tests).');
