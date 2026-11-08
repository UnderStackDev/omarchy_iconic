#pragma once

#include <QStringList>

// Iconic without a window, for scripts, the Omarchy hook, and agents.
// Every command takes --json, exits 0 on success and 1 on failure, and
// writes errors to stderr.
namespace Cli {
// True when `args` (argv without the program) asks for a command rather
// than the window.
bool wantsCli(const QStringList &args);
int run(const QStringList &args);
}
