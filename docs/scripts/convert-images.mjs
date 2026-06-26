#!/usr/bin/env node
/**
 * convert-images.mjs — 教程图片 PNG/JPG → WebP 压缩
 *
 * 将 docs/tutorials/<tutorial>/images/ 与 docs/en/tutorials/<tutorial>/images/
 * 下的 PNG/JPG 转为 WebP：
 *   - quality 85（UI 截图目测无损）
 *   - 最大宽度 1440px（文档显示宽度通常 ≤1440，超出的等比缩小）
 *   - 输出同名 .webp，与原图同目录
 *
 * 规则与 cosmo-edge-website 的 site/scripts/convert-images.mjs 完全一致，
 * 压缩在主仓源头完成，避免开源仓库带入超大原图。
 *
 * 用法（在仓库根目录运行）：
 *   node docs/scripts/convert-images.mjs                   # dry-run（仅报告，不写文件）
 *   node docs/scripts/convert-images.mjs --apply           # 实际转换并写文件
 *   node docs/scripts/convert-images.mjs --apply --delete  # 转换后删除原 PNG/JPG
 *   node docs/scripts/convert-images.mjs --only 01-quickstart   # 限定单个教程目录
 */

import fs from 'fs';
import path from 'path';
import sharp from 'sharp';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
// docs/scripts/ -> docs/
const DOCS_DIR = path.resolve(__dirname, '..');

const QUALITY = 85;
const MAX_WIDTH = 1440;
const SOURCE_EXTS = new Set(['.png', '.jpg', '.jpeg']);

// 处理中英两套教程图片目录
const IMAGE_DIRS = [
  path.join(DOCS_DIR, 'tutorials'),
  path.join(DOCS_DIR, 'en/tutorials'),
];

const apply = process.argv.includes('--apply');
const deleteOriginals = process.argv.includes('--delete');

// --only <name> 限定单个教程目录（如 01-quickstart），用于小范围验证
const onlyIdx = process.argv.indexOf('--only');
const only = onlyIdx !== -1 ? process.argv[onlyIdx + 1] : null;

function collectImages(dir) {
  const results = [];
  if (!fs.existsSync(dir)) return results;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      if (entry.name === 'images') {
        for (const f of fs.readdirSync(full)) {
          if (SOURCE_EXTS.has(path.extname(f).toLowerCase())) {
            results.push(path.join(full, f));
          }
        }
      } else {
        results.push(...collectImages(full));
      }
    }
  }
  return results;
}

function fmt(bytes) {
  return bytes >= 1024 * 1024
    ? `${(bytes / 1024 / 1024).toFixed(2)}MB`
    : `${(bytes / 1024).toFixed(0)}KB`;
}

async function main() {
  const files = [];
  for (const dir of IMAGE_DIRS) {
    if (only) {
      // 仅处理名字匹配的教程子目录
      const target = path.join(dir, only);
      if (fs.existsSync(target)) files.push(...collectImages(target));
    } else {
      files.push(...collectImages(dir));
    }
  }

  console.log('='.repeat(70));
  console.log(`  Image conversion: PNG/JPG → WebP`);
  console.log(`  Mode: ${apply ? `APPLY${deleteOriginals ? ' + DELETE ORIGINALS' : ''}` : 'DRY RUN (no files written)'}`);
  console.log(`  Quality: ${QUALITY}  Max width: ${MAX_WIDTH}px`);
  console.log(`  Files found: ${files.length}`);
  console.log('='.repeat(70));

  if (files.length === 0) {
    console.log('\nNo images found.');
    return;
  }

  let totalOrig = 0;
  let totalWebp = 0;
  let converted = 0;
  let skipped = 0;
  const errors = [];

  for (const src of files) {
    const ext = path.extname(src);
    const dest = src.slice(0, -ext.length) + '.webp';
    const rel = path.relative(DOCS_DIR, src);

    try {
      const origSize = fs.statSync(src).size;
      totalOrig += origSize;

      if (!apply) {
        // dry-run: 读取元数据，但不写文件
        const meta = await sharp(src).metadata();
        const widthNote = meta.width > MAX_WIDTH ? ` (→${MAX_WIDTH})` : '';
        console.log(`  [DRY] ${rel}  ${fmt(origSize)}  ${meta.width}x${meta.height}${widthNote}`);
        continue;
      }

      await sharp(src)
        .resize({ width: MAX_WIDTH, withoutEnlargement: true })
        .webp({ quality: QUALITY })
        .toFile(dest);

      const webpSize = fs.statSync(dest).size;
      totalWebp += webpSize;
      const saved = ((1 - webpSize / origSize) * 100).toFixed(0);
      console.log(`  [OK]  ${rel}  ${fmt(origSize)} → ${fmt(webpSize)}  (-${saved}%)`);
      converted++;

      if (deleteOriginals && dest !== src) {
        fs.unlinkSync(src);
      }
    } catch (err) {
      console.error(`  [ERR] ${rel}: ${err.message}`);
      errors.push(rel);
      skipped++;
    }
  }

  console.log('\n' + '='.repeat(70));
  console.log(`  Summary:`);
  console.log(`    Processed:     ${converted + skipped}`);
  console.log(`    Converted:     ${converted}`);
  console.log(`    Errors:        ${skipped}`);
  console.log(`    Original size: ${fmt(totalOrig)}`);
  if (apply) {
    console.log(`    WebP size:     ${fmt(totalWebp)}`);
    console.log(`    Saved:         ${fmt(totalOrig - totalWebp)} (${((1 - totalWebp / totalOrig) * 100).toFixed(0)}%)`);
  } else {
    // dry-run 汇总原图总量
    console.log(`    (dry-run, no files written)`);
  }
  if (errors.length) {
    console.log(`\n  Errors:\n    ${errors.join('\n    ')}`);
  }
  if (!apply) {
    console.log('\n  ⚠ DRY RUN — no files were written.');
    console.log('    Run with --apply to convert, add --delete to also remove originals.');
  }
}

main().catch(err => {
  console.error(err);
  process.exit(1);
});
