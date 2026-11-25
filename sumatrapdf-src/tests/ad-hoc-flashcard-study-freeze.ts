// Reproduction probe for flashcard STUDY mode freeze
// Creates flashcards, starts study mode, rates cards one by one with timeout checks
// Fails when a rating takes > 2s or ping times out (indicating UI freeze)

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, cmdId, runStandalone, tmpPath } from "./util";
import { sleep } from "./winapi";
import { spawn } from "node:child_process";

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

function dumpThreadStacks(pid: number): string {
  const symbolsPath = "srv*C:\\Users\\Testa\\AppData\\Local\\Temp\\opencode\\symbols*https://msdl.microsoft.com/download/symbols";
  const cdbPath = "C:\\Users\\Testa\\AppData\\Local\\Temp\\opencode\\cdb\\cdb.exe";
  const outFile = `C:\\Users\\Testa\\AppData\\Local\\Temp\\cdb_dump_${pid}_${Date.now()}.txt`;
  
  return new Promise<string>((resolve, reject) => {
    const proc = spawn(cdbPath, [
      "-p", String(pid),
      "-y", symbolsPath,
      "-c", "~* kb 30; q",
      "-logo", outFile
    ], { stdio: "pipe" });
    
    let stdout = "";
    let stderr = "";
    proc.stdout?.on("data", (d) => stdout += d.toString());
    proc.stderr?.on("data", (d) => stderr += d.toString());
    
    proc.on("close", (code) => {
      const fs = require("node:fs");
      try {
        const content = fs.readFileSync(outFile, "utf8");
        fs.unlinkSync(outFile);
        resolve(content);
      } catch (e) {
        resolve(`stdout: ${stdout}\nstderr: ${stderr}\n(exit code ${code})`);
      }
    });
    
    proc.on("error", (e) => {
      reject(e);
    });
    
    setTimeout(() => {
      proc.kill();
      reject(new Error("cdb timeout"));
    }, 30000);
  });
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("flashcard-study-freeze.pdf");
  writeFileSync(pdf, makeMultiPageTextPdf(10, 5)); // 10 pages, 5 lines each

  const exe = EXE;
  const pipeName = `sumatra-control-${process.pid}-${Date.now()}`;
  
  // Start the app with control pipe
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
    
    // Create flashcards on pages 1-10
    const cardKeys: string[] = [];
    for (let page = 1; page <= 10; page++) {
      const res = await client.request(ControlCommand.TestCreateFlashcard, [page]);
      if (res[0] !== 1) {
        throw new Error(`Failed to create flashcard on page ${page}: ${res[1]}`);
      }
      cardKeys.push(String(res[1]));
      console.log(`Created flashcard on page ${page}, key=${res[1]}`);
      await sleep(200);
    }
    
    // Start study mode
    await client.request(ControlCommand.TestRunCommand, [cmdId("CmdFlashcardStudy")]);
    await sleep(1000);
    
    // Debug: check study order using TestFlashcardFlush
    try {
      const res = await client.request(ControlCommand.TestFlashcardFlush, [0]);
      console.log("FlashcardFlush result after study start:", res);
    } catch (e) {
      console.log("FlashcardFlush error:", e);
    }
    
    // Get rate command IDs
    const rate1 = cmdId("CmdFlashcardRate1");
    const rate2 = cmdId("CmdFlashcardRate2");
    const rate3 = cmdId("CmdFlashcardRate3");
    const rate4 = cmdId("CmdFlashcardRate4");
    
    // Rate cards one by one with timing - use TestRunCommand with actual rate commands
    // instead of TestRateFlashcard to avoid cardIdx issues
    const maxCards = 10;
    const timeoutMs = 2000; // 2 second timeout per rating
    const pingTimeoutMs = 5000; // 5 second ping timeout
    
    for (let i = 0; i < maxCards; i++) {
      console.log(`\n--- Rating card ${i + 1}/${maxCards} ---`);
      
      // Ping before rating to verify app is responsive
      const pingStart = Date.now();
      try {
        await Promise.race([
          client.request(ControlCommand.Ping),
          new Promise((_, reject) => setTimeout(() => reject(new Error("Ping timeout")), pingTimeoutMs))
        ]);
        console.log(`Pre-rating ping OK (${Date.now() - pingStart}ms)`);
      } catch (e) {
        console.log(`\n!!! APP FROZE BEFORE RATING CARD ${i + 1} - DUMPING THREAD STACKS !!!`);
        try {
          const stacks = await dumpThreadStacks(proc.pid);
          console.log("=== THREAD STACKS ===");
          console.log(stacks);
          console.log("=== END THREAD STACKS ===");
        } catch (dumpErr) {
          console.error("Failed to dump stacks:", dumpErr);
        }
        throw new Error(`App froze before rating card ${i + 1}: ${e}`);
      }
      
      // Rate the card using actual rate command (uses currentCardIdx)
      const rateCmd = [rate1, rate2, rate3, rate4][i % 4];
      const rateStart = Date.now();
      
      try {
        const rateResult = await Promise.race([
          client.request(ControlCommand.TestRunCommand, [rateCmd]),
          new Promise((_, reject) => setTimeout(() => reject(new Error("Rate timeout")), timeoutMs))
        ]);
        const rateTime = Date.now() - rateStart;
        console.log(`Rate card ${i + 1} took ${rateTime}ms, result: ${rateResult[0]}`);
        
        if (rateTime > timeoutMs) {
          throw new Error(`Rating took ${rateTime}ms > ${timeoutMs}ms timeout`);
        }
        if (rateResult[0] !== 1) {
          throw new Error(`Rate failed: ${rateResult[1]}`);
        }
      } catch (e) {
        if (e instanceof Error && e.message.includes("timeout")) {
          console.log(`\n!!! APP FROZE DURING RATING CARD ${i + 1} - DUMPING THREAD STACKS !!!`);
          try {
            const stacks = await dumpThreadStacks(proc.pid);
            console.log("=== THREAD STACKS ===");
            console.log(stacks);
            console.log("=== END THREAD STACKS ===");
          } catch (dumpErr) {
            console.error("Failed to dump stacks:", dumpErr);
          }
          throw new Error(`App froze during rating card ${i + 1} (${timeoutMs}ms timeout)`);
        }
        throw e;
      }
      
      // Ping after rating
      const pingStart2 = Date.now();
      try {
        await Promise.race([
          client.request(ControlCommand.Ping),
          new Promise((_, reject) => setTimeout(() => reject(new Error("Ping timeout")), pingTimeoutMs))
        ]);
        console.log(`Post-rating ping OK (${Date.now() - pingStart2}ms)`);
      } catch (e) {
        console.log(`\n!!! APP FROZE AFTER RATING CARD ${i + 1} - DUMPING THREAD STACKS !!!`);
        try {
          const stacks = await dumpThreadStacks(proc.pid);
          console.log("=== THREAD STACKS ===");
          console.log(stacks);
          console.log("=== END THREAD STACKS ===");
        } catch (dumpErr) {
          console.error("Failed to dump stacks:", dumpErr);
        }
        throw new Error(`App froze after rating card ${i + 1}: ${e}`);
      }
      
      // Small delay between cards
      await sleep(100);
    }
    
    console.log("\n✅ All cards rated successfully without freeze!");
    
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