// Regenerates src/langs.json from the console's translation files, so the web
// UI ships every language and never needs a backend lookup.
//
//   npm run langs

import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const langsDir = join(here, "..", "..", "data", "assets", "langs");
const output = join(here, "..", "src", "langs.json");

const tables = {};

for (const file of readdirSync(langsDir).sort()) {
  if (!file.toLowerCase().endsWith(".ini")) continue;

  const table = {};
  for (const raw of readFileSync(join(langsDir, file), "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith(";") || line.startsWith("#")) continue;

    const separator = line.indexOf("=");
    if (separator <= 0) continue;

    const key = line.slice(0, separator).trim();
    const value = line.slice(separator + 1).trim();
    if (key) table[key] = value;
  }

  tables[file.replace(/\.ini$/i, "")] = table;
}

// Web-only translations live in src/langs.overrides.json so the console's ini
// files stay untouched; they win over anything the ini files provide.
const overridesPath = join(here, "..", "src", "langs.overrides.json");
if (existsSync(overridesPath)) {
  const overrides = JSON.parse(readFileSync(overridesPath, "utf8"));
  for (const [language, entries] of Object.entries(overrides)) {
    tables[language] = { ...(tables[language] ?? {}), ...entries };
  }

  // The web looks strings up by their English text, so two keys with the same
  // value collapse into one. Report them here instead of discovering it in the
  // UI; the first key wins, which keeps the console's wording in charge.
  const byText = new Map();
  for (const [key, value] of Object.entries(tables.English ?? {})) {
    const text = String(value).trim().toLowerCase();
    if (!byText.has(text)) byText.set(text, []);
    byText.get(text).push(key);
  }

  for (const [text, keys] of byText) {
    if (keys.length > 1) console.warn(`duplicate English text ${JSON.stringify(text)}: ${keys.join(", ")}`);
  }
}

writeFileSync(output, JSON.stringify(tables), "utf8");
console.log(`langs.json: ${Object.keys(tables).length} languages -> ${output}`);
