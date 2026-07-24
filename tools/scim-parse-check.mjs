// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Drive SCIM SaveParser_Read (AnthorNet/SCIM) against a local .sav to get the
// exact property that trips readStructProperty. Usage:
//   node scim-parse-check.mjs <save.sav> [scimRepoRoot]
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const savePath = process.argv[2];
const scimRoot = process.argv[3] ?? 'D:/Modding/Satisfactory/Home/external-SCIM';

// Read.js tail registers a web-worker handler on `self`; stub for node.
globalThis.self = { onmessage: null };

const readerUrl = pathToFileURL(path.join(scimRoot, 'src/SaveParser/Read.js')).href;
const { default: SaveParser_Read } = await import(readerUrl);

const buf = fs.readFileSync(savePath);
const arrayBuffer = buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);

const worker = {
  postMessage(msg) {
    switch (msg.command) {
      case 'alert':
      case 'alertParsing':
        console.log('WORKER-ALERT:', JSON.stringify(msg));
        break;
      case 'transferData':
        if (msg.data && msg.data.header) {
          console.log('HEADER:', JSON.stringify(msg.data.header));
        }
        break;
      case 'endSaveLoading':
        console.log('PARSE-OK');
        break;
      default:
        break; // progress spam
    }
  },
};

try {
  new SaveParser_Read(worker, { language: 'en', arrayBuffer });
} catch (err) {
  console.log('THROWN:', err.message);
}
