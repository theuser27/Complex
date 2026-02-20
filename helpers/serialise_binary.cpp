
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define XHL_FILES_IMPL
#include "../Source/Third Party/xhl/files.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "../Source/Third Party/stb/stb_sprintf.h"

#ifdef _WIN32
  #define PATH_DELIMITER_CHAR '\\'
  #define PATH_DELIMITER_STRING "\\"
#else
  #define PATH_DELIMITER_CHAR '/'
  #define PATH_DELIMITER_STRING "/"
#endif

#define max(a, b)  (((a) > (b)) ? (a) : (b))

// recursive serialisation of files inside a directory
// Usage: serialise_binary <serialisation destination> <directory to search>
void printUsage(int argc, char **argv)
{
  fprintf(stderr, "Cmd:");
  for (int i = 0; i < argc; i++)
    fprintf(stderr, "%s ", argv[i]);

  fprintf(stderr, "\nUsage: %s /path/to/serialisation/folder /path/to/recursive/directory \n", argv[0]);
}

// every character needs a 1-to-1 replacement
void replaceCharacters(char *string, size_t size, const char *toReplace, const char *replacements)
{
  if (!string)
    return;

  for (size_t i = 0; i < size; ++i)
  {
    size_t j = 0;
    for (; toReplace[j] && string[i] != toReplace[j]; ++j) { }
    if (toReplace[j])
      string[i] = replacements[j];
  }
}

struct rawData
{
  char *data{};
  size_t size{};
  size_t capacity{};

  void append(const char *format, ...)
  {
    va_list args, copy;
    va_start(args, format);

    va_copy(copy, args);

    size_t newSize = stbsp_vsnprintf(nullptr, 0, format, copy);

    if (newSize >= capacity - size)
    {
      capacity = max(newSize * 2, capacity * 2);
      data = (char *)realloc(data, capacity);
    }

    va_end(copy);

    size += stbsp_vsnprintf(data + size, (int)(capacity - size), format, args);

    va_end(args);
  }
};

struct DestinationFiles
{
  rawData headerData{};
  rawData sourceData{};
};

void fileCallback(void* data, const xfiles_list_item_t* item)
{
    const char* name = item->path + item->name_idx;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      return;

    if (item->is_dir)
    {
      xfiles_list(item->path, data, fileCallback);
    }
    else
    {
      fprintf(stderr, "Entry: %s\n", item->path);

      unsigned char *fileData;
      size_t dataSize;
      xfiles_read(item->path, (void **)&fileData, &dataSize);

      size_t size = item->path_len - item->name_idx;
      char *string = (char *)malloc(1 + size);
      memcpy(string, item->path + item->name_idx, size);
      string[size] = '\0';
      replaceCharacters(string, size, "-.", "__");

      DestinationFiles *files = (DestinationFiles *)data;

      files->headerData.append("\n\textern const unsigned char %.*s[];\n\tconstexpr unsigned long long %.*sSize = %zu;\n",
        (int)size, string, (int)size, string, dataSize);

      files->sourceData.append("\nconst unsigned char %.*s[] = {\n", (int)size, string);

      const size_t COL_WIDTH = 32;
      for (size_t i = 0, column = 0; i < dataSize; (++i), (++column))
      {
        const char *format = "%hhu,";
        if (column >= COL_WIDTH)
        {
          format = "\n%hhu,";
          column = 0;
        }
        files->sourceData.append(format, fileData[i]);
      }

      files->sourceData.append("};\n\n");

      free(string);
      free(fileData);
    }
}

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    printUsage(argc, argv);
    return 1;
  }

  const char* outputPath = argv[1];
  const char* inputPath = argv[2];

  xfiles_create_directory_recursive(outputPath);

  char *headerPath, *sourcePath;
  {
    size_t destinationPathSize = strlen(outputPath);
    const char headerFileName[] = PATH_DELIMITER_STRING "BinaryData.h";
    const char sourceFileName[] = PATH_DELIMITER_STRING "BinaryData.cpp";

    headerPath = (char *)calloc(1 + destinationPathSize + sizeof(headerFileName), sizeof(char));
    memcpy(headerPath, outputPath, destinationPathSize);
    memcpy(headerPath + destinationPathSize, headerFileName, sizeof(headerFileName));


    sourcePath = (char *)calloc(1 + destinationPathSize + sizeof(sourceFileName), sizeof(char));
    memcpy(sourcePath, outputPath, destinationPathSize);
    memcpy(sourcePath + destinationPathSize, sourceFileName, sizeof(sourceFileName));

    fprintf(stderr, "Generated header: %s\n", headerPath);
    fprintf(stderr, "Generated source: %s\n", sourcePath);
  }

  DestinationFiles files{};

  files.headerData.append("// This file is auto-generated. Do not edit\n\n#pragma once\n\nnamespace BinaryData\n{");
  files.sourceData.append("// This file is auto-generated. Do not edit\n\n#include \"BinaryData.h\"\n\nnamespace BinaryData\n{");

  xfiles_list(inputPath, &files, fileCallback);

  files.headerData.append("}\n");
  files.sourceData.append("\n}\n");

  xfiles_write(headerPath, files.headerData.data, files.headerData.size);
  xfiles_write(sourcePath, files.sourceData.data, files.sourceData.size);

  return 0;
}
