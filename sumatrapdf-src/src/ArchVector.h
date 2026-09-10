/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineMupdf;
struct DisplayModel;

struct ArchVecSeg {
    float x0, y0, x1, y1; // in PAGE coordinates (PDF user space, points)
};

struct ArchSnapResult {
    bool snapped;
    float pageX, pageY; // in PAGE coordinates
};

// Extract vector line segments from a PDF page (architecture drawings are vector lines).
// Returns cached segments if already extracted for this page.
Vec<ArchVecSeg> ArchExtractPageSegments(EngineMupdf* engine, int pageNo);

// Clear the vector segment cache (call on document load/close).
void ArchClearVectorCache();

// Snap a screen point to the nearest vector endpoint or segment.
// screenX, screenY: screen coordinates (pixels)
// thresholdPx: snap threshold in screen pixels
// Returns snapped page coordinates if within threshold, otherwise the converted point.
ArchSnapResult ArchSnapToVector(DisplayModel* dm, int pageNo, float screenX, float screenY, float thresholdPx);

// Hit-test a page point against vector segments.
// pageX, pageY: point in PAGE coordinates
// thresholdPage: distance threshold in PAGE units
// If a segment is within threshold, returns true and fills outX0/outY0/outX1/outY1 with the segment endpoints (PAGE
// coords).
bool ArchHitTestSegment(DisplayModel* dm, int pageNo, float pageX, float pageY, float thresholdPage, float* outX0,
                        float* outY0, float* outX1, float* outY1);