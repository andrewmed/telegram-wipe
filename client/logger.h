#include <string>
#include <iostream>

namespace Logger {
using namespace std;

enum LogLevel {
  TRACE,
  DEBUG,
  INFO,
  ERROR
};  


void logPrint() {
  cerr << '\n';
}

template <typename Head, typename... Tail>
inline void logPrint(Head&& head, Tail... tail) {
  cerr << ' ' << head;
  logPrint(tail...);
}

void logTime(string&& level) {
  auto now = chrono::system_clock::now();
  auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
  auto timer = chrono::system_clock::to_time_t(now);
  std::tm bt = *std::localtime(&timer);
  cerr << std::put_time(&bt, "%H:%M:%S");  // HH:MM:SS
  cerr << '.' << std::setfill('0') << std::setw(3) << ms.count();
  cerr << ' ' << level;
}
}

Logger::LogLevel GlobalLogger = Logger::INFO;

template <typename... Args>
void trace(Args &&...args) {
  if (GlobalLogger > Logger::TRACE) {
    return;
  }
  Logger::logTime("[TRACE]");
  Logger::logPrint(args...);
}

template <typename... Args>
void debug(Args &&...args) {
  if (GlobalLogger > Logger::DEBUG) {
    return;
  }
  Logger::logTime("[DEBUG]");
  Logger::logPrint(args...);
}

template <typename... Args>
void info(Args &&...args) {
  if (GlobalLogger > Logger::INFO) {
    return;
  }
  Logger::logTime("[INFO] ");
  Logger::logPrint(args...);
}

template <typename... Args>
void error(Args &&...args) {
  if (GlobalLogger > Logger::ERROR) {
    return;
  }
  Logger::logTime("[ERROR]");
  Logger::logPrint(args...);
}