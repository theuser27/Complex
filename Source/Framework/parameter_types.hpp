
// Created: 2025-03-26 11:20:23

#pragma once

#include "constants.hpp"
#include "utils.hpp"
#include "memory.hpp"

namespace Plugin
{
  struct State;
}

namespace simd_values
{
  struct simd_int;
  struct simd_float;
}

namespace Generation
{
  class BaseProcessor;
}

namespace Framework
{
  class ParameterValue;

  // symmetric types apply the flipped curve to negative values
  // all x values are normalised

  COMPLEX_ENUM(ParameterScale,
    (            Toggle, 1757693064462006100),  // round(x)
    (           Indexed, 1757693075999062000),  // round(x * (max - min))
    (            Linear, 1757693088052707600),  // x * (max - min) + min
    (             Clamp, 1757693157266612600),  // clamp(x, min, max)
    (         Quadratic, 1757693166022104900),  // x ^ 2 * (max - min) + min
    (  ReverseQuadratic, 1757693171422009600),  // (x - 1) ^ 2 * sgn(x - 1) * (max - min) + max
    (SymmetricQuadratic, 1757693229769487000),  // ((x - 0.5) ^ 2 * sgn(x - 0.5) + 0.5) * 2 * (max - min) + min
    (             Cubic, 1757693243184020300),  // x ^ 3
    (      ReverseCubic, 1757693263245919300),  // (x - 1) ^ 3 * (max - min) + max
    (    SymmetricCubic, 1757693277746078900),  // (2 * x - 1) ^ 3
    (          Loudness, 1757693294934436100),  // 20 * log10(x)
    ( SymmetricLoudness, 1757693297826885200),  // 20 * log10(abs(x)) * sgn(x)
    (         Frequency, 1757693375693329300),  // (sampleRate / 2 * minFrequency) ^ x
    (SymmetricFrequency, 1757693385953525000)   // (sampleRate / 2 * minFrequency) ^ (abs(x)) * sgn(x)
  );

  COMPLEX_ENUM(ParameterChangeReason, 
    ( inputSidechain, 1757682036260828100),
    (outputSidechain, 1757682033430552800),
    (      laneCount, 1757682031418305900),
  );

  struct ProcessorMetadata;
  struct ParameterMetadata;

  struct IndexedData
  {
    enum { NoneFlag, VtableFlag, ProcessorFlag, ParameterFlag };

    utils::string_view displayName{};           // user-readable name for given parameter value
    uuid id{};                                  // uuid for parameter value
    u32 count = 1;                              // how many consecutive values are of this indexed type
                                                //   can be more than the ones currently available
    u32 flags{};
    uuid dynamicUpdateUuid{};                   // this uuid is used to register for in the State
                                                //   these updates will happen only if the parameter is not mapped/modulated
    IndexedData *parent{};
    IndexedData *children{};
    IndexedData *next{};
    union
    {
      utils::span<void(*const)()> vtable{};
      ProcessorMetadata *processorMetadata;
      ParameterMetadata *parameterMetadata;
    };

    operator IndexedData *() { return this; }
    IndexedData &operator,(IndexedData &other) 
    {
      if (next)
        *next, other;
      else
      {
        next = &other;
        other.parent = parent;
        if (parent)
          parent->count += other.count;
      }

      return *this;
    }

    template<typename ... Args>
    IndexedData &
    addChildren(Args &&... args)
    {
      IndexedData *childrenArgs[] = { (&args)... };
      childrenArgs[0]->parent = this;
      count += childrenArgs[0]->count;
      for (usize i = 1; i < sizeof...(Args); ++i)
      {
        childrenArgs[i - 1]->next = childrenArgs[i];
        childrenArgs[i]->parent = this;
        count += childrenArgs[i]->count;
      }
      children = childrenArgs[0];
      return *this;
    }

    static IndexedData *deepCopy(auto *arena, IndexedData *options)
    {
      auto *newOption = anew(arena, Framework::IndexedData, { *options });
      if (newOption->children)
      {
        newOption->children = deepCopy(arena, newOption->children);
        for (auto child = newOption->children; child->next; child = child->next)
          child->next = deepCopy(arena, child->next);
      }

      return newOption;
    }

    static void visit(IndexedData *data, const auto &predicate)
    {
      for (auto *child = data->children; child; child = child->next)
      {
        predicate(*child);
        visit(child, predicate);
      }
    }
  };

  struct ParameterDetails
  {
    enum Flags : u8
    { 
      None        = 0     ,
      Stereo      = 1 << 0,         // if parameter allows stereo modulation
      Modulatable = 1 << 1,         // if parameter allows modulation at all
      Automatable = 1 << 2,         // if parameter allows host automation
      Extensible  = 1 << 3,         // if parameter's minimum/maximum/default values can change
      All = Stereo | Modulatable |  
        Automatable | Extensible,
    };

    utils::string_view displayName{};                     // name displayed to the user
    uuid id{};												                    // parameter id
    union
    {
      struct
      {
        float minValue;                                   // minimum scaled value
        float maxValue;                                   // maximum scaled value
        float defaultValue;                               // default scaled value
        float defaultNormalisedValue;                     // default normalised value
      };
      struct
      {
        IndexedData *options;                             // (nested) list of options
        uuid defaultOptionId;                             // id of the default option
      };
    };

