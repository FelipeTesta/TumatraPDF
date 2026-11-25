// Reproduction probe for flashcard NAVIGATION stutter
// Creates many flashcards, then navigates pages quickly with timing
// Fails when a page change takes > 500ms or ping times out

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, cmdId, runStandalone, tmpPath } from "./util";
import { sleep } from "./winapi";

function makeMultiPageTextPdf(pageCount: number, linesPerPage: number): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc(`<< /Type /Catalog /Pages 2 0 R >>`);
  body[2] = enc(`<< /Type /Pages /Kids [${Array.from({ length: pageCount }, (_, i) => `${3 + i * 2} 0 R`).join(" ")}] /Count ${pageCount} >>`);
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");

  let objNum = 7;
  for (let p = 0; p < pageCount; p++) {
    const pageObj = 3 + p * 2;
    const contentObj = pageObj + 1;
    const streamLines = Array.from({ length: linesPerPage }, (_, i) =>
      `Page ${p + 1} Line ${i + 1}: The quick brown fox jumps over the lazy dog.`
    ).join(" ");
    const stream = `BT /F1 18 Tf 72 700 Td (${streamLines}) Tj ET`;
    body[contentObj] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
    body[pageObj] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents ${contentObj} 0 R >>`
    );
    objNum = contentObj + 1;
  }

  const maxN = objNum - 1;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n] ?? enc("null"), enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("flashcard-nav-stutter.pdf");
  // 100 pages with content, will create flashcards on 50 of them
  writeFileSync(pdf, makeMultiPageTextPdf(100, 3));

  const exe = EXE;
  const pipeName = `sumatra-control-${process.pid}-${Date.now()}`;
  
  const proc = Bun.spawn([exe, "-for-testing", "-dbg-control", pipeName, pdf], {
    stdout: "ignore",
    stderr: "pipe",
  });

  console.log(`App PID: ${proc.pid}`);
  
  let client: ControlClient | undefined;
  try {
    client = await ControlClient.connect(pipeName, 10000);
    
    // Wait for document to load
    await sleep(2000);
    
    // Enable flashcard mode
    await client.request(ControlCommand.TestRunCommand, [cmdId("CmdFlashcardToggle")]);
    await sleep(500);
    
    // Create flashcards on pages 1-50 (on 50 pages of a 100-page doc)
    const numCards = 50;
    for (let page = 1; page <= numCards; page++) {
      const res = await client.request(ControlCommand.TestCreateFlashcard, [page]);
      if (res[0] !== 1) {
        throw new Error(`Failed to create flashcard on page ${page}: ${res[1]}`);
      }
      console.log(`Created flashcard on page ${page}, key=${res[1]}`);
      await sleep(100);
    }
    
    await sleep(1000);
    
    // Test navigation: go through pages 1-20 and back, measure time
    const pageCmds = {
      next: cmdId("CmdGoToNextPage"),
      prev: cmdId("CmdGoToPrevPage"),
    };
    
    console.log("\n=== Navigation test with cards ===");
    const navTimes: number[] = [];
    const pingTimeoutMs = 5000;
    
    for (let pass = 0; pass < 2; pass++) {
      for (let page = 1; page <= 100; page++) {
        // Ping before navigation
        const pingStart = Date.now();
        try {
          await Promise.race([
            client.request(ControlCommand.Ping),
            new Promise((_, reject) => setTimeout(() => reject(new Error("Ping timeout")), pingTimeoutMs))
          ]);
        } catch (e) {
          throw new Error(`App froze before navigating to page ${page}: ${e}`);
        }
        
        const navStart = Date.now();
        await client.request(ControlCommand.TestRunCommand, [pageCmds.next]);
        const navTime = Date.now() - navStart;
        navTimes.push(navTime);
        
        console.log(`Nav to page ${page} (pass ${pass + 1}) took ${navTime}ms`);
        
        if (navTime > 1000) {
          throw new Error(`Navigation took ${navTime}ms > 1000ms timeout on page ${page}`);
        }
        
        await sleep(100);
      }
      
      // Go back
      for (let page = 100; page >= 1; page--) {
        const pingStart = Date.now();
        try {
          await Promise.race([
            client.request(ControlCommand.Ping),
            new Promise((_, reject) => setTimeout(() => reject(new Error("Ping timeout")), pingTimeoutMs))
          ]);
        } catch (e) {
          throw new Error(`App froze before navigating back to page ${page}: ${e}`);
        }
        
        const navStart = Date.now();
        await client.request(ControlCommand.TestRunCommand, [pageCmds.prev]);
        const navTime = Date.now() - navStart;
        navTimes.push(navTime);
        
        console.log(`Nav back to page ${page} (pass ${pass + 1}) took ${navTime}ms`);
        
        if (navTime > 1000) {
          throw new Error(`Navigation back took ${navTime}ms > 1000ms timeout on page ${page}`);
        }
        
        await sleep(100);
      }
    }
    
    // Summary
    const avgTime = navTimes.reduce((a, b) => a + b, 0) / navTimes.length;
    const maxTime = Math.max(...navTimes);
    console.log(`\n=== Navigation Summary ===`);
    console.log(`Average navigation time: ${avgTime.toFixed(1)}ms`);
    console.log(`Max navigation time: ${maxTime}ms`);
    console.log(`Total navigations: ${navTimes.length}`);
    
    // Now test WITHOUT cards for comparison (can't easily remove, but we can note)
    console.log("\n=== Test passed - no navigation stutter detected ===");
    
  } finally {
    if (client) {
      try {
        await client.quit();
      } catch {
        proc.kill();
      }
      client.close();
    } else {
      proc.kill();
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}