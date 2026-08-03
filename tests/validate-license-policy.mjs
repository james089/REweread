import fs from 'node:fs';

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

const license = fs.readFileSync('LICENSE', 'utf8');
const readme = fs.readFileSync('README.md', 'utf8');
const agents = fs.readFileSync('AGENTS.md', 'utf8');
const dependencies = fs.readFileSync('docs/dependencies.md', 'utf8');
const legal = fs.readFileSync('docs/legal-and-commercial-use.md', 'utf8');
const release = fs.readFileSync('docs/release-checklist.md', 'utf8');
const notices = fs.readFileSync('docs/third-party-notices.md', 'utf8');

assert(license.startsWith('MIT License'), 'root license must use the standard MIT title');
assert(license.includes('Copyright (c) 2026 nasonliu'), 'root license must carry the project copyright notice');
assert(license.includes('Permission is hereby granted, free of charge'), 'root license must include the standard MIT grant');
assert(license.includes('THE SOFTWARE IS PROVIDED "AS IS"'), 'root license must include the standard MIT warranty disclaimer');
assert(!license.includes('PolyForm Noncommercial'), 'root license must no longer contain the former noncommercial grant');

for (const text of [readme, agents, dependencies, legal, release, notices]) {
  assert(text.includes('MIT'), 'project policy documents must name the active MIT license');
}

assert(readme.includes('OSI 批准的开源软件许可证'), 'README must accurately identify MIT as OSI approved');
assert(readme.includes('本项目自有代码'), 'README must scope the MIT grant to project-owned code');
assert(agents.includes('commercial use of project-owned code'), 'Agent guide must describe the MIT commercial-use grant precisely');
assert(legal.includes('微信读书用户协议') && legal.includes('weread.koplugin'), 'legal guide must identify service-terms and upstream-license blockers');
assert(legal.includes('所有当前贡献者均已同意'), 'legal guide must record the current contributor relicensing basis');
assert(release.includes('service or trademark authorization'), 'release checklist must keep service and trademark authorization separate');

console.log('license policy ok');
