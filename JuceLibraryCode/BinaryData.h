/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   Icon_Contrast_svg;
    const int            Icon_Contrast_svgSize = 1337;

    extern const char*   DDINBold_ttf;
    const int            DDINBold_ttfSize = 52204;

    extern const char*   InterMedium_ttf;
    const int            InterMedium_ttfSize = 367772;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 3;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}