    ParameterScale::Value scale = ParameterScale::Linear; // value skew factor
    utils::string_view displayUnits{};                    // "%", " db", etc.
    u32 flags = Modulatable | Automatable;                // various flags
    UpdateFlag updateFlag = UpdateFlag::Realtime;         // at which point during processing the parameter is updated
    usize (*generateNumeric)(char *string,                // string generator function for IndexedNumeric parameters
      usize stringSize, double value,
      const ParameterDetails &details) = nullptr;
  };


  utils::pair<const IndexedData *, usize>
  getIndexedData(double scaledValue, const ParameterDetails &details) noexcept;
  double
  getValueFromOptionText(utils::string_view text, const ParameterDetails &details) noexcept;
  double
  getValueFromOptionId(uuid optionId, const ParameterDetails &details) noexcept;

  // with skewOnly == true a normalised value between [0,1] or [-0.5, 0.5] is returned,
  // depending on whether the parameter is bipolar
  double
  scaleValue(double value, const ParameterDetails &details, float sampleRate = kDefaultSampleRate,
    bool scalePercent = false, bool skewOnly = false) noexcept;

  double
  unscaleValue(double value, const ParameterDetails &details,
    float sampleRate = kDefaultSampleRate, bool unscalePercent = true) noexcept;

  simd_values::simd_float
  scaleValue(simd_values::simd_float value, const ParameterDetails &details,
    float sampleRate = kDefaultSampleRate) noexcept;



  struct ParameterMetadata
  {
    Framework::ParameterDetails details{};
    ParameterMetadata *next{};

    ParameterMetadata &operator,(ParameterMetadata &other)
    {
      if (next)
        *next, other;
      else
        next = &other;
      return *this;
    }
    operator ParameterMetadata *() { return this; }
  };

  struct ProcessorMetadata
  {
    enum : u32
    {
      NoTag = 0U,
      GroupTag = NoTag,
      ProcessorTag = 1U << 0,
      NoParameterValidationTag = 1U << 1,

      InactiveTag = 1U << 30,
      RuntimeAddedTag = 1U << 31,
    };

    u32 flags{};
    uuid id{};
    utils::string_view name{};
    Generation::BaseProcessor *(*create)(Plugin::State *, ProcessorMetadata *, const void *){};
    ProcessorMetadata *parent{};
    ProcessorMetadata *next{};
    ProcessorMetadata *children{};
    ParameterMetadata *parameters{};
    u32 childrenCount{};
    u32 parametersCount{};
    utils::span<void(*const)()> vtable{};

    ProcessorMetadata &operator,(ProcessorMetadata &other)
    {
      if (next)
        *next, other;
      else
        next = &other;
      return *this;
    }
    operator ProcessorMetadata *() { return this; }

    ProcessorMetadata &
    computeCounts()
    {
      for (auto child = children; child; child = child->next)
      {
        ++childrenCount;
        child->parent = this;
      }

      for (auto parameter = parameters; parameter; parameter = parameter->next)
        ++parametersCount;

      return *this;
    }

    ProcessorMetadata &
    computeParameterCounts(const auto &lambda)
    {
      for (auto child = children; child; child = child->next)
      {
        ++childrenCount;
        child->parent = this;
      }

      lambda(*this);

      for (auto parameter = parameters; parameter; parameter = parameter->next)
        ++parametersCount;

      return *this;
    }
  };

  struct PluginStructure
  {
    u64 versionNumber{};

    utils::bumpArena *arena{};

    Framework::ProcessorMetadata *metadata{};
    utils::vectornd<utils::sp<utils::Dylib>> loadedDynamicLibs{};

    utils::bumpArena *getNewArena(usize size)
    {
      return utils::bumpArena::createNested(arena, size);
    }
  };

  inline usize printToggleValues(char *string, usize size, double value, const ParameterDetails &)
  {
    static constexpr utils::string_view toggleStrings[] = { "Off", "On" };
    size = utils::min(toggleStrings[(usize)value].size(), size - 1);
    ::memcpy(string, toggleStrings[(usize)value].data(), size);
    string[size] = '\0';
    return size;
  }
}

#define COMPLEX_STRUCTURE_PARAMETER(...) (*anew(arena, Framework::ParameterMetadata, { .details = { __VA_ARGS__ } }))
#define COMPLEX_STRUCTURE_PARAMETER_CUSTOM(...) (__VA_ARGS__(*anew(arena, Framework::ParameterMetadata, {})))

#define COMPLEX_STRUCTURE_PROCESSOR(T, nameString, idNumber, ...) (*anew(arena, Framework::ProcessorMetadata, \
  { .flags = ProcessorMetadata::ProcessorTag, .id = idNumber, .name = { nameString, sizeof(nameString)  }, \
    .create = (Generation::BaseProcessor *(*)(Plugin::State *, ProcessorMetadata *, const void *))(::createProcessor<T>) __VA_OPT__(,) __VA_ARGS__ }))
#define COMPLEX_STRUCTURE_GROUP(nameString, idNumber, ...) (*anew(arena, Framework::ProcessorMetadata, \
  { .flags = ProcessorMetadata::GroupTag, .id = idNumber, .name = { nameString, sizeof(nameString) } __VA_OPT__(,) __VA_ARGS__ }))

// count == 0 if nothing is provided
#define COMPLEX_STRUCTURE_INDEXED_DATA(...) (*anew(arena, Framework::IndexedData, { COMPLEX_DEFAULT_OR(.count = 0, __VA_ARGS__) }))

template<typename T>
T *
createProcessor(Plugin::State *state, Framework::ProcessorMetadata *metadata, const void *copy = nullptr);
template<typename T>
void *
initialiseTypeStructure(void *metadata, Framework::PluginStructure &structure);
