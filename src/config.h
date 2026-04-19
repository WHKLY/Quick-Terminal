#ifndef QUICK_TERMINAL_CONFIG_H
#define QUICK_TERMINAL_CONFIG_H

#include "app.h"

void SetDefaultConfig(AppConfig *config);
BOOL BuildConfigDirectoryPathFromName(const wchar_t *directory_name, wchar_t *buffer, size_t buffer_count);
BOOL ResolveConfigDirectory(const AppOptions *options);
BOOL WriteStoredConfigDirectory(const wchar_t *directory_path);
const wchar_t *GetResolvedConfigDirectoryPath(void);
BOOL SaveConfig(const AppConfig *config);
BOOL LoadConfig(AppConfig *config);
BOOL ValidateConfig(const AppConfig *config);

#endif
