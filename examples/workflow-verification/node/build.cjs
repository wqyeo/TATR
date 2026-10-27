const fs = require('node:fs');
const path = require('node:path');
const render = require('fixture-message');
const output = process.argv[2] || 'dist';
if (process.env.FIXTURE_MESSAGE !== 'built through TATR') {
  throw new Error('Expected dotenv fixture value');
}
fs.mkdirSync(output, { recursive: true });
fs.writeFileSync(path.join(output, 'index.txt'), render(process.env.FIXTURE_MESSAGE));
console.log(`Node build completed: ${output}`);
