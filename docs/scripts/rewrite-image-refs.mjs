#!/usr/bin/env node
/**
 * rewrite-image-refs.mjs — 批量改写 markdown 图片引用 .png/.jpg/.jpeg → .webp
 *
 * 只替换 markdown 图片语法中的图片路径后缀：
 *   ![](xxx.png)  →  ![](xxx.webp)
 *   ![](xxx.jpg)  →  ![](xxx.webp)
 *   ![](xxx.jpeg) →  ![](xxx.webp)
 *
 * 不触碰非图片上下文的 .png/.jpg 字符串。
 *
 * 用法（在仓库根目录运行）：
 *   node docs/scripts/rewrite-image-refs.mjs            # dry-run
 *   node docs/scripts/rewrite-image-refs.mjs --apply    # 实际写入
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const DOCS_DIR = path.resolve(__dirname, '..');

const TUTORIAL_DIRS = [
  path.join(DOCS_DIR, 'tutorials'),
  path.join(DOCS_DIR, 'en/tutorials'),
];

const apply = process.argv.includes('--apply');

// 匹配 markdown 图片语法里的图片路径，捕获后缀
// 形如 ![]( ... .png )  或  ![alt]( ... .jpg "title" )
const IMG_RE = /(\!\[[^\]]*\]\([^)\s]*?)\.(png|jpg|jpeg)(\s*("[^"]*")?\))/gi;

function walkMd(dir) {
  const out = [];
  if (!fs.existsSync(dir)) return out;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walkMd(full));
    else if (entry.name.endsWith('.md')) out.push(full);
  }
  return out;
}

function main() {
  const files = [];
  for (const dir of TUTORIAL_DIRS) files.push(...walkMd(dir));

  console.log('='.repeat(70));
  console.log(`  Rewrite markdown image refs → .webp`);
  console.log(`  Mode: ${apply ? 'APPLY' : 'DRY RUN (no files written)'}`);
  console.log(`  Markdown files: ${files.length}`);
  console.log('='.repeat(70));

  let totalReplacements = 0;
  let changedFiles = 0;

  for (const file of files) {
    const orig = fs.readFileSync(file, 'utf-8');
    let count = 0;
    const next = orig.replace(IMG_RE, (m, pre, _ext, post) => {
      count++;
      return `${pre}.webp${post}`;
    });

    if (count > 0) {
      const rel = path.relative(DOCS_DIR, file);
      console.log(`  [${apply ? 'OK' : 'DRY'}] ${rel}  (${count} refs)`);
      totalReplacements += count;
      changedFiles++;
      if (apply) fs.writeFileSync(file, next);
    }
  }

  console.log('\n' + '='.repeat(70));
  console.log(`  Summary:`);
  console.log(`    Files changed:  ${changedFiles}`);
  console.log(`    Refs rewritten: ${totalReplacements}`);
  if (!apply) {
    console.log('\n  ⚠ DRY RUN — no files were written.');
    console.log('    Run with --apply to rewrite.');
  }
}

main();
