extern Arena* gLogAllocator;
extern str::Builder* gLogBuf;
extern bool gLogToConsole;
extern bool gLogToDebugger;
extern bool gReducedLogging;
extern bool gLogToPipe;
extern Str gLogAppName;
extern Str gLogFilePath;
void StartLogToFile(Str path, bool removeIfExists);
bool WriteCurrentLogToFile(Str path);
void DestroyLogging();
void LogParentProcessChain();

// Log levels for file output tagging
enum class LogLevel : int {
    Info = 0,
    Warn = 1,
    Error = 2,
};

// Log with level tag (file output only gets [LEVEL] prefix; debugger/console unchanged)
void log_level(LogLevel lvl, const char* fmt, ...);
#define LogInfo(fmt, ...) log_level(LogLevel::Info, fmt, __VA_ARGS__)
#define LogWarn(fmt, ...) log_level(LogLevel::Warn, fmt, __VA_ARGS__)
#define LogErr(fmt, ...) log_level(LogLevel::Error, fmt, __VA_ARGS__)