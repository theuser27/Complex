/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   Icon_Pitch_svg;
    const int            Icon_Pitch_svgSize = 360;

    extern const char*   Icon_Filter_svg;
    const int            Icon_Filter_svgSize = 295;

    extern const char*   Icon_Dynamics_svg;
    const int            Icon_Dynamics_svgSize = 271;

    extern const char*   Icon_Phase_svg;
    const int            Icon_Phase_svgSize = 1106;

    extern const char*   DDINBold_ttf;
    const int            DDINBold_ttfSize = 52296;

    extern const char*   InterMedium_ttf;
    const int            InterMedium_ttfSize = 454712;

    extern const char*   Complex_skin;
    const int            Complex_skinSize = 4157;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 7;

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
