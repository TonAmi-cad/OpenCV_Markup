#pragma once
#ifndef ZIPHELPER_H
#define ZIPHELPER_H

#include <zip.h>
#include <string>

void addFileToZip(zip_t* zip, const std::string& path, const std::string& zipPath);

#endif
