# WebView2 Adapter Pattern — Markdown HtmlWindowCallback

## What

JS notify (page calls `window.chrome.webview.postMessage`) → `BrowserDocView::OnJsNotifyCb` (BrowserDocView.cpp:298) → adapter `MarkdownHtmlWindowHandler` (MarkdownModel.cpp:181) → `MarkdownModel` → `ControllerCallbackHandler`.

Adapter MUST forward every new `HtmlWindowCallback` virtual. No-op default = feature silently dies in Markdown.

Case-study: autoscroll ETA stuck on markdown — adapter did not forward `OnAutoScrollProgress`, so the `.md` ETA never updated and bottom stop desynced. Contrast adapter had 3 divergent JS copies that drifted.

## Current required forwards (all 9, HtmlWindow.h:15-43)

| #   | Method                                      | HtmlWindow.h line | default      |
| --- | ------------------------------------------- | ----------------- | ------------ |
| 1   | `OnBeforeNavigate(Str url, bool newWindow)` | 15                | pure virtual |
| 2   | `OnDocumentComplete(Str url)`               | 18                | pure virtual |
| 3   | `GetDataForUrl(Str url)`                    | 22                | pure virtual |
| 4   | `OnLButtonDown()`                           | 26                | pure virtual |
| 5   | `DownloadData(Str url, Str data)`           | 29                | pure virtual |
| 6   | `OnFindResult(int, int, int)`               | 33                | no-op `{}`   |
| 7   | `OnFindAllResult(Str)`                      | 37                | no-op `{}`   |
| 8   | `OnAutoScrollBottom()`                      | 40                | no-op `{}`   |
| 9   | `OnAutoScrollProgress(int remainingPx)`     | 43                | no-op `{}`   |

All 9 forwarded in `MarkdownHtmlWindowHandler` (MarkdownModel.cpp:188-199) via `mm->OnXxx(...)`, then `MarkdownModel` relays to `cb->OnXxx(...)`.

## Pattern

Adapter impl:

```cpp
virtual void OnXxx(...) override { mm->OnXxx(...); }
```

i.e. `virtual void OnXxx() { callback->OnXxx(); }` — thin pass-through, no logic in adapter.

## Work rule

ANY future feature adding a JS bridge method (= new `HtmlWindowCallback` virtual) MUST:

1. Add adapter forward in `MarkdownHtmlWindowHandler` (all 9, not subset).
2. Add control test (`Test*`) in `cmd/control.ts` + C++ handler in `SumatraControl.cpp` that exercises the new forward.

Never add a JS bridge method without the adapter forward — otherwise it silently no-ops in Markdown.
