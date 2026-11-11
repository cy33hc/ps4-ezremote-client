// Translations are generated from data/assets/langs/*.ini into langs.json, so
// the web UI never needs a backend call. Entries are looked up by their English
// text, which lets existing literals be wrapped with t("…") as they are touched.

import tables from "./langs.json";

type Table = Record<string, string>;

const languages = tables as unknown as Record<string, Table>;

let dictionary: Record<string, string> = {};
let currentLanguage = "English";

function normalise(text: string): string {
  return text.trim().replace(/\s+/g, " ").toLowerCase();
}

/** Text nodes and placeholder/title attributes that should never be touched. */
const SKIP_TAGS = new Set(["INPUT", "TEXTAREA", "CODE", "PRE", "SCRIPT", "STYLE"]);
const ATTRIBUTES = ["title", "placeholder", "aria-label"];

// What each node/attribute looked like before we translated it. Without this,
// switching back to a language whose text equals the source would leave the
// old translation in place: React sees identical virtual text and never
// rewrites the node we changed.
const textSources = new WeakMap<Node, string>();
const textWritten = new WeakMap<Node, string>();
const attributeSources = new WeakMap<Element, Record<string, string>>();
const attributeWritten = new WeakMap<Element, Record<string, string>>();


const CACHE_KEY = "ezremote.language";

/** Last language seen, so the sign-in screen can translate before any request. */
export function cachedLanguage(): string | null {
  try {
    return localStorage.getItem(CACHE_KEY);
  } catch {
    return null;
  }
}

function rememberLanguage(language: string): void {
  try {
    localStorage.setItem(CACHE_KEY, language);
  } catch {
    // Storage may be unavailable; translations still work for this session.
  }
}

/** Builds the English → localized lookup for one language. */
export function loadLanguage(language: string): void {
  currentLanguage = language && language !== "Default" ? language : "English";

  const english = languages.English ?? {};
  const localized = languages[currentLanguage] ?? null;

  if (!localized || currentLanguage === "English") {
    dictionary = {};
    return;
  }

  // Keyed by the normalised English text so casing/spacing differences in the
  // UI literals still resolve.
  const next: Record<string, string> = {};
  for (const key of Object.keys(english)) {
    const value = localized[key];
    const from = english[key];
    if (!value || !from || value === from) continue;

    // Two keys can share an English value (e.g. STR_INSTALL_FAILED and
    // STR_STATE_FAILED). The console's INI order comes first, so it wins;
    // this keeps the lookup deterministic instead of last-writer-wins.
    const id = normalise(from);
    if (!(id in next)) next[id] = value;
  }
  dictionary = next;
  rememberLanguage(currentLanguage);

  // Re-walk the page so text that is not re-rendered from JSX (plain markup
  // translated in place) follows the new language, and reverts to the source
  // when switching back to one that matches it.
  if (typeof document !== "undefined" && document.body) translateDom();

}

/** Small handle for diagnosing translations from the browser console. */
if (typeof window !== "undefined") {
  (window as unknown as Record<string, unknown>).__i18n = () => ({
    language: currentLanguage,
    entries: Object.keys(dictionary).length,
  });
}

export function getLanguage(): string {
  return currentLanguage;
}

/** Languages present in the generated table, for pickers. */
export function languageNames(): string[] {
  return Object.keys(languages).sort();
}

/** Translates by English text; returns the input unchanged when unknown. */
export function t(text: string | undefined): string {
  if (!text) return "";
  return dictionary[normalise(text)] ?? text;
}

function translateAttribute(element: Element, name: string): void {
  const current = element.getAttribute(name);
  if (current === null) return;

  const sources = attributeSources.get(element) ?? {};
  const written = attributeWritten.get(element) ?? {};

  // Anything other than what we last wrote came from React: new source text.
  if (written[name] !== current) sources[name] = current;

  const source = sources[name] ?? current;
  const translated = t(source);
  const next = translated === source ? source : translated;

  attributeSources.set(element, sources);
  if (next !== current) {
    written[name] = next;
    element.setAttribute(name, next);
  }
  attributeWritten.set(element, written);
}

function translateAttributes(element: Element): void {
  for (const name of ATTRIBUTES) translateAttribute(element, name);
}

function translateText(node: Node): void {
  const current = node.nodeValue;
  if (!current) return;

  if (textWritten.get(node) !== current) textSources.set(node, current);

  const source = textSources.get(node) ?? current;
  const trimmed = source.trim();
  if (!trimmed) return;

  const translated = t(trimmed);
  const next = translated === trimmed ? source : source.replace(trimmed, translated);
  if (next === current) return;

  textWritten.set(node, next);
  node.nodeValue = next;
}

function translateNode(node: Node): void {
  if (node.nodeType === Node.TEXT_NODE) {
    translateText(node);
    return;
  }

  if (node.nodeType !== Node.ELEMENT_NODE) return;

  const element = node as Element;
  if (SKIP_TAGS.has(element.tagName)) return;
  if (element instanceof HTMLElement && element.dataset.noI18n !== undefined) return;

  translateAttributes(element);
  for (const child of Array.from(element.childNodes)) translateNode(child);
}

/**
 * Translates the rendered DOM in place. React keeps its own English strings in
 * the virtual tree, so this runs after every commit and on language change,
 * which gives full coverage without touching every call site.
 */
export function translateDom(root: Node = document.body): void {
  translateNode(root);
}

/** Watches for re-renders so newly shown dialogs are translated too. */
export function observeTranslations(): () => void {
  if (typeof MutationObserver === "undefined") return () => {};

  const observer = new MutationObserver((mutations) => {
    for (const mutation of mutations) {
      if (mutation.type === "characterData") translateNode(mutation.target);
      for (const added of Array.from(mutation.addedNodes)) translateNode(added);
    }
  });

  observer.observe(document.body, { childList: true, subtree: true, characterData: true });
  translateDom();
  return () => observer.disconnect();
}

/**
 * Translates a template and fills {name} placeholders. The template is looked up
 * first, so translations may reorder or drop placeholders.
 */
export function tp(template: string, values: Record<string, string | number>): string {
  const text = t(template);
  return text.replace(/\{(\w+)\}/g, (_match, key: string) => {
    const value = values[key];
    return value === undefined ? `{${key}}` : String(value);
  });
}

// Prime from the last known language before the first paint: the sign-in
// screen otherwise waits for a request to come back. Kept at the end of the
// module so every declaration it can reach is already initialised.
const cached = cachedLanguage();
if (cached && languages[cached]) loadLanguage(cached);
